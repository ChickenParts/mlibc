#ifndef _ABIBITS_SIGNAL_H
#define _ABIBITS_SIGNAL_H

#include <mlibc-config.h>

#include <abi-bits/pid_t.h>
#include <abi-bits/sigevent.h>
#include <abi-bits/sigset_t.h>
#include <abi-bits/uid_t.h>
#include <bits/ansi/clock_t.h>
#include <bits/size_t.h>
#include <bits/types.h>

#if defined(_DEFAULT_SOURCE) || (__MLIBC_POSIX1 && !__MLIBC_POSIX2024)
#define POLL_IN 1
#define POLL_OUT 2
#define POLL_MSG 3
#define POLL_ERR 4
#define POLL_PRI 5
#define POLL_HUP 6
#endif

/* struct taken from musl. */

typedef struct {
	int si_signo, si_errno, si_code;
	union {
		char __pad[128 - 2*sizeof(int) - sizeof(long)];
		struct {
			union {
				struct {
					pid_t si_pid;
					uid_t si_uid;
				} __piduid;
				struct {
					int si_timerid;
					int si_overrun;
				} __timer;
			} __first;
			union {
				union sigval si_value;
				struct {
					int si_status;
					clock_t si_utime, si_stime;
				} __sigchld;
			} __second;
		} __si_common;
		struct {
			void *si_addr;
			short si_addr_lsb;
			union {
				struct {
					void *si_lower;
					void *si_upper;
				} __addr_bnd;
				unsigned si_pkey;
			} __first;
		} __sigfault;
		struct {
			long si_band;
			int si_fd;
		} __sigpoll;
		struct {
			void *si_call_addr;
			int si_syscall;
			unsigned si_arch;
		} __sigsys;
	} __si_fields;
} siginfo_t;
#define si_pid     __si_fields.__si_common.__first.__piduid.si_pid
#define si_uid     __si_fields.__si_common.__first.__piduid.si_uid
#define si_status  __si_fields.__si_common.__second.__sigchld.si_status
#define si_utime   __si_fields.__si_common.__second.__sigchld.si_utime
#define si_stime   __si_fields.__si_common.__second.__sigchld.si_stime
#define si_value   __si_fields.__si_common.__second.si_value
#define si_addr    __si_fields.__sigfault.si_addr
#define si_addr_lsb __si_fields.__sigfault.si_addr_lsb
#define si_lower   __si_fields.__sigfault.__first.__addr_bnd.si_lower
#define si_upper   __si_fields.__sigfault.__first.__addr_bnd.si_upper
#define si_pkey    __si_fields.__sigfault.__first.si_pkey
#define si_band    __si_fields.__sigpoll.si_band
#define si_fd      __si_fields.__sigpoll.si_fd
#define si_timerid __si_fields.__si_common.__first.__timer.si_timerid
#define si_overrun __si_fields.__si_common.__first.__timer.si_overrun
#define si_ptr     si_value.sival_ptr
#define si_int     si_value.sival_int
#define si_call_addr __si_fields.__sigsys.si_call_addr
#define si_syscall __si_fields.__sigsys.si_syscall
#define si_arch    __si_fields.__sigsys.si_arch

/* Required for sys_sigaction sysdep. */
#define SA_NOCLDSTOP 1
#define SA_NOCLDWAIT 2
#define SA_SIGINFO 4
#define SA_ONSTACK 0x08000000
#define SA_RESTART 0x10000000
#define SA_NODEFER 0x40000000
#define SA_RESETHAND 0x80000000
#define SA_RESTORER 0x04000000

/* SA_NOMASK is an alias for SA_NODEFER */
/* SA_ONESHOT is an alias for SA_RESETHAND */
#define SA_NOMASK SA_NODEFER
#define SA_ONESHOT SA_RESETHAND

