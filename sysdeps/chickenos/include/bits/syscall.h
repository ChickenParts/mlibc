#ifndef _BITS_SYSCALL_H
#define _BITS_SYSCALL_H

/*
 * ChickenOS syscall numbers — unified across all architectures.
 * See syscall(7) for the full specification.
 */

/* 0x000-0x03F: Core File I/O */
#define SYS_read            0x00
#define SYS_write           0x01
#define SYS_open            0x02
#define SYS_close           0x03
#define SYS_stat64          0x404
#define SYS_fstat64         0x405
#define SYS_lstat64         0x406
#define SYS_lseek           0x07
#define SYS_pread64         0x408
#define SYS_pwrite64        0x409
#define SYS_readv           0x0A
#define SYS_writev          0x0B
#define SYS_access          0x0C
#define SYS_dup             0x0D
#define SYS_dup2            0x0E
#define SYS_dup3            0x0F
#define SYS_fcntl64         0x410
#define SYS_ioctl           0x11
#define SYS_flock           0x12
#define SYS_fsync           0x13
#define SYS_fdatasync       0x14
#define SYS_truncate        0x15
#define SYS_ftruncate       0x16

/* 0x040-0x07F: Pipes & FD Types */
#define SYS_pipe            0x40
#define SYS_pipe2           0x41
#define SYS_eventfd         0x42
#define SYS_eventfd2        0x43
#define SYS_timerfd_create  0x44
#define SYS_timerfd_settime 0x45
#define SYS_timerfd_gettime 0x46
#define SYS_signalfd        0x47
#define SYS_signalfd4       0x48
#define SYS_memfd_create    0x49

/* 0x080-0x0BF: Directory & Path Operations */
#define SYS_mkdir           0x80
#define SYS_rmdir           0x81
#define SYS_chdir           0x82
#define SYS_fchdir          0x83
#define SYS_getcwd          0x84
#define SYS_getdents64      0x485
#define SYS_unlink          0x86
#define SYS_rename          0x87
#define SYS_link            0x88
#define SYS_symlink         0x89
#define SYS_readlink        0x8A
#define SYS_chmod           0x8B
#define SYS_fchmod          0x8C
#define SYS_chown           0x8D
#define SYS_fchown          0x8E
#define SYS_lchown          0x8F
#define SYS_umask           0x90
/* *at variants */
#define SYS_openat          0xA0
#define SYS_mkdirat         0xA1
#define SYS_unlinkat        0xA2
#define SYS_renameat        0xA3
#define SYS_linkat          0xA4
#define SYS_symlinkat       0xA5
#define SYS_readlinkat      0xA6
#define SYS_fchmodat        0xA7
#define SYS_fchownat        0xA8
#define SYS_faccessat       0xA9
#define SYS_renameat2       0xAA

/* 0x0C0-0x0FF: Memory Management */
#define SYS_mmap            0xC0
#define SYS_munmap          0xC1
#define SYS_mprotect        0xC2
#define SYS_brk             0xC3
#define SYS_mremap          0xC4
#define SYS_msync           0xC5
#define SYS_madvise         0xC6
#define SYS_mlock           0xC7
#define SYS_munlock         0xC8
#define SYS_mlockall        0xC9
#define SYS_munlockall      0xCA

/* 0x100-0x13F: Process Control */
#define SYS_fork            0x100
#define SYS_execve          0x102
#define SYS_exit            0x103
#define SYS_exit_group      0x104
#define SYS_wait4           0x105
#define SYS_waitid          0x106
#define SYS_clone           0x107
#define SYS_getpid          0x109
#define SYS_getppid         0x10A
#define SYS_gettid          0x10B
#define SYS_getpgrp         0x10C
#define SYS_setpgid         0x10D
#define SYS_getpgid         0x10E
#define SYS_setsid          0x10F
#define SYS_getsid          0x110
/* uid/gid ops */
#define SYS_getuid          0x120
#define SYS_setuid          0x121
#define SYS_geteuid         0x122
#define SYS_getgid          0x123
#define SYS_setgid          0x124
#define SYS_getegid         0x125
#define SYS_getgroups       0x126
#define SYS_setgroups       0x127

/* 0x140-0x17F: Signals */
#define SYS_kill            0x140
#define SYS_tkill           0x141
#define SYS_tgkill          0x142
#define SYS_rt_sigaction    0x143
#define SYS_rt_sigprocmask  0x144
#define SYS_rt_sigpending   0x145
#define SYS_rt_sigsuspend   0x146
#define SYS_rt_sigreturn    0x147
#define SYS_sigaltstack     0x148
#define SYS_alarm           0x160
#define SYS_pause           0x161

