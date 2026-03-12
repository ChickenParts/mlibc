/*
 * Yolk sysdeps for mlibc - Generic syscall implementations
 * SPDX-License-Identifier: MIT
 *
 * This file implements mlibc's syscall interface using Yolk syscalls.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>  /* For ENOSYS, EAGAIN, etc. */
#include <limits.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <termios.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <ucontext.h>
#include <sched.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <abi-bits/statfs.h>
#include <bits/winsize.h>  /* For struct winsize */

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif

/* epoll definitions for sysdeps (avoid header dependencies during bootstrap) */
struct epoll_event {
    uint32_t events;
    union {
        void *ptr;
        int fd;
        uint32_t u32;
        uint64_t u64;
    } data;
} __attribute__((packed));

#include <bits/ensure.h>
#include <mlibc/all-sysdeps.hpp>
#include <mlibc/debug.hpp>

#ifndef MLIBC_BUILDING_RTLD
#include <mlibc/tcb.hpp>
#endif

#include <yolk/syscall.h>

namespace mlibc {

static inline bool sc_failed(long result) {
	return result < 0;
}

static inline int sc_errno(long result) {
	return -result;
}

static inline bool sc_enosys(long result) {
	return result == -ENOSYS;
}

/* Userspace fallback umask state until native kernel support lands. */
static mode_t g_process_umask = 0022;
static int g_process_nice = 0;
static unsigned long g_memfd_seq = 0;
static unsigned long g_process_personality = 0;
static thread_local stack_t g_sigaltstack_state = {
	.ss_sp = nullptr,
	.ss_flags = SS_DISABLE,
	.ss_size = 0
};

struct itimer_state {
	struct itimerval programmed;
	struct timeval armed_at;
	bool armed;
};

static itimer_state g_itimer_state[3] = {};

static inline long long timeval_to_us(const struct timeval &tv) {
	return static_cast<long long>(tv.tv_sec) * 1000000LL + static_cast<long long>(tv.tv_usec);
}

static inline struct timeval us_to_timeval(long long us) {
	struct timeval tv {};
	if (us < 0) {
		us = 0;
	}
	tv.tv_sec = static_cast<time_t>(us / 1000000LL);
	tv.tv_usec = static_cast<suseconds_t>(us % 1000000LL);
	return tv;
}

static int get_now_timeval(struct timeval *tv) {
	if (!tv) {
		return EINVAL;
	}
	long result = __syscall2(SYS_gettimeofday_core, (long)tv, 0);
	return result < 0 ? -result : 0;
}

static bool valid_timeval(const struct timeval &tv) {
	return tv.tv_sec >= 0 && tv.tv_usec >= 0 && tv.tv_usec < 1000000;
}

static bool valid_timespec(const struct timespec &ts) {
	return ts.tv_sec >= 0 && ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000L;
}

static void compute_itimer_current(int which, const struct timeval &now, struct itimerval *out) {
	memset(out, 0, sizeof(*out));
	if (which < 0 || which >= 3) {
		return;
	}

	itimer_state &st = g_itimer_state[which];
	if (!st.armed) {
		out->it_interval = st.programmed.it_interval;
		out->it_value = {};
		return;
	}

	long long initial_us = timeval_to_us(st.programmed.it_value);
	long long interval_us = timeval_to_us(st.programmed.it_interval);
	long long elapsed_us = timeval_to_us(now) - timeval_to_us(st.armed_at);
	if (elapsed_us < 0) {
		elapsed_us = 0;
	}

	out->it_interval = st.programmed.it_interval;
	if (elapsed_us < initial_us) {
		out->it_value = us_to_timeval(initial_us - elapsed_us);
		return;
	}

	if (interval_us <= 0) {
		st.armed = false;
		out->it_value = {};
		return;
	}

	long long after_first = elapsed_us - initial_us;
	long long rem = interval_us - (after_first % interval_us);
	if (rem == interval_us) {
		rem = 0;
	}
	out->it_value = us_to_timeval(rem);
}


static void fill_statvfs_from_statfs(const struct statfs *in, struct statvfs *out) {
	if (!in || !out) {
		return;
	}

	memset(out, 0, sizeof(*out));
	out->f_bsize = in->f_bsize;
	out->f_frsize = in->f_frsize ? in->f_frsize : in->f_bsize;
	out->f_blocks = in->f_blocks;
	out->f_bfree = in->f_bfree;
	out->f_bavail = in->f_bavail;
	out->f_files = in->f_files;
	out->f_ffree = in->f_ffree;
	out->f_favail = in->f_ffree;
	out->f_fsid = (static_cast<unsigned long>(static_cast<unsigned int>(in->f_fsid.__val[1])) << 32)
		| static_cast<unsigned long>(static_cast<unsigned int>(in->f_fsid.__val[0]));
	out->f_flag = in->f_flags;
	out->f_namemax = in->f_namelen;
}

static int iov_total_length(const struct iovec *iov, size_t iovlen, size_t *total_out) {
	if (!iov && iovlen > 0) {
		return EINVAL;
	}
	if (!total_out) {
		return EINVAL;
	}
	size_t total = 0;
	for (size_t i = 0; i < iovlen; i++) {
		if (iov[i].iov_len > SIZE_MAX - total) {
			return EINVAL;
		}
		total += iov[i].iov_len;
	}
	*total_out = total;
	return 0;
}

static int iov_validate_bases(const struct iovec *iov, size_t iovlen) {
	if (!iov && iovlen > 0) {
		return EINVAL;
	}
	for (size_t i = 0; i < iovlen; i++) {
		if (iov[i].iov_len > 0 && !iov[i].iov_base) {
			return EFAULT;
		}
	}
	return 0;
}

static void iov_gather_bytes(void *dst, const struct iovec *iov, size_t iovlen) {
	uint8_t *out = reinterpret_cast<uint8_t *>(dst);
	for (size_t i = 0; i < iovlen; i++) {
		if (iov[i].iov_len == 0) {
			continue;
		}
		memcpy(out, iov[i].iov_base, iov[i].iov_len);
		out += iov[i].iov_len;
	}
}

static void iov_scatter_bytes(const void *src, size_t src_len, const struct iovec *iov, size_t iovlen) {
	const uint8_t *in = reinterpret_cast<const uint8_t *>(src);
	size_t remaining = src_len;
	for (size_t i = 0; i < iovlen && remaining > 0; i++) {
		size_t n = iov[i].iov_len;
		if (n > remaining) {
			n = remaining;
		}
		if (n > 0) {
			memcpy(iov[i].iov_base, in, n);
			in += n;
			remaining -= n;
		}
	}
}

#ifndef MLIBC_BUILDING_RTLD
static int resolve_dirfd_path(int dirfd, const char *path, char **resolved_path) {
	if (!path || !resolved_path) {
		return EINVAL;
	}

	*resolved_path = nullptr;
	if (path[0] == '\0' || path[0] == '/' || dirfd == AT_FDCWD) {
		char *copy = strdup(path);
		if (!copy) {
			return ENOMEM;
		}
		*resolved_path = copy;
		return 0;
	}

	int cwd_fd = -1;
	int e = sys_open(".", O_RDONLY | O_DIRECTORY, 0, &cwd_fd);
	if (e) {
		return e;
	}

	e = sys_fchdir(dirfd);
	if (e) {
		sys_close(cwd_fd);
		return e;
	}

	char cwd[PATH_MAX];
	e = sys_getcwd(cwd, sizeof(cwd));
	int restore_e = sys_fchdir(cwd_fd);
	int close_e = sys_close(cwd_fd);
	if (e) {
		return e;
	}
	if (restore_e) {
		return restore_e;
	}
	if (close_e) {
		return close_e;
	}

	size_t cwd_len = strlen(cwd);
	size_t path_len = strlen(path);
	bool add_slash = (cwd_len == 0 || cwd[cwd_len - 1] != '/');
	size_t total_len = cwd_len + (add_slash ? 1 : 0) + path_len + 1;

	char *joined = static_cast<char *>(malloc(total_len));
	if (!joined) {
		return ENOMEM;
	}

	memcpy(joined, cwd, cwd_len);
	size_t offset = cwd_len;
	if (add_slash) {
		joined[offset++] = '/';
	}
	memcpy(joined + offset, path, path_len);
	joined[offset + path_len] = '\0';

	*resolved_path = joined;
	return 0;
}
#endif

/* Forward declarations for local cross-calls. */
int sys_isatty(int fd);

/* =============================================================================
 * Core System Functions
 * =============================================================================
 */

void sys_libc_log(const char *message) {
    size_t len = strlen(message);
    __syscall3(SYS_write, 2, (long)message, len);  // stderr
}

[[noreturn]] void sys_libc_panic() {
    sys_libc_log("mlibc: panic!\n");
    __syscall1(SYS_exit, 127);
    __builtin_unreachable();
}

[[noreturn]] void sys_exit(int status) {
    __syscall1(SYS_exit_group_core, status);
    __builtin_unreachable();
}

/* =============================================================================
 * Thread Control Block (TCB) / Thread-Local Storage
 * =============================================================================
 */

#if defined(__x86_64__)
int sys_tcb_set(void *pointer) {
    constexpr long ARCH_SET_FS = 0x1002;
    long result = __syscall2(SYS_arch_prctl, ARCH_SET_FS, (long)pointer);
    if (result < 0 && result > -4096) {
        return -result;
    }
    return 0;
}
#elif defined(__aarch64__)
int sys_tcb_set(void *pointer) {
    /* ARM64: Use TPIDR_EL0 register */
    __asm__ volatile("msr tpidr_el0, %0" :: "r"(pointer) : "memory");
    return 0;
}
#elif defined(__riscv)
int sys_tcb_set(void *pointer) {
    /* RISC-V: Use tp register */
    __asm__ volatile("mv tp, %0" :: "r"(pointer) : "memory");
    return 0;
}
#else
#error "Unsupported architecture for TCB"
#endif

void *sys_tcb_get() {
    void *result;
#if defined(__x86_64__)
    __asm__ volatile("rdfsbase %0" : "=r"(result));
#elif defined(__aarch64__)
    __asm__ volatile("mrs %0, tpidr_el0" : "=r"(result));
#elif defined(__riscv)
    __asm__ volatile("mv %0, tp" : "=r"(result));
#endif
    return result;
}

/* =============================================================================
 * Memory Management
 * =============================================================================
 */

int sys_anon_allocate(size_t size, void **pointer) {
    long result = __syscall6(SYS_mmap, 0, size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result < 0 && result > -4096) {
        return -result;
    }
    *pointer = reinterpret_cast<void *>(result);
    return 0;
}

int sys_anon_free(void *pointer, size_t size) {
    long result = __syscall2(SYS_munmap, (long)pointer, size);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_vm_map(void *hint, size_t size, int prot, int flags, int fd, off_t offset,
               void **window) {
    long result = __syscall6(SYS_mmap, (long)hint, size, prot, flags, fd, offset);
    if (result < 0 && result > -4096) {
        return -result;
    }
    *window = reinterpret_cast<void *>(result);
    return 0;
}

int sys_vm_unmap(void *pointer, size_t size) {
    long result = __syscall2(SYS_munmap, (long)pointer, size);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_vm_protect(void *pointer, size_t size, int prot) {
    long result = __syscall3(SYS_mprotect, (long)pointer, size, prot);
    if (result < 0) {
        return -result;
    }
    return 0;
}

/* =============================================================================
 * File Operations
 * =============================================================================
 */

int sys_open(const char *path, int flags, mode_t mode, int *fd) {
    if (flags & O_CREAT) {
        mode &= ~g_process_umask;
    }
    long result = __syscall4(SYS_open_core, (long)path, flags, mode, AT_FDCWD);
    if (result < 0) {
        return -result;
    }
    *fd = result;
    return 0;
}

int sys_openat(int dirfd, const char *path, int flags, mode_t mode, int *fd) {
    /* Yolk's open takes dirfd as 4th arg */
    if (flags & O_CREAT) {
        mode &= ~g_process_umask;
    }
    long result = __syscall4(SYS_open_core, (long)path, flags, mode, dirfd);
    if (result < 0) {
        return -result;
    }
    *fd = result;
    return 0;
}

int sys_close(int fd) {
    long result = __syscall1(SYS_close_core, fd);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_read(int fd, void *buf, size_t count, ssize_t *bytes_read) {
    long result = __syscall3(SYS_read, fd, (long)buf, count);
    if (result < 0) {
        return -result;
    }
    *bytes_read = result;
    return 0;
}

int sys_readv(int fd, const struct iovec *iovs, int iovc, ssize_t *bytes_read) {
    if (iovc < 0 || (!iovs && iovc > 0) || !bytes_read) {
        return EINVAL;
    }

    ssize_t total = 0;
    for (int i = 0; i < iovc; i++) {
        if (!iovs[i].iov_base && iovs[i].iov_len) {
            return EINVAL;
        }
        if (!iovs[i].iov_len) {
            continue;
        }

        ssize_t got = 0;
        int e = sys_read(fd, iovs[i].iov_base, iovs[i].iov_len, &got);
        if (e) {
            if (total > 0) {
                *bytes_read = total;
                return 0;
            }
            return e;
        }

        total += got;
        if (static_cast<size_t>(got) < iovs[i].iov_len) {
            break;
        }
    }

    *bytes_read = total;
    return 0;
}

int sys_write(int fd, const void *buf, size_t count, ssize_t *bytes_written) {
    long result = __syscall3(SYS_write, fd, (long)buf, count);
    if (result < 0) {
        return -result;
    }
    *bytes_written = result;
    return 0;
}

int sys_writev(int fd, const struct iovec *iovs, int iovc, ssize_t *bytes_written) {
    if (iovc < 0 || (!iovs && iovc > 0) || !bytes_written) {
        return EINVAL;
    }

    ssize_t total = 0;
    for (int i = 0; i < iovc; i++) {
        if (!iovs[i].iov_base && iovs[i].iov_len) {
            return EINVAL;
        }
        if (!iovs[i].iov_len) {
            continue;
        }

        ssize_t wrote = 0;
        int e = sys_write(fd, iovs[i].iov_base, iovs[i].iov_len, &wrote);
        if (e) {
            if (total > 0) {
                *bytes_written = total;
                return 0;
            }
            return e;
        }

        total += wrote;
        if (static_cast<size_t>(wrote) < iovs[i].iov_len) {
            break;
        }
    }

    *bytes_written = total;
    return 0;
}

int sys_seek(int fd, off_t offset, int whence, off_t *new_offset) {
    long result = __syscall3(SYS_lseek, fd, offset, whence);
    if (result < 0) {
        return -result;
    }
    *new_offset = result;
    return 0;
}

int sys_pread(int fd, void *buf, size_t n, off_t off, ssize_t *bytes_read) {
    if (!bytes_read) {
        return EINVAL;
    }

    off_t original = 0;
    int e = sys_seek(fd, 0, SEEK_CUR, &original);
    if (e) {
        return e;
    }

    off_t ignored = 0;
    e = sys_seek(fd, off, SEEK_SET, &ignored);
    if (e) {
        return e;
    }

    int io_err = sys_read(fd, buf, n, bytes_read);
    int restore_err = sys_seek(fd, original, SEEK_SET, &ignored);

    if (io_err) {
        return io_err;
    }
    return restore_err;
}

int sys_pwrite(int fd, const void *buf, size_t n, off_t off, ssize_t *bytes_written) {
    if (!bytes_written) {
        return EINVAL;
    }

    off_t original = 0;
    int e = sys_seek(fd, 0, SEEK_CUR, &original);
    if (e) {
        return e;
    }

    off_t ignored = 0;
    e = sys_seek(fd, off, SEEK_SET, &ignored);
    if (e) {
        return e;
    }

    int io_err = sys_write(fd, buf, n, bytes_written);
    int restore_err = sys_seek(fd, original, SEEK_SET, &ignored);

    if (io_err) {
        return io_err;
    }
    return restore_err;
}

int sys_dup(int fd, int flags, int *newfd) {
    if (!newfd) {
        return EINVAL;
    }
    if (flags & ~O_CLOEXEC) {
        return EINVAL;
    }

    long result = __syscall1(SYS_dup, fd);
    if (result < 0) {
        return -result;
    }

    int duplicated = static_cast<int>(result);
    if (flags & O_CLOEXEC) {
        long cloexec_result = __syscall3(SYS_fcntl, duplicated, F_SETFD, FD_CLOEXEC);
        if (cloexec_result < 0) {
            __syscall1(SYS_close_core, duplicated);
            return -cloexec_result;
        }
    }

    *newfd = duplicated;
    return 0;
}

int sys_dup2(int fd, int flags, int newfd) {
    if (flags & ~O_CLOEXEC) {
        return EINVAL;
    }
    if ((flags & O_CLOEXEC) && fd == newfd) {
        /* Match dup3 semantics used by mlibc when flags are present. */
        return EINVAL;
    }

    long result = __syscall2(SYS_dup2, fd, newfd);
    if (result < 0) {
        return -result;
    }

    if (flags & O_CLOEXEC) {
        long cloexec_result = __syscall3(SYS_fcntl, newfd, F_SETFD, FD_CLOEXEC);
        if (cloexec_result < 0) {
            __syscall1(SYS_close_core, newfd);
            return -cloexec_result;
        }
    }

    return 0;
}

int sys_fcntl(int fd, int request, va_list args, int *result_value) {
    long arg = va_arg(args, long);
    long result = __syscall3(SYS_fcntl, fd, request, arg);
    if (result < 0) {
        return -result;
    }
    *result_value = result;
    return 0;
}

int sys_flock(int fd, int options) {
    struct flock fl {};
    if (options & LOCK_UN) {
        fl.l_type = F_UNLCK;
    } else if (options & LOCK_EX) {
        fl.l_type = F_WRLCK;
    } else if (options & LOCK_SH) {
        fl.l_type = F_RDLCK;
    } else {
        return EINVAL;
    }

    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = 0;

    int cmd = (options & LOCK_NB) ? F_SETLK : F_SETLKW;
    long result = __syscall3(SYS_fcntl, fd, cmd, (long)&fl);
    return result < 0 ? -result : 0;
}

int sys_stat(fsfd_target fsfdt, int fd, const char *path, int flags,
             struct stat *statbuf) {
    long result;
    if (fsfdt == fsfd_target::path) {
        if (flags & AT_SYMLINK_NOFOLLOW) {
            result = __syscall2(SYS_lstat, (long)path, (long)statbuf);
        } else {
            result = __syscall2(SYS_stat, (long)path, (long)statbuf);
        }
    } else if (fsfdt == fsfd_target::fd) {
        result = __syscall2(SYS_fstat, fd, (long)statbuf);
    } else {
        /* fsfd_target::fd_path - fstatat with fallback for AT_FDCWD */
        result = __syscall4(SYS_fstatat_core, fd, (long)path, (long)statbuf, flags);
        if (sc_enosys(result) && fd == AT_FDCWD) {
            if (flags & AT_SYMLINK_NOFOLLOW) {
                result = __syscall2(SYS_lstat, (long)path, (long)statbuf);
            } else {
                result = __syscall2(SYS_stat, (long)path, (long)statbuf);
            }
        }
    }
    return sc_failed(result) ? sc_errno(result) : 0;
}

int sys_statvfs(const char *path, struct statvfs *out) {
    if (!path || !out) {
        return EINVAL;
    }

    struct statfs sfs;
    long result = __syscall2(SYS_statfs, (long)path, (long)&sfs);
    if (sc_failed(result)) {
        return sc_errno(result);
    }

    fill_statvfs_from_statfs(&sfs, out);
    return 0;
}

int sys_fstatvfs(int fd, struct statvfs *out) {
    if (!out) {
        return EINVAL;
    }

    struct statfs sfs;
    long result = __syscall2(SYS_fstatfs_core, fd, (long)&sfs);
    if (sc_failed(result)) {
        return sc_errno(result);
    }

    fill_statvfs_from_statfs(&sfs, out);
    return 0;
}

} // namespace mlibc

extern "C" int statfs(const char *path, struct statfs *buf) {
	if (!path || !buf) {
		errno = EINVAL;
		return -1;
	}

	long result = __syscall2(SYS_statfs, (long)path, (long)buf);
	if (result < 0) {
		errno = -result;
		return -1;
	}
	return 0;
}

extern "C" int fstatfs(int fd, struct statfs *buf) {
	if (!buf) {
		errno = EINVAL;
		return -1;
	}

	long result = __syscall2(SYS_fstatfs_core, fd, (long)buf);
	if (result < 0) {
		errno = -result;
		return -1;
	}
	return 0;
}

#if defined(_LARGEFILE64_SOURCE)
extern "C" int fstatfs64(int fd, struct statfs64 *buf) {
	return fstatfs(fd, reinterpret_cast<struct statfs *>(buf));
}
#endif

namespace mlibc {

int sys_ftruncate(int fd, size_t size) {
    long result = __syscall2(SYS_ftruncate, fd, size);
    return result < 0 ? -result : 0;
}

int sys_truncate(const char *path, size_t size) {
    long result = __syscall2(SYS_truncate, (long)path, size);
    return result < 0 ? -result : 0;
}

int sys_fsync(int fd) {
    long result = __syscall1(SYS_fsync, fd);
    return result < 0 ? -result : 0;
}

int sys_fdatasync(int fd) {
    return sys_fsync(fd);
}

int sys_ioctl(int fd, unsigned long request, void *arg, int *result) {
    long ret = __syscall3(SYS_ioctl, fd, request, (long)arg);
    if (ret < 0) {
        return -ret;
    }
    if (result) {
        *result = ret;
    }
    return 0;
}

int sys_tcgetattr(int fd, struct termios *attr) {
    long result = __syscall3(SYS_ioctl, fd, TCGETS, (long)attr);
    return result < 0 ? -result : 0;
}

int sys_tcsetattr(int fd, int optional_action, const struct termios *attr) {
    int request = 0;
    switch (optional_action) {
        case TCSANOW:
            request = TCSETS;
            break;
        case TCSADRAIN:
            request = TCSETSW;
            break;
        case TCSAFLUSH:
            request = TCSETSF;
            break;
        default:
            return EINVAL;
    }

    long result = __syscall3(SYS_ioctl, fd, request, (long)attr);
    return result < 0 ? -result : 0;
}

int sys_tcflush(int fd, int queue) {
    long result = __syscall3(SYS_ioctl, fd, TCFLSH, queue);
    return result < 0 ? -result : 0;
}

int sys_tcdrain(int fd) {
    long result = __syscall3(SYS_ioctl, fd, TCSBRK, 1);
    return result < 0 ? -result : 0;
}

int sys_tcflow(int fd, int action) {
    long result = __syscall3(SYS_ioctl, fd, TCXONC, action);
    return result < 0 ? -result : 0;
}

int sys_ttyname(int fd, char *buf, size_t size) {
    if (!buf || size == 0) {
        return EINVAL;
    }

    int e = sys_isatty(fd);
    if (e) {
        return e;
    }

    static const char tty_path[] = "/dev/tty";
    if (sizeof(tty_path) > size) {
        return ERANGE;
    }

    memcpy(buf, tty_path, sizeof(tty_path));
    return 0;
}

int sys_isatty(int fd) {
    /* Check if fd is a tty by attempting TIOCGWINSZ */
    struct winsize ws;
    int result;
    if (sys_ioctl(fd, TIOCGWINSZ, &ws, &result) == 0) {
        return 0;  /* Success - is a tty */
    }
    return ENOTTY;  /* Not a tty */
}

/* =============================================================================
 * Directory Operations
 * =============================================================================
 */

int sys_mkdir(const char *path, mode_t mode) {
    mode &= ~g_process_umask;
    long result = __syscall2(SYS_mkdir, (long)path, mode);
    return result < 0 ? -result : 0;
}

#ifndef MLIBC_BUILDING_RTLD
int sys_mkdirat(int dirfd, const char *path, mode_t mode) {
    mode &= ~g_process_umask;
    long result = __syscall3(SYS_mkdirat_core, dirfd, (long)path, mode);
    if (sc_enosys(result)) {
        if (dirfd == AT_FDCWD) {
            result = __syscall2(SYS_mkdir, (long)path, mode);
        } else {
            char *resolved = nullptr;
            int e = resolve_dirfd_path(dirfd, path, &resolved);
            if (e) {
                return e;
            }
            result = __syscall2(SYS_mkdir, (long)resolved, mode);
            free(resolved);
        }
    }
    return sc_failed(result) ? sc_errno(result) : 0;
}

int sys_rmdir(const char *path) {
    long result = __syscall1(SYS_rmdir, (long)path);
    return result < 0 ? -result : 0;
}

int sys_getcwd(char *buffer, size_t size) {
    long result = __syscall2(SYS_getcwd, (long)buffer, size);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_chdir(const char *path) {
    long result = __syscall1(SYS_chdir, (long)path);
    return result < 0 ? -result : 0;
}

int sys_fchdir(int fd) {
    long result = __syscall1(SYS_fchdir, fd);
    return result < 0 ? -result : 0;
}

int sys_open_dir(const char *path, int *handle) {
    return sys_open(path, O_RDONLY | O_DIRECTORY, 0, handle);
}

int sys_read_entries(int handle, void *buffer, size_t max_size, size_t *bytes_read) {
    if (!bytes_read) {
        return EINVAL;
    }

    long result = __syscall3(SYS_getdents, handle, (long)buffer, max_size);
    if (sc_failed(result)) {
        return sc_errno(result);
    }

    if (static_cast<size_t>(result) > max_size) {
        return EIO;
    }
    *bytes_read = static_cast<size_t>(result);
    return 0;
}

int sys_readdir(int fd, struct dirent *entry, size_t max_size) {
	if (!entry || max_size < sizeof(struct dirent)) {
		return EINVAL;
	}

	void *buf = malloc(max_size);
	if (!buf) {
		return ENOMEM;
	}

	size_t bytes_read = 0;
	int e = sys_read_entries(fd, buf, max_size, &bytes_read);
	if (e) {
		free(buf);
		return e;
	}
	if (bytes_read == 0) {
		free(buf);
		return ENOENT;
	}

	struct dirent *first = static_cast<struct dirent *>(buf);
	size_t reclen = first->d_reclen;
	if (reclen == 0 || reclen > bytes_read) {
		free(buf);
		return EIO;
	}
	if (reclen > max_size) {
		reclen = max_size;
	}

	memset(entry, 0, max_size);
	memcpy(entry, first, reclen);
	free(buf);
	return 0;
}

/* =============================================================================
 * Link Operations
 * =============================================================================
 */

int sys_link(const char *old_path, const char *new_path) {
    long result = __syscall2(SYS_link, (long)old_path, (long)new_path);
    return result < 0 ? -result : 0;
}

int sys_linkat(int olddirfd, const char *old_path, int newdirfd, const char *new_path,
               int flags) {
    if (flags != 0) {
        return EINVAL;
    }

	char *old_resolved = nullptr;
	int e = resolve_dirfd_path(olddirfd, old_path, &old_resolved);
	if (e) {
		return e;
	}

	char *new_resolved = nullptr;
	e = resolve_dirfd_path(newdirfd, new_path, &new_resolved);
	if (e) {
		free(old_resolved);
		return e;
	}

	int link_e = sys_link(old_resolved, new_resolved);
	free(old_resolved);
	free(new_resolved);
	return link_e;
}

int sys_unlink(const char *path) {
    long result = __syscall1(SYS_unlink, (long)path);
    return result < 0 ? -result : 0;
}

int sys_unlinkat(int dirfd, const char *path, int flags) {
    long result = __syscall3(SYS_unlinkat_core, dirfd, (long)path, flags);
    if (sc_enosys(result) && dirfd == AT_FDCWD) {
        if (flags & AT_REMOVEDIR) {
            result = __syscall1(SYS_rmdir, (long)path);
        } else if (flags == 0) {
            result = __syscall1(SYS_unlink, (long)path);
        } else {
            return EINVAL;
        }
    }
    return sc_failed(result) ? sc_errno(result) : 0;
}

int sys_symlink(const char *target_path, const char *link_path) {
    long result = __syscall2(SYS_symlink, (long)target_path, (long)link_path);
    return result < 0 ? -result : 0;
}

int sys_symlinkat(const char *target_path, int dirfd, const char *link_path) {
	char *resolved = nullptr;
	int e = resolve_dirfd_path(dirfd, link_path, &resolved);
	if (e) {
		return e;
	}
	int symlink_e = sys_symlink(target_path, resolved);
	free(resolved);
	return symlink_e;
}

int sys_readlink(const char *path, void *buffer, size_t max_size, ssize_t *length) {
    long result = __syscall3(SYS_readlink, (long)path, (long)buffer, max_size);
    if (result < 0) {
        return -result;
    }
    *length = result;
    return 0;
}

int sys_readlinkat(int dirfd, const char *path, void *buffer, size_t max_size, ssize_t *length) {
	char *resolved = nullptr;
	int e = resolve_dirfd_path(dirfd, path, &resolved);
	if (e) {
		return e;
	}
	int readlink_e = sys_readlink(resolved, buffer, max_size, length);
	free(resolved);
	return readlink_e;
}

int sys_rename(const char *old_path, const char *new_path) {
    long result = __syscall2(SYS_rename, (long)old_path, (long)new_path);
    return result < 0 ? -result : 0;
}

int sys_renameat(int old_dirfd, const char *old_path, int new_dirfd,
                 const char *new_path) {
    long result = __syscall4(SYS_renameat_core, old_dirfd, (long)old_path, new_dirfd, (long)new_path);
    if (sc_enosys(result) && old_dirfd == AT_FDCWD && new_dirfd == AT_FDCWD) {
        result = __syscall2(SYS_rename, (long)old_path, (long)new_path);
    }
    return sc_failed(result) ? sc_errno(result) : 0;
}

/* =============================================================================
 * Permission Operations
 * =============================================================================
 */

int sys_chmod(const char *path, mode_t mode) {
    long result = __syscall2(SYS_chmod, (long)path, mode);
    return result < 0 ? -result : 0;
}

int sys_fchmod(int fd, mode_t mode) {
    long result = __syscall2(SYS_fchmod, fd, mode);
    return result < 0 ? -result : 0;
}

int sys_fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
    long result = __syscall4(SYS_fchmodat_core, dirfd, (long)path, mode, flags);
    if (sc_enosys(result)) {
        if (flags != 0) {
            return EINVAL;
        }
		char *resolved = nullptr;
		int e = resolve_dirfd_path(dirfd, path, &resolved);
		if (e) {
			return e;
		}
        result = __syscall2(SYS_chmod, (long)resolved, mode);
		free(resolved);
    }
    return sc_failed(result) ? sc_errno(result) : 0;
}

int sys_chown(const char *path, uid_t uid, gid_t gid) {
    long result = __syscall3(SYS_chown, (long)path, uid, gid);
    return result < 0 ? -result : 0;
}

int sys_fchown(int fd, uid_t uid, gid_t gid) {
    long result = __syscall3(SYS_fchown, fd, uid, gid);
    return result < 0 ? -result : 0;
}

int sys_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) {
    if ((flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW)) != 0) {
        return EINVAL;
    }

    if ((flags & AT_EMPTY_PATH) && pathname && pathname[0] == '\0') {
        return sys_fchown(dirfd, owner, group);
    }

	if (flags & AT_SYMLINK_NOFOLLOW) {
		return EOPNOTSUPP;
	}

	char *resolved = nullptr;
	int e = resolve_dirfd_path(dirfd, pathname, &resolved);
	if (e) {
		return e;
	}
	int chown_e = sys_chown(resolved, owner, group);
	free(resolved);
	return chown_e;
}

int sys_access(const char *path, int mode) {
    long result = __syscall2(SYS_access, (long)path, mode);
    return result < 0 ? -result : 0;
}

int sys_faccessat(int dirfd, const char *pathname, int mode, int flags) {
    long result = __syscall4(SYS_faccessat_core, dirfd, (long)pathname, mode, flags);
    if (sc_enosys(result)) {
        if (flags != 0) {
            return EINVAL;
        }
		char *resolved = nullptr;
		int e = resolve_dirfd_path(dirfd, pathname, &resolved);
		if (e) {
			return e;
		}
        result = __syscall2(SYS_access, (long)resolved, mode);
		free(resolved);
    }
    return sc_failed(result) ? sc_errno(result) : 0;
}

int sys_utimensat(int dirfd, const char *pathname, const struct timespec times[2],
                  int flags) {
    long result = __syscall4(SYS_utimensat_core, dirfd, (long)pathname, (long)times, flags);
    if (sc_enosys(result) && pathname && flags == 0) {
		char *resolved = nullptr;
		int e = resolve_dirfd_path(dirfd, pathname, &resolved);
		if (e) {
			return e;
		}

		struct timeval tv[2];
		struct timeval *tv_ptr = nullptr;
		if (times) {
			tv[0].tv_sec = times[0].tv_sec;
			tv[0].tv_usec = times[0].tv_nsec / 1000;
			tv[1].tv_sec = times[1].tv_sec;
			tv[1].tv_usec = times[1].tv_nsec / 1000;
			tv_ptr = tv;
		}
        result = __syscall2(SYS_utimes, (long)resolved, (long)tv_ptr);
		free(resolved);
    }
    return sc_failed(result) ? sc_errno(result) : 0;
}
#endif

/* =============================================================================
 * Process Control
 * =============================================================================
 */

pid_t sys_getpid() {
    return __syscall0(SYS_getpid_core);
}

pid_t sys_getppid() {
    return __syscall0(SYS_getppid_core);
}

uid_t sys_getuid() {
    return __syscall0(SYS_getuid_core);
}

uid_t sys_geteuid() {
    return __syscall0(SYS_geteuid_core);
}

gid_t sys_getgid() {
    return __syscall0(SYS_getgid_core);
}

gid_t sys_getegid() {
    return __syscall0(SYS_getegid_core);
}

pid_t sys_gettid() {
    return __syscall0(SYS_gettid_core);
}

int sys_setuid(uid_t uid) {
    uid_t cur = sys_getuid();
    return (uid == cur) ? 0 : EPERM;
}

int sys_seteuid(uid_t euid) {
    uid_t cur = sys_geteuid();
    return (euid == cur) ? 0 : EPERM;
}

int sys_setgid(gid_t gid) {
    gid_t cur = sys_getgid();
    return (gid == cur) ? 0 : EPERM;
}

int sys_setegid(gid_t egid) {
    gid_t cur = sys_getegid();
    return (egid == cur) ? 0 : EPERM;
}

int sys_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid) {
    if (!ruid || !euid || !suid) {
        return EINVAL;
    }
    uid_t uid = sys_getuid();
    uid_t euid_cur = sys_geteuid();
    *ruid = uid;
    *euid = euid_cur;
    *suid = euid_cur;
    return 0;
}

int sys_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid) {
    if (!rgid || !egid || !sgid) {
        return EINVAL;
    }
    gid_t gid = sys_getgid();
    gid_t egid_cur = sys_getegid();
    *rgid = gid;
    *egid = egid_cur;
    *sgid = egid_cur;
    return 0;
}

