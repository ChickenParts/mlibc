/*
 * Yolk syscall wrappers for mlibc
 * SPDX-License-Identifier: MIT
 */

#ifndef _YOLK_SYSCALL_H
#define _YOLK_SYSCALL_H

#include <stdint.h>

/* Yolk syscall numbers - must match kernel/syscall/nr.h */

/* Basic I/O */
#define SYS_write           1
#define SYS_exit            2
#define SYS_read            3
#define SYS_poll            7   /* Poll file descriptors */
#define SYS_mmap            9
#define SYS_mprotect        10
#define SYS_munmap          11
#define SYS_ioctl           16
#define SYS_yield           24
#define SYS_dup             32
#define SYS_dup2            33
#define SYS_dup3            34
#define SYS_pipe            35
#define SYS_pipe2           36
#define SYS_fcntl           72
#define SYS_ppoll           271 /* Poll with sigmask */

/* VFS syscalls */
#define SYS_lseek           8
#define SYS_stat            4
#define SYS_fstat           5
#define SYS_lstat           6
#define SYS_mkdir           83
#define SYS_rmdir           84
#define SYS_link            86
#define SYS_unlink          87
#define SYS_symlink         88
#define SYS_readlink        89
#define SYS_chmod           90
#define SYS_chown           92
#define SYS_rename          82
#define SYS_truncate        76
#define SYS_ftruncate       77
#define SYS_fsync           74
#define SYS_utimes          235
#define SYS_statfs          137
#define SYS_mount           165
#define SYS_umount          166
#define SYS_getdents        78
#define SYS_getcwd          169
#define SYS_chdir           170
#define SYS_fchdir          171
#define SYS_fchmod          91
#define SYS_fchown          93
#define SYS_access          94
#define SYS_fstatat         262
#define SYS_mkdirat         263
#define SYS_unlinkat        264
#define SYS_renameat        265
#define SYS_fchmodat        266
#define SYS_faccessat       267
#define SYS_utimensat       268
#define SYS_pselect         270
#define SYS_open            257
#define SYS_close           258

/* Futex syscalls */
#define SYS_futex_wait      224
#define SYS_futex_wake      225
#define SYS_futex           228

/* Time syscalls */
#define SYS_clock_gettime   288
#define SYS_clock_getres    290
#define SYS_nanosleep       291
#define SYS_gettimeofday    293

/* System info syscalls */
#define SYS_uname           63

/* Process syscalls */
#define SYS_fork            320
#define SYS_vfork           321
#define SYS_clone           322
#define SYS_getpid          323
#define SYS_getppid         324
#define SYS_getuid          325
#define SYS_geteuid         326
#define SYS_getgid          327
#define SYS_getegid         328
#define SYS_gettid          329
#define SYS_setpgid         330
#define SYS_getpgid         331
#define SYS_setsid          332
#define SYS_getsid          333
#define SYS_execve          334
#define SYS_exit_group      335
#define SYS_waitpid         336
#define SYS_wait4           337

/* Signal syscalls */
#define SYS_kill            340
#define SYS_sigaction       341
#define SYS_sigprocmask     342
#define SYS_sigreturn       343
#define SYS_sigsuspend      344
#define SYS_sigpending      345
#define SYS_tkill           346
#define SYS_tgkill          347

/* IPC syscalls */
#define SYS_endpoint_create     100
#define SYS_ipc_send            101
#define SYS_ipc_recv            102
#define SYS_ipc_call            103
#define SYS_ipc_reply           104
#define SYS_ipc_send_timeout    105
#define SYS_ipc_recv_timeout    106
#define SYS_ipc_call_timeout    107

/* Shared memory syscalls */
#define SYS_shmem_create        108
#define SYS_shmem_destroy       109
#define SYS_shmem_map           110
#define SYS_shmem_unmap         111
#define SYS_shmem_protect       112
#define SYS_shmem_stat          113
#define SYS_shmem_resize        114

/* Service registration */
#define SYS_service_register 350

/* Network/Socket syscalls - must match kernel nr.h (400-420 range) */
#define SYS_socket          400
#define SYS_socketpair      401
#define SYS_bind            402
#define SYS_listen          403
#define SYS_accept          404
#define SYS_accept4         405
#define SYS_connect         406
#define SYS_sendto          407
#define SYS_recvfrom        408
#define SYS_sendmsg         409
#define SYS_recvmsg         410
#define SYS_getsockopt      411
#define SYS_setsockopt      412
#define SYS_shutdown        413
#define SYS_getsockname     414
#define SYS_getpeername     415

