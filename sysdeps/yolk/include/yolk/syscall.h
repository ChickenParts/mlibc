/*
 * Yolk syscall ABI for mlibc.
 * SPDX-License-Identifier: MIT
 *
 * This header intentionally selects the compact Core-Yolk aliases whenever
 * one exists. Keep it synchronized with Yolk include/kernel/syscall/nr.h.
 */
#ifndef _YOLK_SYSCALL_H
#define _YOLK_SYSCALL_H

#include <stdint.h>

#define SYS_write 1
#define SYS_exit 2
#define SYS_read 3
#define SYS_stat 4
#define SYS_fstat 5
#define SYS_lstat 6
#define SYS_poll 7
#define SYS_lseek 8
#define SYS_mmap 9
#define SYS_mprotect 10
#define SYS_munmap 11
#define SYS_ioctl 16
#define SYS_yield 24
#define SYS_getpid 25
#define SYS_getppid 26
#define SYS_gettid 27
#define SYS_getuid 28
#define SYS_geteuid 29
#define SYS_getgid 30
#define SYS_getegid 31
#define SYS_dup 32
#define SYS_dup2 33
#define SYS_dup3 34
#define SYS_pipe 35
#define SYS_pipe2 36
#define SYS_open 37
#define SYS_close 38
#define SYS_clock_gettime 39
#define SYS_clock_getres 40
#define SYS_nanosleep 41
#define SYS_gettimeofday 42
#define SYS_fork 43
#define SYS_execve 44
#define SYS_exit_group 45
#define SYS_waitpid 46
#define SYS_wait4 47
#define SYS_setpgid 48
#define SYS_getpgid 49
#define SYS_setsid 50
#define SYS_getsid 51
#define SYS_kill 52
#define SYS_sigaction 53
#define SYS_sigprocmask 54
#define SYS_sigreturn 55
#define SYS_sigsuspend 56
#define SYS_sigpending 57
#define SYS_tgkill 58
#define SYS_clone 59
#define SYS_vfork 60
#define SYS_tkill 61
#define SYS_sigtimedwait 62
#define SYS_uname 63
#define SYS_fstatat 64
#define SYS_mkdirat 65
#define SYS_unlinkat 66
#define SYS_renameat 67
#define SYS_fchmodat 68
#define SYS_faccessat 69
#define SYS_utimensat 70
#define SYS_fcntl 72
#define SYS_fsync 74
#define SYS_truncate 76
#define SYS_ftruncate 77
#define SYS_getdents 78
#define SYS_rename 82
#define SYS_mkdir 83
#define SYS_rmdir 84
#define SYS_link 86
#define SYS_unlink 87
#define SYS_symlink 88
#define SYS_readlink 89
#define SYS_chmod 90
#define SYS_fchmod 91
#define SYS_chown 92
#define SYS_fchown 93
#define SYS_access 94
#define SYS_socket 115
#define SYS_socketpair 116
#define SYS_bind 117
#define SYS_listen 118
#define SYS_accept 119
#define SYS_accept4 120
#define SYS_connect 121
#define SYS_sendto 122
#define SYS_recvfrom 123
#define SYS_sendmsg 124
#define SYS_recvmsg 125
#define SYS_getsockopt 126
#define SYS_setsockopt 127
#define SYS_shutdown 128
#define SYS_getsockname 129
#define SYS_getpeername 130
#define SYS_epoll_create 131
#define SYS_epoll_ctl 132
#define SYS_epoll_wait 133
#define SYS_pselect 134
#define SYS_ppoll 135
#define SYS_statfs 137
#define SYS_fstatfs 163
#define SYS_brk 164
#define SYS_mount 165
#define SYS_umount 166
#define SYS_clock_settime 167
#define SYS_getcwd 169
#define SYS_chdir 170
#define SYS_fchdir 171
#define SYS_msync 189
#define SYS_futex_wait 224
#define SYS_futex_wake 225
#define SYS_futex 228
#define SYS_utimes 235
#define SYS_arch_prctl 352

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

/* The recovered legacy translation unit provided these routines directly.
 * Rename them there so recovery.cpp can publish the syscall-mediated contract. */