int sys_setreuid(uid_t ruid, uid_t euid) {
    uid_t cur_r = sys_getuid();
    uid_t cur_e = sys_geteuid();
    if ((ruid != (uid_t)-1 && ruid != cur_r) ||
        (euid != (uid_t)-1 && euid != cur_e)) {
        return EPERM;
    }
    return 0;
}

int sys_setregid(gid_t rgid, gid_t egid) {
    gid_t cur_r = sys_getgid();
    gid_t cur_e = sys_getegid();
    if ((rgid != (gid_t)-1 && rgid != cur_r) ||
        (egid != (gid_t)-1 && egid != cur_e)) {
        return EPERM;
    }
    return 0;
}

int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid) {
    uid_t cur_r = sys_getuid();
    uid_t cur_e = sys_geteuid();
    if ((ruid != (uid_t)-1 && ruid != cur_r) ||
        (euid != (uid_t)-1 && euid != cur_e) ||
        (suid != (uid_t)-1 && suid != cur_e)) {
        return EPERM;
    }
    return 0;
}

int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
    gid_t cur_r = sys_getgid();
    gid_t cur_e = sys_getegid();
    if ((rgid != (gid_t)-1 && rgid != cur_r) ||
        (egid != (gid_t)-1 && egid != cur_e) ||
        (sgid != (gid_t)-1 && sgid != cur_e)) {
        return EPERM;
    }
    return 0;
}

