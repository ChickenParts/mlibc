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
/* Core-Yolk aliases for identity syscalls */
#define SYS_getpid_core     25
#define SYS_getppid_core    26
#define SYS_gettid_core     27
#define SYS_getuid_core     28
#define SYS_geteuid_core    29
#define SYS_getgid_core     30
#define SYS_getegid_core    31
#define SYS_dup             32
#define SYS_dup2            33
#define SYS_dup3            34
#define SYS_pipe            35
#define SYS_pipe2           36
/* Core-Yolk aliases for FD lifecycle */
#define SYS_open_core       37
#define SYS_close_core      38
/* Core-Yolk aliases for time primitives */
#define SYS_clock_gettime_core 39
#define SYS_clock_getres_core  40
#define SYS_nanosleep_core     41
#define SYS_gettimeofday_core  42
/* Core-Yolk aliases for process control */
#define SYS_fork_core          43
#define SYS_execve_core        44
#define SYS_exit_group_core    45
#define SYS_waitpid_core       46
#define SYS_wait4_core         47
/* Core-Yolk aliases for job control */
#define SYS_setpgid_core       48
#define SYS_getpgid_core       49
#define SYS_setsid_core        50
#define SYS_getsid_core        51
/* Core-Yolk aliases for signal control */
#define SYS_kill_core          52
#define SYS_sigaction_core     53
#define SYS_sigprocmask_core   54
#define SYS_sigreturn_core     55
#define SYS_sigsuspend_core    56
#define SYS_sigpending_core    57
#define SYS_tgkill_core        58
#define SYS_clone_core         59
#define SYS_vfork_core         60
#define SYS_tkill_core         61
/* Core-Yolk aliases for minimal *at compatibility subset */
#define SYS_fstatat_core       64
#define SYS_mkdirat_core       65
#define SYS_unlinkat_core      66
#define SYS_renameat_core      67
#define SYS_fchmodat_core      68
#define SYS_faccessat_core     69
#define SYS_utimensat_core     70
/* Core-Yolk aliases for socket/network paths */
#define SYS_socket_core        115
#define SYS_socketpair_core    116
#define SYS_bind_core          117
#define SYS_listen_core        118
#define SYS_accept_core        119
#define SYS_accept4_core       120
#define SYS_connect_core       121
#define SYS_sendto_core        122
#define SYS_recvfrom_core      123
#define SYS_sendmsg_core       124
#define SYS_recvmsg_core       125
#define SYS_getsockopt_core    126
#define SYS_setsockopt_core    127
#define SYS_shutdown_core      128
#define SYS_getsockname_core   129
#define SYS_getpeername_core   130
/* Core-Yolk aliases for epoll */
#define SYS_epoll_create_core  131
#define SYS_epoll_ctl_core     132
#define SYS_epoll_wait_core    133
/* Core-Yolk aliases for poll-with-mask */
#define SYS_pselect_core       134
#define SYS_ppoll_core         135
/* Native async waitset core syscalls */
#define SYS_waitset_create     160
#define SYS_waitset_ctl        161
#define SYS_waitset_wait       162
#define SYS_fstatfs_core       163
/* Core-Yolk alias for legacy service registration */
#define SYS_service_register_core 96
/* Core-Yolk aliases for legacy console control */
#define SYS_console_takeover_core 140
#define SYS_console_release_core  141
#define SYS_fcntl           72

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
#define SYS_fstatfs        SYS_fstatfs_core
#define SYS_mount           165
#define SYS_umount          166
#define SYS_getdents        78
#define SYS_getcwd          169
#define SYS_chdir           170
#define SYS_fchdir          171
#define SYS_fchmod          91
#define SYS_fchown          93
#define SYS_access          94
#define SYS_fstatat         SYS_fstatat_core
#define SYS_mkdirat         SYS_mkdirat_core
#define SYS_unlinkat        SYS_unlinkat_core
#define SYS_renameat        SYS_renameat_core
#define SYS_fchmodat        SYS_fchmodat_core
#define SYS_faccessat       SYS_faccessat_core
#define SYS_utimensat       SYS_utimensat_core
#define SYS_pselect         SYS_pselect_core
#define SYS_ppoll           SYS_ppoll_core
#define SYS_open            SYS_open_core
#define SYS_close           SYS_close_core

/* Futex syscalls */
#define SYS_futex_wait      224
#define SYS_futex_wake      225
#define SYS_futex           228

/* Time syscalls */
#define SYS_clock_gettime   SYS_clock_gettime_core
#define SYS_clock_getres    SYS_clock_getres_core
#define SYS_nanosleep       SYS_nanosleep_core
#define SYS_gettimeofday    SYS_gettimeofday_core

/* System info syscalls */
#define SYS_uname           63

/* Process syscalls */
#define SYS_getpid          SYS_getpid_core
#define SYS_getppid         SYS_getppid_core
#define SYS_getuid          SYS_getuid_core
#define SYS_geteuid         SYS_geteuid_core
#define SYS_getgid          SYS_getgid_core
#define SYS_getegid         SYS_getegid_core
#define SYS_fork            SYS_fork_core
#define SYS_vfork           SYS_vfork_core
#define SYS_clone           SYS_clone_core
#define SYS_gettid          SYS_gettid_core
#define SYS_setpgid         SYS_setpgid_core
#define SYS_getpgid         SYS_getpgid_core
#define SYS_setsid          SYS_setsid_core
#define SYS_getsid          SYS_getsid_core
#define SYS_execve          SYS_execve_core
#define SYS_exit_group      SYS_exit_group_core
#define SYS_waitpid         SYS_waitpid_core
#define SYS_wait4           SYS_wait4_core

/* Signal syscalls */
#define SYS_kill            SYS_kill_core
#define SYS_sigaction       SYS_sigaction_core
#define SYS_sigprocmask     SYS_sigprocmask_core
#define SYS_sigreturn       SYS_sigreturn_core
#define SYS_sigsuspend      SYS_sigsuspend_core
#define SYS_sigpending      SYS_sigpending_core
#define SYS_tkill           SYS_tkill_core
#define SYS_tgkill          SYS_tgkill_core

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
#define SYS_service_register SYS_service_register_core

/* Network/Socket syscalls - must match kernel nr.h (400-420 range) */
#define SYS_socket          SYS_socket_core
#define SYS_socketpair      SYS_socketpair_core
#define SYS_bind            SYS_bind_core
#define SYS_listen          SYS_listen_core
#define SYS_accept          SYS_accept_core
#define SYS_accept4         SYS_accept4_core
#define SYS_connect         SYS_connect_core
#define SYS_sendto          SYS_sendto_core
#define SYS_recvfrom        SYS_recvfrom_core
#define SYS_sendmsg         SYS_sendmsg_core
#define SYS_recvmsg         SYS_recvmsg_core
#define SYS_getsockopt      SYS_getsockopt_core
#define SYS_setsockopt      SYS_setsockopt_core
#define SYS_shutdown        SYS_shutdown_core
#define SYS_getsockname     SYS_getsockname_core
#define SYS_getpeername     SYS_getpeername_core

/* epoll syscalls (420-422) */
#define SYS_epoll_create    SYS_epoll_create_core
#define SYS_epoll_ctl       SYS_epoll_ctl_core
#define SYS_epoll_wait      SYS_epoll_wait_core

/* Console syscalls */
#define SYS_console_takeover SYS_console_takeover_core  /* Request userspace takeover of console */
#define SYS_console_release  SYS_console_release_core   /* Release console back to kernel */

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