/* epoll syscalls (420-422) */
#define SYS_epoll_create    420
#define SYS_epoll_ctl       421
#define SYS_epoll_wait      422

/* Console syscalls */
#define SYS_console_takeover 360  /* Request userspace takeover of console */
#define SYS_console_release  361  /* Release console back to kernel */

/* Console types */
#define CONSOLE_SERIAL      0
#define CONSOLE_FBCON       1

#ifdef __cplusplus
extern "C" {
#endif

/* Syscall with varying number of arguments */
static inline long __syscall0(long n) {
    long ret;
#if defined(__x86_64__)
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n)
        : "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0");
    __asm__ volatile(
        "svc 0"
        : "=r"(x0)
        : "r"(x8)
        : "memory"
    );
    ret = x0;
#elif defined(__riscv)
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0");
    __asm__ volatile(
        "ecall"
        : "=r"(a0)
        : "r"(a7)
        : "memory"
    );
    ret = a0;
#else
#error "Unsupported architecture"
#endif
    return ret;
}

static inline long __syscall1(long n, long a1) {
    long ret;
#if defined(__x86_64__)
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1)
        : "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    __asm__ volatile(
        "svc 0"
        : "+r"(x0)
        : "r"(x8)
        : "memory"
    );
    ret = x0;
#elif defined(__riscv)
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a1;
    __asm__ volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a7)
        : "memory"
    );
    ret = a0;
#endif
    return ret;
}

static inline long __syscall2(long n, long a1, long a2) {
    long ret;
#if defined(__x86_64__)
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    __asm__ volatile(
        "svc 0"
        : "+r"(x0)
        : "r"(x8), "r"(x1)
        : "memory"
    );
    ret = x0;
#elif defined(__riscv)
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a1;
    register long a1_reg __asm__("a1") = a2;
    __asm__ volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a7), "r"(a1_reg)
        : "memory"
    );
    ret = a0;
#endif
    return ret;
}

static inline long __syscall3(long n, long a1, long a2, long a3) {
    long ret;
#if defined(__x86_64__)
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    __asm__ volatile(
        "svc 0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory"
    );
    ret = x0;
#elif defined(__riscv)
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a1;
    register long a1_reg __asm__("a1") = a2;
    register long a2_reg __asm__("a2") = a3;
    __asm__ volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a7), "r"(a1_reg), "r"(a2_reg)
        : "memory"
    );
    ret = a0;
#endif
    return ret;
}

static inline long __syscall4(long n, long a1, long a2, long a3, long a4) {
    long ret;
#if defined(__x86_64__)
    register long r10 __asm__("r10") = a4;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    __asm__ volatile(
        "svc 0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3)
        : "memory"
    );
    ret = x0;
#elif defined(__riscv)
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a1;
    register long a1_reg __asm__("a1") = a2;
    register long a2_reg __asm__("a2") = a3;
    register long a3_reg __asm__("a3") = a4;
    __asm__ volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a7), "r"(a1_reg), "r"(a2_reg), "r"(a3_reg)
        : "memory"
    );
    ret = a0;
#endif
    return ret;
}

static inline long __syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
    long ret;
#if defined(__x86_64__)
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    register long x4 __asm__("x4") = a5;
    __asm__ volatile(
        "svc 0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
        : "memory"
    );
    ret = x0;
#elif defined(__riscv)
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a1;
    register long a1_reg __asm__("a1") = a2;
    register long a2_reg __asm__("a2") = a3;
    register long a3_reg __asm__("a3") = a4;
    register long a4_reg __asm__("a4") = a5;
    __asm__ volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a7), "r"(a1_reg), "r"(a2_reg), "r"(a3_reg), "r"(a4_reg)
        : "memory"
    );
    ret = a0;
#endif
    return ret;
}

static inline long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
#if defined(__x86_64__)
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    register long x4 __asm__("x4") = a5;
    register long x5 __asm__("x5") = a6;
    __asm__ volatile(
        "svc 0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory"
    );
    ret = x0;
#elif defined(__riscv)
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a1;
    register long a1_reg __asm__("a1") = a2;
    register long a2_reg __asm__("a2") = a3;
    register long a3_reg __asm__("a3") = a4;
    register long a4_reg __asm__("a4") = a5;
    register long a5_reg __asm__("a5") = a6;
    __asm__ volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a7), "r"(a1_reg), "r"(a2_reg), "r"(a3_reg), "r"(a4_reg), "r"(a5_reg)
        : "memory"
    );
    ret = a0;
#endif
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif /* _YOLK_SYSCALL_H */