int sys_getgroups(size_t size, gid_t *list, int *ret) {
    if (!ret) {
        return EINVAL;
    }
    gid_t gid = sys_getgid();
    if (size == 0) {
        *ret = 1;
        return 0;
    }
    if (!list) {
        return EINVAL;
    }
    list[0] = gid;
    *ret = 1;
    return 0;
}

int sys_setgroups(size_t size, const gid_t *list) {
    if (size == 0) {
        return 0;
    }
    if (!list) {
        return EINVAL;
    }
    gid_t cur = sys_getgid();
    if (size == 1 && list[0] == cur) {
        return 0;
    }
    return EPERM;
}

int sys_getpriority(int which, id_t who, int *value) {
    if (!value) {
        return EINVAL;
    }
    pid_t pgrp = 0;
    if (which == PRIO_PGRP) {
        int e = sys_getpgid(0, &pgrp);
        if (e) {
            return e;
        }
    }
    switch (which) {
    case PRIO_PROCESS:
        if (who != 0 && (pid_t)who != sys_getpid()) {
            return ESRCH;
        }
        break;
    case PRIO_PGRP:
        if (who != 0 && (pid_t)who != pgrp) {
            return ESRCH;
        }
        break;
    case PRIO_USER:
        if (who != 0 && (uid_t)who != sys_getuid()) {
            return ESRCH;
        }
        break;
    default:
        return EINVAL;
    }
    *value = g_process_nice;
    return 0;
}