#ifdef __cplusplus
extern "C" {
#endif

/* Argument for signal() */
typedef void (*__sighandler) (int);

#define SIG_ERR ((__sighandler)(void *)(-1))
#define SIG_DFL ((__sighandler)(void *)(0))
#define SIG_IGN ((__sighandler)(void *)(1))

#define SIGABRT 6
#define SIGFPE 8
#define SIGILL 4
#define SIGINT 2
#define SIGSEGV 11
#define SIGTERM 15
#define SIGPROF 27
#define SIGIO 29
#define SIGPWR 30
#define SIGRTMIN 35
#define SIGRTMAX 64

/* constants for sigprocmask() */
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SIGHUP    1
#define SIGQUIT   3
#define SIGTRAP   5
#define SIGIOT    SIGABRT
#define SIGBUS    7
#define SIGKILL   9
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGWINCH  28
#define SIGPOLL   29
#define SIGSYS    31
#define SIGUNUSED SIGSYS
#define SIGCANCEL 32
#define SIGTIMER  33

#if defined(_DEFAULT_SOURCE) || __MLIBC_XOPEN
#define MINSIGSTKSZ 2048
#define SIGSTKSZ 8192
#define SS_ONSTACK 1
#define SS_DISABLE 2
#endif

typedef struct __stack {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} stack_t;

/* constants for sigev_notify of struct sigevent */
#define SIGEV_SIGNAL 0
#define SIGEV_NONE 1
#define SIGEV_THREAD 2
#define SIGEV_THREAD_ID 4

#define SEGV_MAPERR 1
#define SEGV_ACCERR 2

#define BUS_ADRALN 1
#define BUS_ADRERR 2
#define BUS_OBJERR 3
#define BUS_MCEERR_AR 4
#define BUS_MCEERR_AO 5

#define ILL_ILLOPC 1
#define ILL_ILLOPN 2
#define ILL_ILLADR 3
#define ILL_ILLTRP 4
#define ILL_PRVOPC 5
#define ILL_PRVREG 6
#define ILL_COPROC 7
#define ILL_BADSTK 8
#define ILL_BADIADDR 9

#define _NSIG 65
#if defined(_DEFAULT_SOURCE)
#define NSIG _NSIG
#endif

#define SI_ASYNCNL (-60)
#define SI_TKILL (-6)
#define SI_SIGIO (-5)
#define SI_ASYNCIO (-4)
#define SI_MESGQ (-3)
#define SI_TIMER (-2)
#define SI_QUEUE (-1)
#define SI_USER 0
#define SI_KERNEL 128

#include <bits/threads.h>

struct sigaction {
	union {
		void (*sa_handler)(int);
		void (*sa_sigaction)(int, siginfo_t *, void *);
	} __sa_handler;
	unsigned long sa_flags;
	void (*sa_restorer)(void);
	sigset_t sa_mask;
};

#define sa_handler __sa_handler.sa_handler
#define sa_sigaction __sa_handler.sa_sigaction

/*
 * ucontext_t: what a SA_SIGINFO handler receives as its third argument.
 *
 * ChickenOS-native, NOT Linux's: no x87/SSE state, trap numbers, CR2 or
 * "oldmask" padding.  This is a MIRROR of the kernel's definition in
 * include/chickenos/ucontext.h (this tree is built before the sysroot receives
 * the kernel's uapi headers, so it cannot include it).  The register-slot
 * numbers and the sizes asserted below are the contract; the kernel asserts the
 * same numbers and has a ktest (signal.frame_layout) over its side.  See
 * signal(7) and sigaction(2) in docs/man.
 *
 * Handlers may rewrite uc_mcontext.gregs[] (e.g. CHICKEN_GREG_PC to skip a
 * faulting instruction) and uc_sigmask; sigreturn resumes with the edited
 * values.  FP/SIMD state is not saved, exposed or restored.
 */
#ifndef CHICKEN_NGREG
#if defined(__x86_64__)
#define CHICKEN_GREG_R15 0
#define CHICKEN_GREG_R14 1
#define CHICKEN_GREG_R13 2
#define CHICKEN_GREG_R12 3
#define CHICKEN_GREG_R11 4
#define CHICKEN_GREG_R10 5
#define CHICKEN_GREG_R9 6
#define CHICKEN_GREG_R8 7
#define CHICKEN_GREG_RDI 8
#define CHICKEN_GREG_RSI 9
#define CHICKEN_GREG_RBP 10
#define CHICKEN_GREG_RSP 11
#define CHICKEN_GREG_RBX 12
#define CHICKEN_GREG_RDX 13
#define CHICKEN_GREG_RCX 14
#define CHICKEN_GREG_RAX 15
#define CHICKEN_GREG_RIP 16
#define CHICKEN_GREG_RFLAGS 17
#define CHICKEN_NGREG 18
#define CHICKEN_GREG_PC CHICKEN_GREG_RIP
#define CHICKEN_GREG_SP CHICKEN_GREG_RSP
#elif defined(__i386__)
#define CHICKEN_GREG_EDI 0
#define CHICKEN_GREG_ESI 1
#define CHICKEN_GREG_EBP 2
#define CHICKEN_GREG_ESP 3
#define CHICKEN_GREG_EBX 4
#define CHICKEN_GREG_EDX 5
#define CHICKEN_GREG_ECX 6
#define CHICKEN_GREG_EAX 7
#define CHICKEN_GREG_EIP 8
#define CHICKEN_GREG_EFLAGS 9
#define CHICKEN_GREG_DS 10
#define CHICKEN_GREG_ES 11
#define CHICKEN_GREG_FS 12
#define CHICKEN_GREG_GS 13
#define CHICKEN_NGREG 14
#define CHICKEN_GREG_PC CHICKEN_GREG_EIP
#define CHICKEN_GREG_SP CHICKEN_GREG_ESP
#elif defined(__aarch64__)
#define CHICKEN_GREG_X0 0
#define CHICKEN_GREG_SP 31
#define CHICKEN_GREG_PC 32
#define CHICKEN_GREG_PSTATE 33
#define CHICKEN_NGREG 34
#elif defined(__riscv)
#define CHICKEN_GREG_PC 0
#define CHICKEN_GREG_RA 1
#define CHICKEN_GREG_SP 2
#define CHICKEN_GREG_A0 10
#define CHICKEN_NGREG 32
#else
#error "Missing architecture specific code."
#endif
#endif /* CHICKEN_NGREG */

typedef struct {
	unsigned long gregs[CHICKEN_NGREG];
} mcontext_t;

typedef struct __ucontext {
	unsigned long uc_flags;
	struct __ucontext *uc_link;
	stack_t uc_stack;
	sigset_t uc_sigmask;
	mcontext_t uc_mcontext;
} ucontext_t;

#if defined(__cplusplus)
#define __CHICKEN_SA static_assert
#else
#define __CHICKEN_SA _Static_assert
#endif
#if defined(__i386__)
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_sigmask) == 20, "ucontext sigmask offset");
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_mcontext) == 148, "ucontext mcontext offset");
__CHICKEN_SA(sizeof(ucontext_t) == 204, "ucontext size");
#elif defined(__x86_64__)
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_sigmask) == 40, "ucontext sigmask offset");
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_mcontext) == 168, "ucontext mcontext offset");
__CHICKEN_SA(sizeof(ucontext_t) == 312, "ucontext size");
#elif defined(__aarch64__)
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_sigmask) == 40, "ucontext sigmask offset");
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_mcontext) == 168, "ucontext mcontext offset");
__CHICKEN_SA(sizeof(ucontext_t) == 440, "ucontext size");
#elif defined(__riscv) && __riscv_xlen == 32
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_sigmask) == 20, "ucontext sigmask offset");
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_mcontext) == 148, "ucontext mcontext offset");
__CHICKEN_SA(sizeof(ucontext_t) == 276, "ucontext size");
#else /* riscv64 */
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_sigmask) == 40, "ucontext sigmask offset");
__CHICKEN_SA(__builtin_offsetof(ucontext_t, uc_mcontext) == 168, "ucontext mcontext offset");
__CHICKEN_SA(sizeof(ucontext_t) == 424, "ucontext size");
#endif
__CHICKEN_SA(sizeof(siginfo_t) == 128, "siginfo_t size");
#undef __CHICKEN_SA

#ifdef __cplusplus
}
#endif

#endif /* _ABIBITS_SIGNAL_H */