#define sys_tcb_set yolk_legacy_sys_tcb_set
#define sys_tcb_get yolk_legacy_sys_tcb_get

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__x86_64__)
static inline long __syscall0(long n) {
	long r; __asm__ volatile("syscall" : "=a"(r) : "a"(n)
		: "rcx", "r11", "memory", "cc"); return r;
}
static inline long __syscall1(long n, long a0) {
	long r; __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a0)
		: "rcx", "r11", "memory", "cc"); return r;
}
static inline long __syscall2(long n, long a0, long a1) {
	long r; __asm__ volatile("syscall" : "=a"(r)
		: "a"(n), "D"(a0), "S"(a1) : "rcx", "r11", "memory", "cc"); return r;
}
static inline long __syscall3(long n, long a0, long a1, long a2) {
	long r; __asm__ volatile("syscall" : "=a"(r)
		: "a"(n), "D"(a0), "S"(a1), "d"(a2)
		: "rcx", "r11", "memory", "cc"); return r;
}
static inline long __syscall4(long n, long a0, long a1, long a2, long a3) {
	register long r10 __asm__("r10") = a3; long r;
	__asm__ volatile("syscall" : "=a"(r)
		: "a"(n), "D"(a0), "S"(a1), "d"(a2), "r"(r10)
		: "rcx", "r11", "memory", "cc"); return r;
}
static inline long __syscall5(long n, long a0, long a1, long a2, long a3,
		long a4) {
	register long r10 __asm__("r10") = a3;
	register long r8 __asm__("r8") = a4; long r;
	__asm__ volatile("syscall" : "=a"(r)
		: "a"(n), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8)
		: "rcx", "r11", "memory", "cc"); return r;
}
static inline long __syscall6(long n, long a0, long a1, long a2, long a3,
		long a4, long a5) {
	register long r10 __asm__("r10") = a3;
	register long r8 __asm__("r8") = a4;
	register long r9 __asm__("r9") = a5; long r;
	__asm__ volatile("syscall" : "=a"(r)
		: "a"(n), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8), "r"(r9)
		: "rcx", "r11", "memory", "cc"); return r;
}
#elif defined(__aarch64__)
static inline long __syscall6(long n, long a0v, long a1v, long a2v, long a3v,
		long a4v, long a5v) {
	register long x0 __asm__("x0") = a0v;
	register long x1 __asm__("x1") = a1v;
	register long x2 __asm__("x2") = a2v;
	register long x3 __asm__("x3") = a3v;
	register long x4 __asm__("x4") = a4v;
	register long x5 __asm__("x5") = a5v;
	register long x8 __asm__("x8") = n;
	__asm__ volatile("svc #0" : "+r"(x0)
		: "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
		: "memory", "cc"); return x0;
}
#define YOLK_SYSCALL_SMALL(N) \
static inline long __syscall##N(long n, long a0, long a1, long a2, long a3, long a4) \
{ return __syscall6(n, a0, a1, a2, a3, a4, 0); }
static inline long __syscall0(long n) { return __syscall6(n, 0, 0, 0, 0, 0, 0); }
static inline long __syscall1(long n, long a0) { return __syscall6(n, a0, 0, 0, 0, 0, 0); }
static inline long __syscall2(long n, long a0, long a1) { return __syscall6(n, a0, a1, 0, 0, 0, 0); }
static inline long __syscall3(long n, long a0, long a1, long a2) { return __syscall6(n, a0, a1, a2, 0, 0, 0); }
static inline long __syscall4(long n, long a0, long a1, long a2, long a3) { return __syscall6(n, a0, a1, a2, a3, 0, 0); }
static inline long __syscall5(long n, long a0, long a1, long a2, long a3, long a4) { return __syscall6(n, a0, a1, a2, a3, a4, 0); }
#elif defined(__riscv) && __riscv_xlen == 64
static inline long __syscall6(long n, long a0v, long a1v, long a2v, long a3v,
		long a4v, long a5v) {
	register long a0 __asm__("a0") = a0v;
	register long a1 __asm__("a1") = a1v;
	register long a2 __asm__("a2") = a2v;
	register long a3 __asm__("a3") = a3v;
	register long a4 __asm__("a4") = a4v;
	register long a5 __asm__("a5") = a5v;
	register long a7 __asm__("a7") = n;
	__asm__ volatile("ecall" : "+r"(a0)
		: "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a7)
		: "memory"); return a0;
}
static inline long __syscall0(long n) { return __syscall6(n, 0, 0, 0, 0, 0, 0); }
static inline long __syscall1(long n, long a0) { return __syscall6(n, a0, 0, 0, 0, 0, 0); }
static inline long __syscall2(long n, long a0, long a1) { return __syscall6(n, a0, a1, 0, 0, 0, 0); }
static inline long __syscall3(long n, long a0, long a1, long a2) { return __syscall6(n, a0, a1, a2, 0, 0, 0); }
static inline long __syscall4(long n, long a0, long a1, long a2, long a3) { return __syscall6(n, a0, a1, a2, a3, 0, 0); }
static inline long __syscall5(long n, long a0, long a1, long a2, long a3, long a4) { return __syscall6(n, a0, a1, a2, a3, a4, 0); }
#else
#error "Unsupported Yolk architecture"
#endif

#ifdef __cplusplus
}
#endif

#endif