int sys_setpriority(int which, id_t who, int prio) {
    pid_t pgrp = 0;
    if (which == PRIO_PGRP) {
        int e = sys_getpgid(0, &pgrp);
        if (e) {
            return e;
        }
    }
    switch (which) {
    case PRIO_PROCESS:
        if (who != 0 && (pid_t)who != sys_getpid()) {
            return ESRCH;
        }
        break;
    case PRIO_PGRP:
        if (who != 0 && (pid_t)who != pgrp) {
            return ESRCH;
        }
        break;
    case PRIO_USER:
        if (who != 0 && (uid_t)who != sys_getuid()) {
            return ESRCH;
        }
        break;
    default:
        return EINVAL;
    }
    if (prio < PRIO_MIN) prio = PRIO_MIN;
    if (prio > PRIO_MAX) prio = PRIO_MAX;
    g_process_nice = prio;
    return 0;
}

int sys_nice(int nice, int *new_nice) {
    if (!new_nice) {
        return EINVAL;
    }
    int prio = g_process_nice + nice;
    if (prio < PRIO_MIN) prio = PRIO_MIN;
    if (prio > PRIO_MAX) prio = PRIO_MAX;
    g_process_nice = prio;
    *new_nice = g_process_nice;
    return 0;
}

int sys_setpgid(pid_t pid, pid_t pgid) {
    long result = __syscall2(SYS_setpgid_core, pid, pgid);
    return result < 0 ? -result : 0;
}

int sys_getpgid(pid_t pid, pid_t *pgid) {
    long result = __syscall1(SYS_getpgid_core, pid);
    if (result < 0) {
        return -result;
    }
    *pgid = result;
    return 0;
}

int sys_setsid(pid_t *sid) {
    long result = __syscall0(SYS_setsid_core);
    if (result < 0) {
        return -result;
    }
    *sid = result;
    return 0;
}

int sys_getsid(pid_t pid, pid_t *sid) {
    long result = __syscall1(SYS_getsid_core, pid);
    if (result < 0) {
        return -result;
    }
    *sid = result;
    return 0;
}

int sys_fork(pid_t *child) {
    long result = __syscall0(SYS_fork_core);
    if (result < 0) {
        return -result;
    }
    *child = result;
    return 0;
}

int sys_execve(const char *path, char *const argv[], char *const envp[]) {
    long result = __syscall3(SYS_execve_core, (long)path, (long)argv, (long)envp);
    /* execve only returns on error */
    return -result;
}

int sys_waitpid(pid_t pid, int *status, int flags, struct rusage *ru, pid_t *ret_pid) {
    long result = __syscall4(SYS_wait4_core, pid, (long)status, flags, (long)ru);
    if (result < 0) {
        return -result;
    }
    *ret_pid = result;
    return 0;
}

/* =============================================================================
 * Signals
 * =============================================================================
 */

int sys_kill(pid_t pid, int sig) {
    long result = __syscall2(SYS_kill_core, pid, sig);
    return result < 0 ? -result : 0;
}

int sys_tgkill(pid_t tgid, pid_t tid, int sig) {
    long result = __syscall3(SYS_tgkill_core, tgid, tid, sig);
    return result < 0 ? -result : 0;
}

/* Signal restorer trampolines (defined in x86_64/signals.S) */
#ifndef MLIBC_BUILDING_RTLD
extern "C" void __mlibc_signal_restore(void);
extern "C" void __mlibc_signal_restore_rt(void);
#endif

#ifndef SA_RESTORER
#define SA_RESTORER 0x04000000
#endif

int sys_sigaction(int signum, const struct sigaction *act,
                  struct sigaction *oldact) {
#ifdef MLIBC_BUILDING_RTLD
    long result = __syscall3(SYS_sigaction_core, signum, (long)act, (long)oldact);
    return result < 0 ? -result : 0;
#else
    /* If setting a new action, install our signal restorer trampoline */
    if (act && (act->sa_flags & SA_RESTORER) == 0) {
        struct sigaction modified_act = *act;
        modified_act.sa_flags |= SA_RESTORER;
        /* Use RT restorer for SA_SIGINFO handlers, regular for others */
        modified_act.sa_restorer = (act->sa_flags & SA_SIGINFO)
            ? __mlibc_signal_restore_rt
            : __mlibc_signal_restore;
        long result = __syscall3(SYS_sigaction_core, signum, (long)&modified_act, (long)oldact);
        return result < 0 ? -result : 0;
    }
    long result = __syscall3(SYS_sigaction_core, signum, (long)act, (long)oldact);
    return result < 0 ? -result : 0;
#endif
}

int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    long result = __syscall3(SYS_sigprocmask_core, how, (long)set, (long)oldset);
    return result < 0 ? -result : 0;
}

int sys_thread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
    return sys_sigprocmask(how, set, oldset);
}

int sys_sigpending(sigset_t *set) {
    long result = __syscall1(SYS_sigpending_core, (long)set);
    return result < 0 ? -result : 0;
}

int sys_sigsuspend(const sigset_t *mask) {
    long result = __syscall1(SYS_sigsuspend_core, (long)mask);
    return result < 0 ? -result : 0;
}

void sys_sigreturn() {
    __syscall0(SYS_sigreturn_core);
    __builtin_unreachable();
}

/* =============================================================================
 * Time Operations
 * =============================================================================
 */

int sys_clock_get(int clock, time_t *secs, long *nanos) {
    struct timespec ts;
    long result = __syscall2(SYS_clock_gettime_core, clock, (long)&ts);
    if (result < 0) {
        return -result;
    }
    *secs = ts.tv_sec;
    *nanos = ts.tv_nsec;
    return 0;
}

int sys_clock_getres(int clock, time_t *secs, long *nanos) {
    struct timespec ts;
    long result = __syscall2(SYS_clock_getres_core, clock, (long)&ts);
    if (result < 0) {
        return -result;
    }
    *secs = ts.tv_sec;
    *nanos = ts.tv_nsec;
    return 0;
}

int sys_sleep(time_t *secs, long *nanos) {
    struct timespec req = {*secs, *nanos};
    struct timespec rem = {0, 0};
    long result = __syscall2(SYS_nanosleep_core, (long)&req, (long)&rem);
    if (result < 0) {
        *secs = rem.tv_sec;
        *nanos = rem.tv_nsec;
        return -result;
    }
    *secs = 0;
    *nanos = 0;
    return 0;
}

int sys_gettimeofday(struct timeval *tv) {
    long result = __syscall2(SYS_gettimeofday_core, (long)tv, 0);
    return result < 0 ? -result : 0;
}

/* =============================================================================
 * Futex Operations
 * =============================================================================
 */

int sys_futex_wait(int *pointer, int expected, const struct timespec *time) {
    long result = __syscall4(SYS_futex_wait, (long)pointer, expected,
                             (long)time, 0);
    if (result < 0) {
        if (result == -EAGAIN || result == -EINTR) {
            return -result;
        }
        return -result;
    }
    return 0;
}

int sys_futex_wake(int *pointer) {
    long result = __syscall2(SYS_futex_wake, (long)pointer, INT_MAX);
    if (result < 0) {
        return -result;
    }
    return 0;
}

/* =============================================================================
 * Resource Limits (stubs for now)
 * =============================================================================
 */

static bool g_rlimits_initialized = false;
static struct rlimit g_rlimits[RLIMIT_NLIMITS];

static void ensure_rlimits_initialized() {
    if (g_rlimits_initialized) {
        return;
    }

    for (int i = 0; i < RLIMIT_NLIMITS; i++) {
        g_rlimits[i].rlim_cur = RLIM_INFINITY;
        g_rlimits[i].rlim_max = RLIM_INFINITY;
    }

    g_rlimits_initialized = true;
}

int sys_getrlimit(int resource, struct rlimit *rlim) {
    if (!rlim) {
        return EINVAL;
    }
    if (resource < 0 || resource >= RLIMIT_NLIMITS) {
        return EINVAL;
    }

    ensure_rlimits_initialized();
    *rlim = g_rlimits[resource];
    return 0;
}

int sys_setrlimit(int resource, const struct rlimit *rlim) {
    if (!rlim) {
        return EINVAL;
    }
    if (resource < 0 || resource >= RLIMIT_NLIMITS) {
        return EINVAL;
    }
    if (rlim->rlim_cur > rlim->rlim_max) {
        return EINVAL;
    }

    ensure_rlimits_initialized();
    g_rlimits[resource] = *rlim;
    return 0;
}

int sys_getrusage(int who, struct rusage *usage) {
    if (!usage) {
        return EINVAL;
    }
    if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN) {
        return EINVAL;
    }

    memset(usage, 0, sizeof(*usage));

    /* Best-effort userspace fallback: expose coarse wall-time for SELF. */
    if (who == RUSAGE_SELF) {
        struct timeval tv {};
        int e = sys_gettimeofday(&tv);
        if (!e) {
            usage->ru_utime = tv;
        }
    }

    return 0;
}

/* =============================================================================
 * Pipe (placeholder - needs Yolk pipe support)
 * =============================================================================
 */

int sys_pipe(int *fds, int flags) {
    long result = __syscall2(SYS_pipe2, (long)fds, flags);
    return result < 0 ? -result : 0;
}

/* =============================================================================
 * Socket Operations
 * =============================================================================
 */

int sys_socket(int domain, int type, int protocol, int *fd) {
    long result = __syscall3(SYS_socket_core, domain, type, protocol);
    if (result < 0) {
        return -result;
    }
    *fd = result;
    return 0;
}

