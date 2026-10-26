/**
 * @file thread.cpp
 * @brief ChickenOS mlibc threading support.
 *
 * Implements sys_prepare_stack() and sys_clone() for mlibc's pthread
 * implementation. The thread entry trampoline __mlibc_enter_thread() is
 * called as the first thing in a new thread's life.
 */

#include <mlibc/thread-entry.hpp>
#include <mlibc/all-sysdeps.hpp>
#include <mlibc/thread.hpp>
#include <bits/ensure.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <chickenos/syscall.hpp>

/*
 * ChickenOS clone flags — these must match the kernel's definitions
 * in include/chicken/thread.h or similar.
 */
#define CLONE_VM            0x00000100
#define CLONE_FS            0x00000200
#define CLONE_FILES         0x00000400
#define CLONE_SIGHAND       0x00000800
#define CLONE_THREAD        0x00010000
#define CLONE_SYSVSEM       0x00040000
#define CLONE_SETTLS        0x00080000
#define CLONE_PARENT_SETTID 0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000

#if defined(__riscv) && __riscv_xlen == 32
extern "C" long __mlibc_clone_riscv32(unsigned long flags, void *stack, void *ptid, void *tls, void *ctid);
#endif

#if defined(__i386__)
extern "C" long __mlibc_clone_i386(unsigned long flags, void *stack, void *ptid, void *tls_desc, void *ctid);
#endif

#if defined(__powerpc__)
extern "C" long __mlibc_clone_powerpc(unsigned long flags, void *stack, void *ptid, void *tls_desc, void *ctid);
#endif

/**
 * Thread entry trampoline — called by the new thread after the kernel
 * starts it. The kernel has already set the TCB via CLONE_SETTLS.
 */
extern "C" void __mlibc_enter_thread(void *entry, void *user_arg) {
	auto tcb = mlibc::get_current_tcb();

	/* Wait until our parent sets up the TID. */
	while(!__atomic_load_n(&tcb->tid, __ATOMIC_RELAXED))
		mlibc::sysdep<FutexWait>(&tcb->tid, 0, nullptr);

	tcb->invokeThreadFunc(entry, user_arg);

	__atomic_store_n(&tcb->didExit, 1, __ATOMIC_RELEASE);
	mlibc::sysdep<FutexWake>(&tcb->didExit, true);

	mlibc::sysdep<ThreadExit>();
}

