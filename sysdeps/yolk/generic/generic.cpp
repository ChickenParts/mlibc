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
#include <abi-bits/statfs.h>
#include <bits/winsize.h>  /* For struct winsize */

#define TIOCGWINSZ 0x5413
#define TCGETS    0x5401
#define TCSETS    0x5402
#define TCSETSW   0x5403
#define TCSETSF   0x5404
#define TCSBRK    0x5409
#define TCXONC    0x540A
#define TCFLSH    0x540B

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
    /* x86_64: Use ARCH_SET_FS via arch_prctl equivalent */
    /* For now, we'll use the FSBASE MSR directly via syscall */
    /* Yolk should implement arch_prctl or we use inline asm */
    __asm__ volatile("wrfsbase %0" :: "r"(pointer) : "memory");
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
    (void)flags;  /* TODO: handle O_CLOEXEC */
    long result = __syscall1(SYS_dup, fd);
    if (result < 0) {
        return -result;
    }
    *newfd = result;
    return 0;
}

int sys_dup2(int fd, int flags, int newfd) {
    (void)flags;  /* TODO: handle O_CLOEXEC */
    long result = __syscall2(SYS_dup2, fd, newfd);
    if (result < 0) {
        return -result;
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

int sys_mkdirat(int dirfd, const char *path, mode_t mode) {
    mode &= ~g_process_umask;
    long result = __syscall3(SYS_mkdirat_core, dirfd, (long)path, mode);
    if (sc_enosys(result) && dirfd == AT_FDCWD) {
        result = __syscall2(SYS_mkdir, (long)path, mode);
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

    /* Kernel returns entry count; mlibc dirent core expects byte count. */
    *bytes_read = static_cast<size_t>(result) * sizeof(struct dirent);
    return 0;
}

int sys_readdir(int fd, struct dirent *entry, size_t max_size) {
    /* Yolk's getdents returns linux_dirent64 format */
    /* For now, read one entry at a time */
    (void)fd;
    (void)entry;
    (void)max_size;
    return ENOSYS;  /* TODO: Implement properly */
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

int sys_readlink(const char *path, char *buffer, size_t max_size, ssize_t *length) {
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
	int readlink_e = sys_readlink(resolved, static_cast<char *>(buffer), max_size, length);
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
extern "C" void __mlibc_signal_restore(void);
extern "C" void __mlibc_signal_restore_rt(void);

#ifndef SA_RESTORER
#define SA_RESTORER 0x04000000
#endif

int sys_sigaction(int signum, const struct sigaction *act,
                  struct sigaction *oldact) {
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

int sys_getrlimit(int resource, struct rlimit *rlim) {
    /* TODO: Implement resource limits in Yolk */
    (void)resource;
    rlim->rlim_cur = RLIM_INFINITY;
    rlim->rlim_max = RLIM_INFINITY;
    return 0;
}

int sys_setrlimit(int resource, const struct rlimit *rlim) {
    /* TODO: Implement resource limits in Yolk */
    (void)resource;
    (void)rlim;
    return 0;
}

int sys_getrusage(int who, struct rusage *usage) {
    /* TODO: Implement rusage in Yolk */
    (void)who;
    memset(usage, 0, sizeof(*usage));
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
    if (!hdr || !hdr->msg_iov || hdr->msg_iovlen == 0) {
        long result = __syscall6(SYS_sendto_core, fd, 0, 0, flags,
                                 (long)(hdr ? hdr->msg_name : nullptr),
                                 (long)(hdr ? hdr->msg_namelen : 0));
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        *length = result;
        return 0;
    }

    if (hdr->msg_iovlen > 1 && (!hdr->msg_control || !hdr->msg_controllen)) {
        size_t total = 0;
        int e = iov_total_length(hdr->msg_iov, hdr->msg_iovlen, &total);
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

        long result = __syscall6(SYS_sendto_core, fd, (long)tmp, total, flags,
                                 (long)hdr->msg_name, hdr->msg_namelen);
        if (tmp) {
            free(tmp);
        }
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        *length = result;
        return 0;
    }

	long result = __syscall3(SYS_sendmsg_core, fd, (long)hdr, flags);
	if (sc_enosys(result)) {
		if (!hdr || !hdr->msg_iov || hdr->msg_iovlen != 1) {
			return EINVAL;
		}
		if (hdr->msg_control && hdr->msg_controllen) {
			return EOPNOTSUPP;
		}

        const struct iovec *iov = hdr->msg_iov;
        result = __syscall6(SYS_sendto_core, fd, (long)iov[0].iov_base, iov[0].iov_len,
                            flags, (long)hdr->msg_name, hdr->msg_namelen);
    }
    if (result < 0) {
        return -result;
    }
    *length = result;
    return 0;
}

int sys_msg_recv(int fd, struct msghdr *hdr, int flags, ssize_t *length) {
    if (!hdr || !hdr->msg_iov || hdr->msg_iovlen == 0) {
        socklen_t addrlen = hdr ? hdr->msg_namelen : 0;
        long result = __syscall6(SYS_recvfrom_core, fd, 0, 0, flags,
                                 (long)(hdr ? hdr->msg_name : nullptr),
                                 (long)&addrlen);
        if (sc_failed(result)) {
            return sc_errno(result);
        }
        if (hdr) {
            hdr->msg_namelen = addrlen;
        }
        *length = result;
        return 0;
    }

    if (hdr->msg_iovlen > 1 && (!hdr->msg_control || !hdr->msg_controllen)) {
        size_t total = 0;
        int e = iov_total_length(hdr->msg_iov, hdr->msg_iovlen, &total);
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
        long result = __syscall6(SYS_recvfrom_core, fd, (long)tmp, total, flags,
                                 (long)hdr->msg_name, (long)&addrlen);
        if (sc_failed(result)) {
            if (tmp) {
                free(tmp);
            }
            return sc_errno(result);
        }

        if (tmp && result > 0) {
            iov_scatter_bytes(tmp, static_cast<size_t>(result), hdr->msg_iov, hdr->msg_iovlen);
            free(tmp);
        }
        hdr->msg_namelen = addrlen;
        *length = result;
        return 0;
    }

	long result = __syscall3(SYS_recvmsg_core, fd, (long)hdr, flags);
	if (sc_enosys(result)) {
		if (!hdr || !hdr->msg_iov || hdr->msg_iovlen != 1) {
			return EINVAL;
		}
		if (hdr->msg_control && hdr->msg_controllen) {
			return EOPNOTSUPP;
		}

        struct iovec *iov = hdr->msg_iov;
        socklen_t addrlen = hdr->msg_namelen;
        result = __syscall6(SYS_recvfrom_core, fd, (long)iov[0].iov_base, iov[0].iov_len,
                            flags, (long)hdr->msg_name, (long)&addrlen);
        if (!sc_failed(result)) {
            hdr->msg_namelen = addrlen;
        }
    }
    if (result < 0) {
        return -result;
    }
    *length = result;
    return 0;
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
    *actual_addr_length = max_addr_length;
    long result = __syscall3(SYS_getsockname_core, fd, (long)addr, (long)actual_addr_length);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_peername(int fd, struct sockaddr *addr, socklen_t max_addr_length,
                 socklen_t *actual_addr_length) {
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
        /* Userspace fallback to poll(2). This intentionally ignores atomic
         * signal-mask switching semantics for now, matching current pselect fallback. */
        (void)sigmask;

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

        result = __syscall3(SYS_poll, (long)fds, (long)count, timeout_ms);
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
    long result = __syscall6(SYS_pselect_core, nfds, (long)read_set, (long)write_set,
                             (long)except_set, (long)timeout, (long)sigmask);
    if (sc_enosys(result)) {
        /* Userspace fallback to poll(2).
         * Note: like the current epoll_pwait path, this ignores sigmask
         * atomicity semantics for now. */
        (void)sigmask;

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
    (void)sigmask;  /* TODO: sigmask support in kernel */
    long result = __syscall4(SYS_epoll_wait_core, epfd, (long)events, maxevents, timeout);
    if (result < 0) {
        return -result;
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

}  // namespace mlibc