int sys_socketpair(int domain, int type_and_flags, int proto, int *fds) {
    long result = __syscall4(SYS_socketpair_core, domain, type_and_flags, proto, (long)fds);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    long result = __syscall3(SYS_bind_core, fd, (long)addr, addrlen);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_listen(int fd, int backlog) {
    long result = __syscall2(SYS_listen_core, fd, backlog);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_accept(int fd, int *newfd, struct sockaddr *addr, socklen_t *addrlen,
               int flags) {
    if (!newfd) {
        return EINVAL;
    }

    int unsupported = flags & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (unsupported) {
        return EINVAL;
    }

    long result;
    if (flags) {
        result = __syscall4(SYS_accept4_core, fd, (long)addr, (long)addrlen, flags);
        if (sc_enosys(result)) {
            result = __syscall3(SYS_accept_core, fd, (long)addr, (long)addrlen);
            if (sc_failed(result)) {
                return sc_errno(result);
            }

            int accepted_fd = result;
            if (flags & SOCK_NONBLOCK) {
                long fl = __syscall3(SYS_fcntl, accepted_fd, F_GETFL, 0);
                if (sc_failed(fl)) {
                    __syscall1(SYS_close_core, accepted_fd);
                    return sc_errno(fl);
                }
                long rc = __syscall3(SYS_fcntl, accepted_fd, F_SETFL, fl | O_NONBLOCK);
                if (sc_failed(rc)) {
                    __syscall1(SYS_close_core, accepted_fd);
                    return sc_errno(rc);
                }
            }

            if (flags & SOCK_CLOEXEC) {
                long rc = __syscall3(SYS_fcntl, accepted_fd, F_SETFD, FD_CLOEXEC);
                if (sc_failed(rc)) {
                    __syscall1(SYS_close_core, accepted_fd);
                    return sc_errno(rc);
                }
            }
        }
    } else {
        result = __syscall3(SYS_accept_core, fd, (long)addr, (long)addrlen);
    }
    if (result < 0) {
        return -result;
    }
    *newfd = result;
    return 0;
}

int sys_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    long result = __syscall3(SYS_connect_core, fd, (long)addr, addrlen);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_msg_send(int fd, const struct msghdr *hdr, int flags, ssize_t *length) {
    if (!length) {
        return EINVAL;
    }

#ifdef MLIBC_BUILDING_RTLD
    long rtld_result = __syscall3(SYS_sendmsg_core, fd, (long)hdr, flags);
    if (sc_failed(rtld_result)) {
        return sc_errno(rtld_result);
    }
    *length = rtld_result;
    return 0;
#else
    if (!hdr) {
        long result = __syscall6(SYS_sendto_core, fd, 0, 0, flags, 0, 0);
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        *length = result;
        return 0;
    }

    struct msghdr normalized = *hdr;
    if (normalized.msg_namelen && !normalized.msg_name) {
        return EINVAL;
    }
    if (normalized.msg_controllen && !normalized.msg_control) {
        return EFAULT;
    }
    if (normalized.msg_iovlen && !normalized.msg_iov) {
        return EINVAL;
    }
    if (normalized.msg_controllen == 0) {
        normalized.msg_control = nullptr;
    }

    long result = __syscall3(SYS_sendmsg_core, fd, (long)&normalized, flags);
    if (!sc_enosys(result)) {
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        *length = result;
        return 0;
    }

    if (normalized.msg_controllen != 0) {
        return EOPNOTSUPP;
    }

    if (!hdr->msg_iov || hdr->msg_iovlen == 0) {
        result = __syscall6(SYS_sendto_core, fd, 0, 0, flags,
                            (long)normalized.msg_name, (long)normalized.msg_namelen);
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        *length = result;
        return 0;
    }

    if (hdr->msg_iovlen > 1) {
        int e = iov_validate_bases(hdr->msg_iov, hdr->msg_iovlen);
        if (e) {
            return e;
        }
        size_t total = 0;
        e = iov_total_length(hdr->msg_iov, hdr->msg_iovlen, &total);
        if (e) {
            return e;
        }

        void *tmp = nullptr;
        if (total > 0) {
            tmp = malloc(total);
            if (!tmp) {
                return ENOMEM;
            }
            iov_gather_bytes(tmp, hdr->msg_iov, hdr->msg_iovlen);
        }

        result = __syscall6(SYS_sendto_core, fd, (long)tmp, total, flags,
                            (long)normalized.msg_name, normalized.msg_namelen);
        if (tmp) {
            free(tmp);
        }
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        if (static_cast<size_t>(result) > total) {
            return EIO;
        }
        *length = result;
        return 0;
    }

    if (!hdr->msg_iov || hdr->msg_iovlen != 1) {
        return EINVAL;
    }
    if (hdr->msg_iov[0].iov_len > 0 && !hdr->msg_iov[0].iov_base) {
        return EFAULT;
    }

    const struct iovec *iov = hdr->msg_iov;
    result = __syscall6(SYS_sendto_core, fd, (long)iov[0].iov_base, iov[0].iov_len,
                        flags, (long)normalized.msg_name, normalized.msg_namelen);
    if (sc_failed(result)) {
        return sc_errno(result);
    }
    *length = result;
    return 0;
#endif
}

int sys_msg_recv(int fd, struct msghdr *hdr, int flags, ssize_t *length) {
    if (!length) {
        return EINVAL;
    }

#ifdef MLIBC_BUILDING_RTLD
    long rtld_result = __syscall3(SYS_recvmsg_core, fd, (long)hdr, flags);
    if (sc_failed(rtld_result)) {
        return sc_errno(rtld_result);
    }
    *length = rtld_result;
    return 0;
#else
    if (!hdr) {
        socklen_t addrlen = 0;
        long result = __syscall6(SYS_recvfrom_core, fd, 0, 0, flags, 0, (long)&addrlen);
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        *length = result;
        return 0;
    }

    struct msghdr normalized = *hdr;
    if (normalized.msg_namelen && !normalized.msg_name) {
        return EINVAL;
    }
    if (normalized.msg_controllen && !normalized.msg_control) {
        return EFAULT;
    }
    if (normalized.msg_iovlen && !normalized.msg_iov) {
        return EINVAL;
    }
    if (normalized.msg_controllen == 0) {
        normalized.msg_control = nullptr;
    }

    long result = __syscall3(SYS_recvmsg_core, fd, (long)&normalized, flags);
    if (!sc_enosys(result)) {
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        hdr->msg_namelen = normalized.msg_namelen;
        hdr->msg_controllen = normalized.msg_controllen;
        hdr->msg_flags = normalized.msg_flags;
        *length = result;
        return 0;
    }

    /* Recv fallback can still provide payload via recvfrom(2) even when
     * ancillary control data is requested; control metadata is dropped. */
    bool control_requested = normalized.msg_controllen != 0;

    if (!hdr->msg_iov || hdr->msg_iovlen == 0) {
        socklen_t addrlen = normalized.msg_namelen;
        result = __syscall6(SYS_recvfrom_core, fd, 0, 0, flags,
                            (long)normalized.msg_name, (long)&addrlen);
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        hdr->msg_namelen = addrlen;
        hdr->msg_controllen = 0;
        hdr->msg_flags = 0;
        if (control_requested) {
            hdr->msg_flags |= MSG_CTRUNC;
        }
        *length = result;
        return 0;
    }

    if (hdr->msg_iovlen > 1) {
        int e = iov_validate_bases(hdr->msg_iov, hdr->msg_iovlen);
        if (e) {
            return e;
        }
        size_t total = 0;
        e = iov_total_length(hdr->msg_iov, hdr->msg_iovlen, &total);
        if (e) {
            return e;
        }

        void *tmp = nullptr;
        if (total > 0) {
            tmp = malloc(total);
            if (!tmp) {
                return ENOMEM;
            }
        }

        socklen_t addrlen = hdr->msg_namelen;
        result = __syscall6(SYS_recvfrom_core, fd, (long)tmp, total, flags,
                            (long)normalized.msg_name, (long)&addrlen);
        if (sc_failed(result)) {
            if (tmp) {
                free(tmp);
            }
            return sc_errno(result);
        }

        if (static_cast<size_t>(result) > total) {
            if (tmp) {
                free(tmp);
            }
            return EIO;
        }

        if (tmp && result > 0) {
            iov_scatter_bytes(tmp, static_cast<size_t>(result), hdr->msg_iov, hdr->msg_iovlen);
        }
        if (tmp) {
            free(tmp);
        }
        hdr->msg_namelen = addrlen;
        hdr->msg_controllen = 0;
        hdr->msg_flags = 0;
        if (control_requested) {
            hdr->msg_flags |= MSG_CTRUNC;
        }
        *length = result;
        return 0;
    }

    if (!hdr->msg_iov || hdr->msg_iovlen != 1) {
        return EINVAL;
    }
    if (hdr->msg_iov[0].iov_len > 0 && !hdr->msg_iov[0].iov_base) {
        return EFAULT;
    }

    struct iovec *iov = hdr->msg_iov;
    socklen_t addrlen = hdr->msg_namelen;
    result = __syscall6(SYS_recvfrom_core, fd, (long)iov[0].iov_base, iov[0].iov_len,
                        flags, (long)normalized.msg_name, (long)&addrlen);
    if (sc_failed(result)) {
        return sc_errno(result);
    }
    hdr->msg_namelen = addrlen;
    hdr->msg_controllen = 0;
    hdr->msg_flags = 0;
    if (control_requested) {
        hdr->msg_flags |= MSG_CTRUNC;
    }
    *length = result;
    return 0;
#endif
}

int sys_setsockopt(int fd, int layer, int number, const void *buffer,
                   socklen_t size) {
    long result = __syscall5(SYS_setsockopt_core, fd, layer, number, (long)buffer, size);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_getsockopt(int fd, int layer, int number, void *__restrict buffer,
                   socklen_t *__restrict size) {
    long result = __syscall5(SYS_getsockopt_core, fd, layer, number, (long)buffer, (long)size);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_sockname(int fd, struct sockaddr *addr, socklen_t max_addr_length,
                 socklen_t *actual_addr_length) {
    if (!actual_addr_length) {
        return EINVAL;
    }
    *actual_addr_length = max_addr_length;
    long result = __syscall3(SYS_getsockname_core, fd, (long)addr, (long)actual_addr_length);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_peername(int fd, struct sockaddr *addr, socklen_t max_addr_length,
                 socklen_t *actual_addr_length) {
    if (!actual_addr_length) {
        return EINVAL;
    }
    *actual_addr_length = max_addr_length;
    long result = __syscall3(SYS_getpeername_core, fd, (long)addr, (long)actual_addr_length);
    if (result < 0) {
        return -result;
    }
    return 0;
}

ssize_t sys_sendto(int fd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest_addr, socklen_t addrlen,
                   ssize_t *bytes_sent) {
    if (!bytes_sent) {
        return EINVAL;
    }
    long result = __syscall6(SYS_sendto_core, fd, (long)buf, len, flags,
                             (long)dest_addr, addrlen);
    if (result < 0) {
        return -result;
    }
    *bytes_sent = result;
    return 0;
}

ssize_t sys_recvfrom(int fd, void *buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addrlen,
                     ssize_t *bytes_recv) {
    if (!bytes_recv) {
        return EINVAL;
    }
    long result = __syscall6(SYS_recvfrom_core, fd, (long)buf, len, flags,
                             (long)src_addr, (long)addrlen);
    if (result < 0) {
        return -result;
    }
    *bytes_recv = result;
    return 0;
}

int sys_shutdown(int sockfd, int how) {
    long result = __syscall2(SYS_shutdown_core, sockfd, how);
    if (result < 0) {
        return -result;
    }
    return 0;
}

/* =============================================================================
 * Poll/Select (placeholder)
 * =============================================================================
 */

int sys_poll(struct pollfd *fds, nfds_t count, int timeout, int *num_events) {
    long result = __syscall3(SYS_poll, (long)fds, (long)count, timeout);
    if (result < 0) {
        return -result;
    }
    *num_events = result;
    return 0;
}

int sys_ppoll(struct pollfd *fds, nfds_t count, const struct timespec *timeout,
              const sigset_t *sigmask, int *num_events) {
    long result = __syscall4(SYS_ppoll_core, (long)fds, (long)count, (long)timeout, (long)sigmask);
    if (sc_enosys(result)) {
        int timeout_ms = -1;
        if (timeout) {
            if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000L) {
                return EINVAL;
            }

            long long ms = timeout->tv_sec * 1000LL + (timeout->tv_nsec + 999999LL) / 1000000LL;
            if (ms > INT_MAX) {
                timeout_ms = INT_MAX;
            } else {
                timeout_ms = static_cast<int>(ms);
            }
        }

        sigset_t old_mask {};
        bool mask_swapped = false;
        if (sigmask) {
            int e = sys_thread_sigmask(SIG_SETMASK, sigmask, &old_mask);
            if (e) {
                return e;
            }
            mask_swapped = true;
        }

        result = __syscall3(SYS_poll, (long)fds, (long)count, timeout_ms);

        int err = 0;
        if (sc_failed(result)) {
            err = sc_errno(result);
        }

        if (mask_swapped) {
            int restore_err = sys_thread_sigmask(SIG_SETMASK, &old_mask, nullptr);
            if (!err && restore_err) {
                err = restore_err;
            }
        }

        if (err) {
            return err;
        }

        *num_events = result;
        return 0;
    }

    if (sc_failed(result)) {
        return sc_errno(result);
    }
    *num_events = result;
    return 0;
}

int sys_pselect(int nfds, fd_set *read_set, fd_set *write_set, fd_set *except_set,
                const struct timespec *timeout, const sigset_t *sigmask,
                int *num_events) {
#ifdef MLIBC_BUILDING_RTLD
    if (!num_events) {
        return EINVAL;
    }
    long result = __syscall6(SYS_pselect_core, nfds, (long)read_set, (long)write_set,
                             (long)except_set, (long)timeout, (long)sigmask);
    if (sc_failed(result)) {
        return sc_errno(result);
    }
    *num_events = result;
    return 0;
#else
    long result = __syscall6(SYS_pselect_core, nfds, (long)read_set, (long)write_set,
                             (long)except_set, (long)timeout, (long)sigmask);
    if (sc_enosys(result)) {
        /* Userspace fallback to ppoll(2)-style behavior with signal-mask swap. */

        if (nfds < 0 || nfds > FD_SETSIZE) {
            return EINVAL;
        }

        fd_set in_read, in_write, in_except;
        if (read_set) {
            memcpy(&in_read, read_set, sizeof(fd_set));
            FD_ZERO(read_set);
        }
        if (write_set) {
            memcpy(&in_write, write_set, sizeof(fd_set));
            FD_ZERO(write_set);
        }
        if (except_set) {
            memcpy(&in_except, except_set, sizeof(fd_set));
            FD_ZERO(except_set);
        }

        struct pollfd *pfds = nullptr;
        if (nfds > 0) {
            pfds = reinterpret_cast<struct pollfd *>(__builtin_alloca(sizeof(struct pollfd) * nfds));
        }

        int poll_count = 0;
        for (int fd = 0; fd < nfds; fd++) {
            short events = 0;
            if (read_set && FD_ISSET(fd, &in_read)) {
                events |= POLLIN;
            }
            if (write_set && FD_ISSET(fd, &in_write)) {
                events |= POLLOUT;
            }
            if (except_set && FD_ISSET(fd, &in_except)) {
                events |= POLLPRI;
            }
            if (!events) {
                continue;
            }

            pfds[poll_count].fd = fd;
            pfds[poll_count].events = events;
            pfds[poll_count].revents = 0;
            poll_count++;
        }

        int ppoll_events = 0;
        int e = sys_ppoll(pfds, poll_count, timeout, sigmask, &ppoll_events);
        if (e) {
            return e;
        }

        int ready = 0;
        for (int i = 0; i < poll_count; i++) {
            short revents = pfds[i].revents;
            bool this_fd_ready = false;

            if (read_set && (revents & (POLLIN | POLLHUP))) {
                FD_SET(pfds[i].fd, read_set);
                this_fd_ready = true;
            }
            if (write_set && (revents & POLLOUT)) {
                FD_SET(pfds[i].fd, write_set);
                this_fd_ready = true;
            }
            if (except_set && (revents & (POLLPRI | POLLERR | POLLNVAL))) {
                FD_SET(pfds[i].fd, except_set);
                this_fd_ready = true;
            }
            if (this_fd_ready) {
                ready++;
            }
        }

        *num_events = ready;
        return 0;
    }

    if (result < 0) {
        return -result;
    }
    *num_events = result;
    return 0;
#endif
}

/* =============================================================================
 * epoll Operations
 * =============================================================================
 */

int sys_epoll_create(int flags, int *fd) {
    long result = __syscall1(SYS_epoll_create_core, flags);
    if (result < 0) {
        return -result;
    }
    *fd = result;
    return 0;
}

int sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    long result = __syscall4(SYS_epoll_ctl_core, epfd, op, fd, (long)event);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                    int timeout, const sigset_t *sigmask, int *raised) {
    sigset_t old_mask {};
    bool mask_swapped = false;
    if (sigmask) {
        int e = sys_thread_sigmask(SIG_SETMASK, sigmask, &old_mask);
        if (e) {
            return e;
        }
        mask_swapped = true;
    }

    long result = __syscall4(SYS_epoll_wait_core, epfd, (long)events, maxevents, timeout);
    int err = 0;
    if (result < 0) {
        err = -result;
    }

    if (mask_swapped) {
        int restore_err = sys_thread_sigmask(SIG_SETMASK, &old_mask, nullptr);
        if (!err && restore_err) {
            return restore_err;
        }
    }

    if (err) {
        return err;
    }

    *raised = result;
    return 0;
}

/* =============================================================================
 * Environment (handled in entry.cpp)
 * =============================================================================
 */

#ifndef MLIBC_BUILDING_RTLD
int sys_environ(char ***envp) {
    *envp = environ;
    return 0;
}
#endif

/* =============================================================================
 * System Information
 * =============================================================================
 */

int sys_uname(struct utsname *buf) {
    long result = __syscall1(SYS_uname, (long)buf);
    return result < 0 ? -result : 0;
}

int sys_gethostname(char *buffer, size_t bufsize) {
    if (!buffer || bufsize == 0) {
        return EINVAL;
    }

    struct utsname uts;
    int e = sys_uname(&uts);
    if (e) {
        return e;
    }

    size_t len = strlen(uts.nodename);
    if (len >= bufsize) {
        return ENAMETOOLONG;
    }

    memcpy(buffer, uts.nodename, len + 1);
    return 0;
}

int sys_umask(mode_t mode, mode_t *old) {
    mode_t previous = g_process_umask;
    g_process_umask = mode & 0777;
    if (old) {
        *old = previous;
    }
    return 0;
}

int sys_getentropy(void *buffer, size_t length) {
    if (!buffer) {
        return EFAULT;
    }
    if (length > 256) {
        return EIO;
    }
    if (length == 0) {
        return 0;
    }

    /* First try the conventional urandom path when available. */
    int fd = -1;
    if (sys_open("/dev/urandom", O_RDONLY | O_CLOEXEC, 0, &fd) == 0) {
        size_t done = 0;
        while (done < length) {
            ssize_t got = 0;
            int e = sys_read(fd, (uint8_t *)buffer + done, length - done, &got);
            if (e) {
                break;
            }
            if (got <= 0) {
                break;
            }
            done += static_cast<size_t>(got);
        }
        (void)sys_close(fd);
        if (done == length) {
            return 0;
        }
    }

    /* Best-effort fallback for early bring-up: deterministic PRNG mixed with
     * time and process identity so callers don't fail with ENOSYS. */
    time_t secs = 0;
    long nanos = 0;
    (void)sys_clock_get(CLOCK_MONOTONIC, &secs, &nanos);

    uint64_t state = (static_cast<uint64_t>(secs) << 32)
        ^ static_cast<uint64_t>(static_cast<uint32_t>(nanos))
        ^ static_cast<uint64_t>(sys_getpid())
        ^ static_cast<uint64_t>(sys_gettid())
        ^ reinterpret_cast<uintptr_t>(buffer)
        ^ 0x9E3779B97F4A7C15ULL;

    uint8_t *out = reinterpret_cast<uint8_t *>(buffer);
    for (size_t i = 0; i < length; i++) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        state *= 0x2545F4914F6CDD1DULL;
        out[i] = static_cast<uint8_t>(state >> 56);
    }

    return 0;
}

/* =============================================================================
 * Compatibility Completion Layer
 * =============================================================================
 */

int sys_before_cancellable_syscall(ucontext_t *uctx) {
    (void)uctx;
    return 0;
}

int sys_brk(void **out) {
	if (!out) {
		return EINVAL;
	}
	long result = __syscall1(SYS_brk_core, 0);
	if (result < 0) {
		*out = nullptr;
		return -result;
	}
	*out = reinterpret_cast<void *>(result);
	return 0;
}

int sys_clock_set(int clock, time_t secs, long nanos) {
	struct timespec ts {};
	ts.tv_sec = secs;
	ts.tv_nsec = nanos;
	long result = __syscall2(SYS_clock_settime_core, clock, (long)&ts);
	return result < 0 ? -result : 0;
}

int sys_fadvise(int fd, off_t offset, off_t length, int advice) {
    (void)fd;
    (void)offset;
    (void)length;
    (void)advice;
    return 0;
}

int sys_fallocate(int fd, off_t offset, size_t size) {
    if (offset < 0) {
        return EINVAL;
    }
    if (size == 0) {
        return 0;
    }

    size_t end = static_cast<size_t>(offset) + size;
    off_t current = 0;
    int e = sys_seek(fd, 0, SEEK_END, &current);
    if (e) {
        return e;
    }
    if (current >= static_cast<off_t>(end)) {
        return 0;
    }
    return sys_ftruncate(fd, end);
}

int sys_futex_tid() {
    return static_cast<int>(sys_gettid());
}

int sys_get_max_priority(int policy, int *out) {
    if (!out) {
        return EINVAL;
    }
    switch (policy) {
        case SCHED_OTHER:
            *out = 0;
            return 0;
        case SCHED_FIFO:
        case SCHED_RR:
            *out = 99;
            return 0;
        default:
            return EINVAL;
    }
}

int sys_get_min_priority(int policy, int *out) {
    if (!out) {
        return EINVAL;
    }
    switch (policy) {
        case SCHED_OTHER:
        case SCHED_FIFO:
        case SCHED_RR:
            *out = 0;
            return 0;
        default:
            return EINVAL;
    }
}

int sys_getparam(pid_t pid, struct sched_param *param) {
    (void)pid;
    if (!param) {
        return EINVAL;
    }
    param->sched_priority = 0;
    return 0;
}

int sys_setparam(pid_t pid, const struct sched_param *param) {
    (void)pid;
    if (!param) {
        return EINVAL;
    }
    return 0;
}

int sys_getschedparam(void *tcb, int *policy, struct sched_param *param) {
    (void)tcb;
    if (!policy || !param) {
        return EINVAL;
    }
    *policy = SCHED_OTHER;
    param->sched_priority = 0;
    return 0;
}

int sys_setschedparam(void *tcb, int policy, const struct sched_param *param) {
    (void)tcb;
    if (!param) {
        return EINVAL;
    }
    if (policy != SCHED_OTHER && policy != SCHED_FIFO && policy != SCHED_RR) {
        return EINVAL;
    }
    return 0;
}

int sys_getscheduler(pid_t pid, int *policy) {
    (void)pid;
    if (!policy) {
        return EINVAL;
    }
    *policy = SCHED_OTHER;
    return 0;
}

int sys_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {
    (void)pid;
    if (!mask || cpusetsize == 0) {
        return EINVAL;
    }
    memset(mask, 0, cpusetsize);
    reinterpret_cast<uint8_t *>(mask)[0] = 0x01;
    return 0;
}

int sys_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask) {
    (void)pid;
    if (!mask || cpusetsize == 0) {
        return EINVAL;
    }
    return 0;
}

int sys_getthreadaffinity(pid_t tid, size_t cpusetsize, cpu_set_t *mask) {
    return sys_getaffinity(tid, cpusetsize, mask);
}

int sys_setthreadaffinity(pid_t tid, size_t cpusetsize, const cpu_set_t *mask) {
    return sys_setaffinity(tid, cpusetsize, mask);
}

int sys_getitimer(int which, struct itimerval *curr_value) {
    if (!curr_value) {
        return EINVAL;
    }

    if (which < ITIMER_REAL || which > ITIMER_PROF) {
        return EINVAL;
    }

    struct timeval now {};
    int e = get_now_timeval(&now);
    if (e) {
        return e;
    }

    compute_itimer_current(which, now, curr_value);
    return 0;
}

int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    if (which < ITIMER_REAL || which > ITIMER_PROF) {
        return EINVAL;
    }
    if (!new_value) {
        return EINVAL;
    }

    if (!valid_timeval(new_value->it_value) || !valid_timeval(new_value->it_interval)) {
        return EINVAL;
    }

    struct timeval now {};
    int e = get_now_timeval(&now);
    if (e) {
        return e;
    }

    if (old_value) {
        compute_itimer_current(which, now, old_value);
    }

    itimer_state &st = g_itimer_state[which];
    st.programmed = *new_value;
    st.armed_at = now;
    st.armed = (new_value->it_value.tv_sec != 0 || new_value->it_value.tv_usec != 0);
    return 0;
}

