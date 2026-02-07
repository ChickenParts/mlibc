/*
 * Yolk sysdeps for mlibc - Generic syscall implementations
 * SPDX-License-Identifier: MIT
 *
 * This file implements mlibc's syscall interface using Yolk syscalls.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>  /* For ENOSYS, EAGAIN, etc. */
#include <sys/types.h>
#include <sys/utsname.h>
#include <bits/winsize.h>  /* For struct winsize */

#define TIOCGWINSZ 0x5413

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
    __syscall1(SYS_exit_group, status);
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
    long result = __syscall4(SYS_open_core, (long)path, flags, mode, AT_FDCWD);
    if (result < 0) {
        return -result;
    }
    *fd = result;
    return 0;
}

int sys_openat(int dirfd, const char *path, int flags, mode_t mode, int *fd) {
    /* Yolk's open takes dirfd as 4th arg */
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

int sys_write(int fd, const void *buf, size_t count, ssize_t *bytes_written) {
    long result = __syscall3(SYS_write, fd, (long)buf, count);
    if (result < 0) {
        return -result;
    }
    *bytes_written = result;
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
        result = __syscall4(SYS_fstatat, fd, (long)path, (long)statbuf, flags);
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
    long result = __syscall2(SYS_mkdir, (long)path, mode);
    return result < 0 ? -result : 0;
}

int sys_mkdirat(int dirfd, const char *path, mode_t mode) {
    long result = __syscall3(SYS_mkdirat, dirfd, (long)path, mode);
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

int sys_unlink(const char *path) {
    long result = __syscall1(SYS_unlink, (long)path);
    return result < 0 ? -result : 0;
}

int sys_unlinkat(int dirfd, const char *path, int flags) {
    long result = __syscall3(SYS_unlinkat, dirfd, (long)path, flags);
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

int sys_readlink(const char *path, char *buffer, size_t max_size, ssize_t *length) {
    long result = __syscall3(SYS_readlink, (long)path, (long)buffer, max_size);
    if (result < 0) {
        return -result;
    }
    *length = result;
    return 0;
}

int sys_rename(const char *old_path, const char *new_path) {
    long result = __syscall2(SYS_rename, (long)old_path, (long)new_path);
    return result < 0 ? -result : 0;
}

int sys_renameat(int old_dirfd, const char *old_path, int new_dirfd,
                 const char *new_path) {
    long result = __syscall4(SYS_renameat, old_dirfd, (long)old_path, new_dirfd, (long)new_path);
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
    long result = __syscall4(SYS_fchmodat, dirfd, (long)path, mode, flags);
    if (sc_enosys(result) && dirfd == AT_FDCWD) {
        if (flags != 0) {
            return EINVAL;
        }
        result = __syscall2(SYS_chmod, (long)path, mode);
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

int sys_access(const char *path, int mode) {
    long result = __syscall2(SYS_access, (long)path, mode);
    return result < 0 ? -result : 0;
}

int sys_faccessat(int dirfd, const char *pathname, int mode, int flags) {
    long result = __syscall4(SYS_faccessat, dirfd, (long)pathname, mode, flags);
    if (sc_enosys(result) && dirfd == AT_FDCWD) {
        if (flags != 0) {
            return EINVAL;
        }
        result = __syscall2(SYS_access, (long)pathname, mode);
    }
    return sc_failed(result) ? sc_errno(result) : 0;
}

int sys_utimensat(int dirfd, const char *pathname, const struct timespec times[2],
                  int flags) {
    long result = __syscall4(SYS_utimensat, dirfd, (long)pathname, (long)times, flags);
    if (sc_enosys(result) && dirfd == AT_FDCWD && pathname && flags == 0) {
        struct timeval tv[2];
        tv[0].tv_sec = times[0].tv_sec;
        tv[0].tv_usec = times[0].tv_nsec / 1000;
        tv[1].tv_sec = times[1].tv_sec;
        tv[1].tv_usec = times[1].tv_nsec / 1000;
        result = __syscall2(SYS_utimes, (long)pathname, (long)tv);
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

int sys_setpgid(pid_t pid, pid_t pgid) {
    long result = __syscall2(SYS_setpgid, pid, pgid);
    return result < 0 ? -result : 0;
}

int sys_getpgid(pid_t pid, pid_t *pgid) {
    long result = __syscall1(SYS_getpgid, pid);
    if (result < 0) {
        return -result;
    }
    *pgid = result;
    return 0;
}

int sys_setsid(pid_t *sid) {
    long result = __syscall0(SYS_setsid);
    if (result < 0) {
        return -result;
    }
    *sid = result;
    return 0;
}

int sys_getsid(pid_t pid, pid_t *sid) {
    long result = __syscall1(SYS_getsid, pid);
    if (result < 0) {
        return -result;
    }
    *sid = result;
    return 0;
}

int sys_fork(pid_t *child) {
    long result = __syscall0(SYS_fork);
    if (result < 0) {
        return -result;
    }
    *child = result;
    return 0;
}

int sys_execve(const char *path, char *const argv[], char *const envp[]) {
    long result = __syscall3(SYS_execve, (long)path, (long)argv, (long)envp);
    /* execve only returns on error */
    return -result;
}

int sys_waitpid(pid_t pid, int *status, int flags, struct rusage *ru, pid_t *ret_pid) {
    long result = __syscall4(SYS_wait4, pid, (long)status, flags, (long)ru);
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
    long result = __syscall2(SYS_kill, pid, sig);
    return result < 0 ? -result : 0;
}

int sys_tgkill(pid_t tgid, pid_t tid, int sig) {
    long result = __syscall3(SYS_tgkill, tgid, tid, sig);
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
        long result = __syscall3(SYS_sigaction, signum, (long)&modified_act, (long)oldact);
        return result < 0 ? -result : 0;
    }
    long result = __syscall3(SYS_sigaction, signum, (long)act, (long)oldact);
    return result < 0 ? -result : 0;
}

int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    long result = __syscall3(SYS_sigprocmask, how, (long)set, (long)oldset);
    return result < 0 ? -result : 0;
}

int sys_sigpending(sigset_t *set) {
    long result = __syscall1(SYS_sigpending, (long)set);
    return result < 0 ? -result : 0;
}

int sys_sigsuspend(const sigset_t *mask) {
    long result = __syscall1(SYS_sigsuspend, (long)mask);
    return result < 0 ? -result : 0;
}

void sys_sigreturn() {
    __syscall0(SYS_sigreturn);
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
    long result = __syscall3(SYS_socket, domain, type, protocol);
    if (result < 0) {
        return -result;
    }
    *fd = result;
    return 0;
}

int sys_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    long result = __syscall3(SYS_bind, fd, (long)addr, addrlen);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_listen(int fd, int backlog) {
    long result = __syscall2(SYS_listen, fd, backlog);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_accept(int fd, int *newfd, struct sockaddr *addr, socklen_t *addrlen,
               int flags) {
    long result;
    if (flags) {
        result = __syscall4(SYS_accept4, fd, (long)addr, (long)addrlen, flags);
    } else {
        result = __syscall3(SYS_accept, fd, (long)addr, (long)addrlen);
    }
    if (result < 0) {
        return -result;
    }
    *newfd = result;
    return 0;
}

int sys_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    long result = __syscall3(SYS_connect, fd, (long)addr, addrlen);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_msg_send(int fd, const struct msghdr *hdr, int flags, ssize_t *length) {
    long result = __syscall3(SYS_sendmsg, fd, (long)hdr, flags);
    if (result < 0) {
        return -result;
    }
    *length = result;
    return 0;
}

int sys_msg_recv(int fd, struct msghdr *hdr, int flags, ssize_t *length) {
    long result = __syscall3(SYS_recvmsg, fd, (long)hdr, flags);
    if (result < 0) {
        return -result;
    }
    *length = result;
    return 0;
}

int sys_setsockopt(int fd, int layer, int number, const void *buffer,
                   socklen_t size) {
    long result = __syscall5(SYS_setsockopt, fd, layer, number, (long)buffer, size);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_getsockopt(int fd, int layer, int number, void *__restrict buffer,
                   socklen_t *__restrict size) {
    long result = __syscall5(SYS_getsockopt, fd, layer, number, (long)buffer, (long)size);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_sockname(int fd, struct sockaddr *addr, socklen_t max_addr_length,
                 socklen_t *actual_addr_length) {
    *actual_addr_length = max_addr_length;
    long result = __syscall3(SYS_getsockname, fd, (long)addr, (long)actual_addr_length);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_peername(int fd, struct sockaddr *addr, socklen_t max_addr_length,
                 socklen_t *actual_addr_length) {
    *actual_addr_length = max_addr_length;
    long result = __syscall3(SYS_getpeername, fd, (long)addr, (long)actual_addr_length);
    if (result < 0) {
        return -result;
    }
    return 0;
}

ssize_t sys_sendto(int fd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest_addr, socklen_t addrlen,
                   ssize_t *bytes_sent) {
    long result = __syscall6(SYS_sendto, fd, (long)buf, len, flags,
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
    long result = __syscall6(SYS_recvfrom, fd, (long)buf, len, flags,
                             (long)src_addr, (long)addrlen);
    if (result < 0) {
        return -result;
    }
    *bytes_recv = result;
    return 0;
}

int sys_shutdown(int sockfd, int how) {
    long result = __syscall2(SYS_shutdown, sockfd, how);
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

int sys_pselect(int nfds, fd_set *read_set, fd_set *write_set, fd_set *except_set,
                const struct timespec *timeout, const sigset_t *sigmask,
                int *num_events) {
    long result = __syscall6(SYS_pselect, nfds, (long)read_set, (long)write_set,
                             (long)except_set, (long)timeout, (long)sigmask);
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
    long result = __syscall1(SYS_epoll_create, flags);
    if (result < 0) {
        return -result;
    }
    *fd = result;
    return 0;
}

int sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    long result = __syscall4(SYS_epoll_ctl, epfd, op, fd, (long)event);
    if (result < 0) {
        return -result;
    }
    return 0;
}

int sys_epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                    int timeout, const sigset_t *sigmask, int *raised) {
    (void)sigmask;  /* TODO: sigmask support in kernel */
    long result = __syscall4(SYS_epoll_wait, epfd, (long)events, maxevents, timeout);
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

}  // namespace mlibc