/* 0x180-0x1BF: I/O Multiplexing */
#define SYS_poll            0x180
#define SYS_ppoll           0x181
#define SYS_select          0x182
#define SYS_pselect6        0x183
#define SYS_epoll_create    0x184
#define SYS_epoll_create1   0x185
#define SYS_epoll_ctl       0x186
#define SYS_epoll_wait      0x187
#define SYS_epoll_pwait     0x188

/* 0x1C0-0x1FF: Sockets */
#define SYS_socket          0x1C0
#define SYS_socketpair      0x1C1
#define SYS_bind            0x1C2
#define SYS_listen          0x1C3
#define SYS_accept          0x1C4
#define SYS_accept4         0x1C5
#define SYS_connect         0x1C6
#define SYS_sendto          0x1C7
#define SYS_recvfrom        0x1C8
#define SYS_sendmsg         0x1C9
#define SYS_recvmsg         0x1CA
#define SYS_shutdown        0x1CB
#define SYS_getsockname     0x1CC
#define SYS_getpeername     0x1CD
#define SYS_setsockopt      0x1CE
#define SYS_getsockopt      0x1CF

/* 0x200-0x23F: Time & Timers */
#define SYS_gettimeofday    0x200
#define SYS_settimeofday    0x201
#define SYS_clock_gettime   0x202
#define SYS_clock_settime   0x203
#define SYS_clock_getres    0x204
#define SYS_clock_nanosleep 0x205
#define SYS_nanosleep       0x206
#define SYS_timer_create    0x207
#define SYS_timer_settime   0x208
#define SYS_timer_gettime   0x209
#define SYS_timer_getoverrun 0x20A
#define SYS_timer_delete    0x20B
#define SYS_setitimer       0x20C
#define SYS_getitimer       0x20D

/* 0x240-0x27F: System Info & Resources */
#define SYS_uname           0x240
#define SYS_sysinfo         0x241
#define SYS_getrandom       0x242
#define SYS_getrlimit       0x243
#define SYS_setrlimit       0x244
#define SYS_prlimit64       0x645
#define SYS_getrusage       0x246
#define SYS_times           0x247
#define SYS_getpriority     0x248
#define SYS_setpriority     0x249
#define SYS_sched_yield     0x24A
#define SYS_reboot          0x24B
#define SYS_futex           0x260

/* 0x280-0x2BF: Mount & Filesystem */
#define SYS_mount           0x280
#define SYS_umount2         0x281
#define SYS_statfs          0x282
#define SYS_fstatfs         0x283
#define SYS_sync            0x284
#define SYS_syncfs          0x285

/* 0x2C0-0x2FF: Terminal */
#define SYS_tcgetattr       0x2C0
#define SYS_tcsetattr       0x2C1
#define SYS_tcgetwinsize    0x2C2
#define SYS_tcsetwinsize    0x2C3
#define SYS_isatty          0x2C4

/* 0x300-0x37F: Architecture-Specific */
#define SYS_arch_prctl      0x300
#define SYS_set_thread_area 0x301
#define SYS_get_thread_area 0x302
#define SYS_set_tid_address 0x303

/* 0x380-0x3BF: Debug & Introspection */
#define SYS_ptrace          0x380

/* Syscall dispatch functions (defined in arch-specific arch-syscall.cpp) */
#ifdef __cplusplus
extern "C" {
#endif

typedef long __sc_word_t;

__sc_word_t __do_syscall0(long sc);
__sc_word_t __do_syscall1(long sc, __sc_word_t arg1);
__sc_word_t __do_syscall2(long sc, __sc_word_t arg1, __sc_word_t arg2);
__sc_word_t __do_syscall3(long sc, __sc_word_t arg1, __sc_word_t arg2,
                          __sc_word_t arg3);
__sc_word_t __do_syscall4(long sc, __sc_word_t arg1, __sc_word_t arg2,
                          __sc_word_t arg3, __sc_word_t arg4);
__sc_word_t __do_syscall5(long sc, __sc_word_t arg1, __sc_word_t arg2,
                          __sc_word_t arg3, __sc_word_t arg4,
                          __sc_word_t arg5);
__sc_word_t __do_syscall6(long sc, __sc_word_t arg1, __sc_word_t arg2,
                          __sc_word_t arg3, __sc_word_t arg4,
                          __sc_word_t arg5, __sc_word_t arg6);

#ifdef __cplusplus
}
#endif

#endif /* _BITS_SYSCALL_H */