int sys_getloadavg(double *samples) {
    if (!samples) {
        return EINVAL;
    }
    samples[0] = 0.0;
    samples[1] = 0.0;
    samples[2] = 0.0;
    return 0;
}

int sys_if_indextoname(unsigned int index, char *name) {
    if (!name) {
        return EINVAL;
    }
    if (index == 1) {
        name[0] = 'l';
        name[1] = 'o';
        name[2] = '\0';
        return 0;
    }
    if (index == 2) {
        name[0] = 'e';
        name[1] = 't';
        name[2] = 'h';
        name[3] = '0';
        name[4] = '\0';
        return 0;
    }
    return ENXIO;
}

int sys_if_nametoindex(const char *name, unsigned int *ret) {
    if (!name || !ret) {
        return EINVAL;
    }
    if (name[0] == 'l' && name[1] == 'o' && name[2] == '\0') {
        *ret = 1;
        return 0;
    }
    if (name[0] == 'e' && name[1] == 't' && name[2] == 'h'
            && name[3] == '0' && name[4] == '\0') {
        *ret = 2;
        return 0;
    }
    return ENXIO;
}

int sys_inet_configured(bool *ipv4, bool *ipv6) {
    if (ipv4) {
        *ipv4 = true;
    }
    if (ipv6) {
        *ipv6 = false;
    }
    return 0;
}