namespace mlibc {

static constexpr size_t default_stacksize = 0x200000; /* 2 MiB */

/**
 * Prepare a stack for a new thread.
 *
 * If *stack is null, allocates a new stack with mmap (with a guard page).
 * If *stack is already set, uses it as-is (caller-provided stack).
 *
 * Pushes the entry point and user_arg onto the top of the stack so that
 * __mlibc_enter_thread can pop them.
 */
int Sysdeps<PrepareStack>::operator()(void **stack, void *entry, void *user_arg, void *tcb,
		size_t *stack_size, size_t *guard_size, void **stack_base) {
	(void)tcb;
	if(!*stack_size)
		*stack_size = default_stacksize;

	uintptr_t map;
	if(*stack) {
		map = reinterpret_cast<uintptr_t>(*stack);
		*guard_size = 0;
	} else {
		map = reinterpret_cast<uintptr_t>(
				mmap(nullptr, *stack_size + *guard_size,
					PROT_NONE,
					MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
				);
		if(reinterpret_cast<void*>(map) == MAP_FAILED)
			return EAGAIN;
		int ret = mprotect(reinterpret_cast<void*>(map + *guard_size), *stack_size,
				PROT_READ | PROT_WRITE);
		if(ret)
			return EAGAIN;
	}

	*stack_base = reinterpret_cast<void*>(map);
	auto sp = reinterpret_cast<uintptr_t*>(map + *guard_size + *stack_size);
	*--sp = reinterpret_cast<uintptr_t>(user_arg);
	*--sp = reinterpret_cast<uintptr_t>(entry);
	*stack = reinterpret_cast<void*>(sp);
	return 0;
}

/**
 * Create a new thread via ChickenOS's clone syscall.
 *
 * The new thread shares the address space (CLONE_VM), file descriptors
 * (CLONE_FILES), filesystem info (CLONE_FS), signal handlers (CLONE_SIGHAND),
 * and SysV semaphore undo (CLONE_SYSVSEM). The TLS pointer is set via
 * CLONE_SETTLS and the parent gets the child TID via CLONE_PARENT_SETTID.
 */
int Sysdeps<Clone>::operator()(void *tcb, pid_t *pid_out, void *stack) {
	unsigned long flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND
		| CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS
		| CLONE_PARENT_SETTID;

#if defined(__i386__)
	/* i386: CLONE_SETTLS takes a pointer to a struct chicken_tls_desc
	 * (<chickenos/tls.h>; flags/reserved must be zero).  The kernel copies it
	 * during the call, so a stack object is fine, and the child inherits the
	 * fixed %gs selector, so none is passed.  The child must not return into
	 * this function on its new stack, so the syscall and the child's jump to
	 * __mlibc_enter_thread live in __mlibc_clone_i386 (x86/arch-syscall.cpp). */
	struct chicken_tls_desc desc = {
		.base = reinterpret_cast<unsigned int>(tcb),
		.flags = 0,
		.selector = 0,
		.reserved = 0,
	};
	auto ret = static_cast<sc_result_t>(__mlibc_clone_i386(flags, stack, pid_out, &desc, nullptr));
#elif defined(__powerpc__)
	/* PPC32 ChickenOS and the PPC32 ELF TLS ABI use r2 as TP, at a +0x7000
	 * bias from the start of the static TLS block. mlibc places that block
	 * immediately after the TCB. The kernel clone ABI accepts the same
	 * descriptor as set_thread_area and installs desc.base into r2. */
	struct chicken_tls_desc desc = {
		.base = reinterpret_cast<unsigned int>(reinterpret_cast<char *>(tcb) + sizeof(Tcb) + 0x7000),
		.flags = 0,
		.selector = 0,
		.reserved = 0,
	};
	auto ret = static_cast<sc_result_t>(__mlibc_clone_powerpc(flags, stack, pid_out, &desc, nullptr));
#elif defined(__x86_64__)
	/* x86_64: FS base is set from the tcb pointer directly */
	auto ret = do_syscall(SYS_clone, flags, stack, pid_out, tcb, nullptr);
#elif defined(__aarch64__)
	/* aarch64: TP should point 16 bytes before end of TCB */
	auto tp = reinterpret_cast<char *>(tcb) + sizeof(Tcb) - 0x10;
	auto ret = do_syscall(SYS_clone, flags, stack, pid_out, reinterpret_cast<void *>(tp), nullptr);
#elif defined(__riscv) && __riscv_xlen == 32
	/* riscv32: TP should point to the address immediately after the TCB, and the
	 * kernel installs the CLONE_SETTLS value as the child's tp.  The child must
	 * not return into this function on its new stack, so the ecall and the
	 * child's jump to __mlibc_enter_thread live in __mlibc_clone_riscv32
	 * (riscv32/arch-syscall.cpp). */
	auto tls = reinterpret_cast<char *>(tcb) + sizeof(Tcb);
	auto ret = static_cast<sc_result_t>(__mlibc_clone_riscv32(flags, stack, pid_out, tls, nullptr));
#elif defined(__riscv)
	/* riscv64: TP should point to address immediately after TCB */
	auto tls = reinterpret_cast<char *>(tcb) + sizeof(Tcb);
	auto ret = do_syscall(SYS_clone, flags, stack, pid_out, reinterpret_cast<void *>(tls), nullptr);
#else
	auto ret = do_syscall(SYS_clone, flags, stack, pid_out, tcb, nullptr);
#endif

	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

} // namespace mlibc