int sys_ioperm(unsigned long int from, unsigned long int num, int turn_on) {
    (void)from;
    (void)num;
    (void)turn_on;
    return EPERM;
}

int sys_iopl(int level) {
    (void)level;
    return EPERM;
}

int sys_madvise(void *addr, size_t length, int advice) {
    (void)addr;
    (void)length;
    (void)advice;
    return 0;
}

int sys_posix_madvise(void *addr, size_t length, int advice) {
    return sys_madvise(addr, length, advice);
}

int sys_memfd_create(const char *name, int flags, int *fd) {
#ifdef MLIBC_BUILDING_RTLD
    (void)name;
    (void)flags;
    if (fd) {
        *fd = -1;
    }
    return ENOSYS;
#else
    if (!fd) {
        return EINVAL;
    }
    *fd = -1;

    unsigned int known_flags = MFD_CLOEXEC | MFD_ALLOW_SEALING;
    if (flags & ~static_cast<int>(known_flags)) {
        return EINVAL;
    }

    const char *raw_name = (name && name[0]) ? name : "anonymous";
    char sanitized[33];
    size_t raw_len = strlen(raw_name);
    if (raw_len > 32) {
        raw_len = 32;
    }
    for (size_t i = 0; i < raw_len; i++) {
        char c = raw_name[i];
        sanitized[i] = (c == '/') ? '_' : c;
    }
    sanitized[raw_len] = '\0';

    int open_flags = O_RDWR | O_CREAT | O_EXCL;
    if (flags & MFD_CLOEXEC) {
        open_flags |= O_CLOEXEC;
    }

    const char *dirs[] = {"/tmp", "/dev/shm", "."};
    char path[PATH_MAX];
    pid_t pid = sys_getpid();
    pid_t tid = sys_gettid();

    for (size_t d = 0; d < (sizeof(dirs) / sizeof(dirs[0])); d++) {
        for (int attempt = 0; attempt < 64; attempt++) {
            unsigned long seq = __atomic_add_fetch(&g_memfd_seq, 1UL, __ATOMIC_RELAXED);
            int n = snprintf(path, sizeof(path), "%s/.yolk-memfd-%d-%d-%lu-%s",
                    dirs[d], static_cast<int>(pid), static_cast<int>(tid), seq, sanitized);
            if (n <= 0 || static_cast<size_t>(n) >= sizeof(path)) {
                return ENAMETOOLONG;
            }

            int local_fd = -1;
            int e = sys_open(path, open_flags, 0600, &local_fd);
            if (!e) {
                (void)sys_unlink(path);
                *fd = local_fd;
                return 0;
            }
            if (e != EEXIST) {
                if (e == ENOENT || e == ENOTDIR) {
                    break;
                }
                return e;
            }
        }
    }

    return ENOSPC;
#endif
}

int sys_mincore(void *addr, size_t length, unsigned char *vec) {
    if (!vec) {
        return EINVAL;
    }
    size_t page_count = (length + 4095) / 4096;
    memset(vec, 1, page_count);
    (void)addr;
    return 0;
}

int sys_mkfifoat(int dirfd, const char *path, mode_t mode) {
    long ret = __syscall3(SYS_mkfifoat_core, dirfd, (long)path, mode);
    if (ret < 0) {
        return -ret;
    }
    return 0;
}

int sys_mknodat(int dirfd, const char *path, int mode, int dev) {
    long ret = __syscall4(SYS_mknodat_core, dirfd, (long)path, mode, dev);
    if (ret < 0) {
        return -ret;
    }
    return 0;
}

int sys_mlock(const void *addr, size_t length) {
    (void)addr;
    (void)length;
    return 0;
}

int sys_mlockall(int flags) {
    (void)flags;
    return 0;
}

int sys_msync(void *addr, size_t length, int flags) {
    long ret = __syscall3(SYS_msync_core, (long)addr, length, flags);
    if (ret < 0) {
        return -ret;
    }
    return 0;
}

int sys_munlock(const void *addr, size_t length) {
    (void)addr;
    (void)length;
    return 0;
}

int sys_munlockall(void) {
    return 0;
}

int sys_name_to_handle_at(int dirfd, const char *pathname, struct file_handle *handle, int *mount_id, int flags) {
    (void)dirfd;
    (void)pathname;
    (void)handle;
    (void)mount_id;
    (void)flags;
    return EOPNOTSUPP;
}

int sys_openpt(int oflags, int *fd) {
    if (!fd) {
        return EINVAL;
    }
    return sys_open("/dev/ptmx", oflags, 0, fd);
}

int sys_ptsname(int fd, char *buffer, size_t length) {
#ifdef MLIBC_BUILDING_RTLD
    (void)fd;
    (void)buffer;
    (void)length;
    return ENOSYS;
#else
    if (!buffer || length == 0) {
        return EINVAL;
    }

    uint32_t pty_num = 0;
    int ignored = 0;
    if (sys_ioctl(fd, TIOCGPTN, &pty_num, &ignored) == 0) {
        int n = snprintf(buffer, length, "/dev/pts/%u", pty_num);
        if (n < 0 || static_cast<size_t>(n) >= length) {
            return ERANGE;
        }
        return 0;
    }

    static const char fallback[] = "/dev/pts/0";
    if (length < sizeof(fallback)) {
        return ERANGE;
    }
    memcpy(buffer, fallback, sizeof(fallback));
    return 0;
#endif
}

int sys_unlockpt(int fd) {
    (void)fd;
    return 0;
}

int sys_openpty(int *mfd, int *sfd, char *name, const struct termios *ios, const struct winsize *win) {
#ifdef MLIBC_BUILDING_RTLD
    (void)mfd;
    (void)sfd;
    (void)name;
    (void)ios;
    (void)win;
    return ENOSYS;
#else
    if (!mfd || !sfd) {
        return EINVAL;
    }

    int e = sys_openpt(O_RDWR | O_NOCTTY, mfd);
    if (e) {
        return e;
    }
    e = sys_unlockpt(*mfd);
    if (e) {
        (void)sys_close(*mfd);
        return e;
    }

    char slave_path[64];
    e = sys_ptsname(*mfd, slave_path, sizeof(slave_path));
    if (e) {
        (void)sys_close(*mfd);
        return e;
    }

    e = sys_open(slave_path, O_RDWR | O_NOCTTY, 0, sfd);
    if (e) {
        (void)sys_close(*mfd);
        return e;
    }

    if (name) {
        strcpy(name, slave_path);
    }
    if (ios) {
        (void)sys_tcsetattr(*sfd, TCSANOW, ios);
    }
    if (win) {
        int ignored = 0;
        (void)sys_ioctl(*sfd, TIOCSWINSZ, (void *)win, &ignored);
    }
    return 0;
#endif
}

int sys_pause() {
    sigset_t mask;
    memset(&mask, 0, sizeof(mask));
    return sys_sigsuspend(&mask);
}

int sys_personality(unsigned long persona, int *out) {
    if (!out) {
        return EINVAL;
    }
    *out = static_cast<int>(g_process_personality);

    /* Linux treats 0xFFFFFFFF as query-only. */
    if (persona == 0xFFFFFFFFUL) {
        return 0;
    }

    g_process_personality = persona;
    return 0;
}

int sys_riscv_flush_icache(void *start, void *end, unsigned long flags) {
    (void)start;
    (void)end;
    (void)flags;
    return 0;
}

int sys_riscv_hwprobe(struct riscv_hwprobe *pairs, size_t pair_count, size_t cpusetsize, cpu_set_t *cpus, unsigned int flags) {
    (void)pairs;
    (void)pair_count;
    (void)cpusetsize;
    (void)cpus;
    (void)flags;
    return EOPNOTSUPP;
}

int sys_semctl(int semid, int semnum, int cmd, void *semun, int *ret) {
    long rv = __syscall4(SYS_semctl_core, semid, semnum, cmd, (long)semun);
    if (rv < 0) {
        if (ret) {
            *ret = -1;
        }
        return -rv;
    }
    if (ret) {
        *ret = (int)rv;
    }
    return 0;
}

int sys_semget(key_t key, int n, int fl, int *id) {
    if (!id) {
        return EINVAL;
    }
    long rv = __syscall3(SYS_semget_core, key, n, fl);
    if (rv < 0) {
        *id = -1;
        return -rv;
    }
    *id = (int)rv;
    return 0;
}

int sys_semop(int semid, struct sembuf *sops, size_t nsops) {
    if (!sops || nsops == 0) {
        return EINVAL;
    }
    long rv = __syscall3(SYS_semop_core, semid, (long)sops, nsops);
    if (rv < 0) {
        return -rv;
    }
    return 0;
}

int sys_sethostname(const char *buffer, size_t bufsize) {
    (void)buffer;
    (void)bufsize;
    return EPERM;
}

int sys_shmat(void **seg_start, int shmid, const void *shmaddr, int shmflg) {
    if (!seg_start) {
        return EINVAL;
    }
    long rv = __syscall3(SYS_shmat_core, shmid, (long)shmaddr, shmflg);
    if (rv < 0) {
        *seg_start = nullptr;
        return -rv;
    }
    *seg_start = (void *)rv;
    return 0;
}

int sys_shmctl(int *idx, int shmid, int cmd, struct shmid_ds *buf) {
    long rv = __syscall3(SYS_shmctl_core, shmid, cmd, (long)buf);
    if (rv < 0) {
        if (idx) {
            *idx = -1;
        }
        return -rv;
    }
    if (idx) {
        *idx = (int)rv;
    }
    return 0;
}

int sys_shmdt(const void *shmaddr) {
    long rv = __syscall1(SYS_shmdt_core, (long)shmaddr);
    if (rv < 0) {
        return -rv;
    }
    return 0;
}

int sys_shmget(int *shm_id, key_t key, size_t size, int shmflg) {
    if (!shm_id) {
        return EINVAL;
    }
    long rv = __syscall3(SYS_shmget_core, key, size, shmflg);
    if (rv < 0) {
        *shm_id = -1;
        return -rv;
    }
    *shm_id = (int)rv;
    return 0;
}

int sys_sigaltstack(const stack_t *ss, stack_t *oss) {
    if (oss) {
        *oss = g_sigaltstack_state;
    }

    if (!ss) {
        return 0;
    }

    if (ss->ss_flags & ~(SS_DISABLE)) {
        return EINVAL;
    }

    if (ss->ss_flags & SS_DISABLE) {
        g_sigaltstack_state.ss_sp = nullptr;
        g_sigaltstack_state.ss_size = 0;
        g_sigaltstack_state.ss_flags = SS_DISABLE;
        return 0;
    }

    if (!ss->ss_sp || ss->ss_size < MINSIGSTKSZ) {
        return ENOMEM;
    }

    g_sigaltstack_state = *ss;
    g_sigaltstack_state.ss_flags &= ~SS_DISABLE;
    return 0;
}

int sys_sigtimedwait(const sigset_t *__restrict set, siginfo_t *__restrict info,
		const struct timespec *__restrict timeout, int *out_signal) {
	if (!set) {
		if (out_signal) {
			*out_signal = 0;
		}
		return EINVAL;
	}

	long result = __syscall3(SYS_sigtimedwait_core, (long)set, (long)info, (long)timeout);
	if (result < 0) {
		if (out_signal) {
			*out_signal = 0;
		}
		return -result;
	}

	if (out_signal) {
		*out_signal = static_cast<int>(result);
	}
	return 0;
}

int sys_splice(int in_fd, off_t *in_off, int out_fd, off_t *out_off, size_t size, unsigned int flags, ssize_t *out) {
    if (!out) {
        return EINVAL;
    }
    *out = 0;

    if (in_off || out_off) {
        return EOPNOTSUPP;
    }

    if (flags != 0) {
        return EOPNOTSUPP;
    }

    if (size == 0) {
        return 0;
    }

    char buffer[16384];
    size_t remaining = size;
    while (remaining > 0) {
        size_t chunk = remaining;
        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }

        ssize_t nr = 0;
        int e = sys_read(in_fd, buffer, chunk, &nr);
        if (e) {
            return (*out > 0) ? 0 : e;
        }
        if (nr <= 0) {
            return 0;
        }

        size_t written = 0;
        while (written < static_cast<size_t>(nr)) {
            ssize_t nw = 0;
            e = sys_write(out_fd, buffer + written, static_cast<size_t>(nr) - written, &nw);
            if (e) {
                return (*out > 0) ? 0 : e;
            }
            if (nw <= 0) {
                return EIO;
            }
            written += static_cast<size_t>(nw);
        }

        *out += static_cast<ssize_t>(written);
        remaining -= written;
    }
    return 0;
}

void sys_sync() {
}

int sys_sysconf(int num, long *ret) {
    if (!ret) {
        return EINVAL;
    }
    switch (num) {
        case _SC_PAGESIZE:
            *ret = 4096;
            return 0;
#if defined(_SC_PAGE_SIZE) && (!defined(_SC_PAGESIZE) || (_SC_PAGE_SIZE != _SC_PAGESIZE))
        case _SC_PAGE_SIZE:
            *ret = 4096;
            return 0;
#endif
        case _SC_OPEN_MAX:
            *ret = 1024;
            return 0;
        case _SC_CLK_TCK:
            *ret = 100;
            return 0;
        case _SC_NPROCESSORS_CONF:
        case _SC_NPROCESSORS_ONLN:
            *ret = 1;
            return 0;
        case _SC_ARG_MAX:
            *ret = 2097152;
            return 0;
        case _SC_TZNAME_MAX:
            *ret = -1;
            return 0;
        case _SC_PHYS_PAGES:
            *ret = 1024;
            return 0;
        case _SC_AVPHYS_PAGES:
            *ret = 1024;
            return 0;
        case _SC_GETPW_R_SIZE_MAX:
            *ret = 1024;
            return 0;
        case _SC_GETGR_R_SIZE_MAX:
            *ret = 1024;
            return 0;
        case _SC_CHILD_MAX:
            *ret = 25;
            return 0;
        case _SC_JOB_CONTROL:
            *ret = 1;
            return 0;
        case _SC_NGROUPS_MAX:
            *ret = 65536;
            return 0;
        case _SC_RE_DUP_MAX:
            *ret = RE_DUP_MAX;
            return 0;
        case _SC_LINE_MAX:
            *ret = 2048;
            return 0;
        case _SC_XOPEN_CRYPT:
            *ret = -1;
            return 0;
        case _SC_HOST_NAME_MAX:
            *ret = HOST_NAME_MAX;
            return 0;
        case _SC_LOGIN_NAME_MAX:
            *ret = LOGIN_NAME_MAX;
            return 0;
        case _SC_FSYNC:
            *ret = _POSIX_FSYNC;
            return 0;
        case _SC_SAVED_IDS:
            *ret = _POSIX_SAVED_IDS;
            return 0;
        case _SC_SYMLOOP_MAX:
            *ret = 8;
            return 0;
        case _SC_VERSION:
            *ret = _POSIX_VERSION;
            return 0;
        case _SC_2_VERSION:
            *ret = _POSIX2_VERSION;
            return 0;
        case _SC_XOPEN_VERSION:
            *ret = _XOPEN_VERSION;
            return 0;
        case _SC_MEMLOCK:
            *ret = _POSIX_MEMLOCK;
            return 0;
        case _SC_MEMLOCK_RANGE:
            *ret = _POSIX_MEMLOCK_RANGE;
            return 0;
        case _SC_MAPPED_FILES:
            *ret = _POSIX_MAPPED_FILES;
            return 0;
        case _SC_SHARED_MEMORY_OBJECTS:
            *ret = _POSIX_SHARED_MEMORY_OBJECTS;
            return 0;
        default:
            return EINVAL;
    }
}

int sys_thread_setname(void *tcb, const char *name) {
    (void)tcb;
    (void)name;
    return 0;
}

int sys_thread_getname(void *tcb, char *name, size_t size) {
    (void)tcb;
    if (!name || size == 0) {
        return EINVAL;
    }
    name[0] = '\0';
    return 0;
}

int sys_timer_create(clockid_t clk, struct sigevent *__restrict evp, timer_t *__restrict res) {
	if (!res) {
		return EINVAL;
	}
	if (evp && evp->sigev_notify != SIGEV_NONE) {
		return EOPNOTSUPP;
	}

	long result = __syscall2(SYS_timer_create_core, clk, (long)res);
	return result < 0 ? -result : 0;
}

int sys_timer_settime(timer_t t, int flags, const struct itimerspec *__restrict val,
		struct itimerspec *__restrict old) {
	if (!val) {
		return EINVAL;
	}
	if (!valid_timespec(val->it_value) || !valid_timespec(val->it_interval)) {
		return EINVAL;
	}
	long result = __syscall4(SYS_timer_settime_core, (long)t, flags, (long)val, (long)old);
	return result < 0 ? -result : 0;
}

int sys_timer_gettime(timer_t t, struct itimerspec *val) {
	if (!val) {
		return EINVAL;
	}
	long result = __syscall2(SYS_timer_gettime_core, (long)t, (long)val);
	return result < 0 ? -result : 0;
}

int sys_timer_delete(timer_t t) {
	long result = __syscall1(SYS_timer_delete_core, (long)t);
	return result < 0 ? -result : 0;
}

int sys_times(struct tms *tms, clock_t *out) {
    if (!out) {
        return EINVAL;
    }
    if (tms) {
        memset(tms, 0, sizeof(*tms));
    }
    time_t secs = 0;
    long nanos = 0;
    int e = sys_clock_get(CLOCK_MONOTONIC, &secs, &nanos);
    if (e) {
        return e;
    }
    *out = static_cast<clock_t>(secs * 100 + nanos / 10000000L);
    return 0;
}

int sys_vm_remap(void *pointer, size_t size, size_t new_size, void **window) {
    if (!window) {
        return EINVAL;
    }

    if (!pointer || size == 0 || new_size == 0) {
        return EINVAL;
    }

    if (new_size == size) {
        *window = pointer;
        return 0;
    }

    void *new_window = nullptr;
    int e = sys_vm_map(nullptr, new_size, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, &new_window);
    if (e) {
        return e;
    }

    size_t bytes_to_copy = (size < new_size) ? size : new_size;
    if (bytes_to_copy > 0) {
        memcpy(new_window, pointer, bytes_to_copy);
    }

    e = sys_vm_unmap(pointer, size);
    if (e) {
        (void)sys_vm_unmap(new_window, new_size);
        return e;
    }

    *window = new_window;
    return 0;
}

int sys_waitid(idtype_t idtype, id_t id, siginfo_t *info, int options) {
    pid_t pid = -1;
    if (idtype == P_PID) {
        pid = static_cast<pid_t>(id);
    } else if (idtype == P_PGID) {
        pid = static_cast<pid_t>(-id);
    } else if (idtype != P_ALL) {
        return EINVAL;
    }

    int status = 0;
    struct rusage ru {};
    pid_t ret_pid = 0;
    int e = sys_waitpid(pid, &status, (options & WNOHANG) ? WNOHANG : 0, &ru, &ret_pid);
    if (e) {
        return e;
    }

    if (info) {
        memset(info, 0, sizeof(*info));
        info->si_signo = SIGCHLD;
        info->si_pid = ret_pid;
        if (ret_pid == 0) {
            info->si_code = 0;
        } else if ((status & 0x7F) == 0) {
            info->si_code = CLD_EXITED;
            info->si_status = (status >> 8) & 0xFF;
        } else {
            info->si_code = CLD_KILLED;
            info->si_status = status & 0x7F;
        }
    }
    return 0;
}

int sys_chroot(const char *path) {
    (void)path;
    return EPERM;
}

void sys_yield() {
    __syscall0(SYS_yield);
}

}  // namespace mlibc

extern "C" int __mlibc_yolk_sys_access(const char *path, int mode) {
	long result = __syscall2(SYS_access, (long)path, mode);
	return result < 0 ? -result : 0;
}

extern "C" int __mlibc_yolk_sys_faccessat(int dirfd, const char *pathname, int mode, int flags) {
	long result = __syscall4(SYS_faccessat_core, dirfd, (long)pathname, mode, flags);
	if (mlibc::sc_enosys(result)) {
		if (flags != 0) {
			return EINVAL;
		}
		if (dirfd != AT_FDCWD && pathname && pathname[0] != '/') {
			return ENOSYS;
		}
		result = __syscall2(SYS_access, (long)pathname, mode);
	}
	return mlibc::sc_failed(result) ? mlibc::sc_errno(result) : 0;
}

extern "C" int __mlibc_yolk_sys_chdir(const char *path) {
	long result = __syscall1(SYS_chdir, (long)path);
	return result < 0 ? -result : 0;
}

extern "C" int __mlibc_yolk_sys_fchdir(int fd) {
	long result = __syscall1(SYS_fchdir, fd);
	return result < 0 ? -result : 0;
}

extern "C" int __mlibc_yolk_sys_open_dir(const char *path, int *handle) {
	if (!handle) {
		return EINVAL;
	}

	long result = __syscall4(SYS_open_core, (long)path, O_RDONLY | O_DIRECTORY, 0, AT_FDCWD);
	if (result < 0) {
		return -result;
	}

	*handle = static_cast<int>(result);
	return 0;
}

extern "C" int __mlibc_yolk_sys_read_entries(int handle, void *buffer, size_t max_size, size_t *bytes_read) {
	if (!bytes_read) {
		return EINVAL;
	}

	long result = __syscall3(SYS_getdents, handle, (long)buffer, max_size);
	if (result < 0) {
		return -result;
	}

	if (static_cast<size_t>(result) > max_size) {
		return EIO;
	}

	*bytes_read = static_cast<size_t>(result);
	return 0;
}
