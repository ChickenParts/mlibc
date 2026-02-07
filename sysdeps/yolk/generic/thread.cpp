/*
 * Yolk sysdeps for mlibc - Thread support
 * SPDX-License-Identifier: MIT
 *
 * Implements sys_clone, sys_prepare_stack, and sys_thread_exit for pthreads.
 */

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>

#include <bits/ensure.h>
#include <mlibc/all-sysdeps.hpp>
#include <mlibc/tcb.hpp>

#include <yolk/syscall.h>

/* Clone flags for threading (must match kernel) */
#define CLONE_VM             0x00000100
#define CLONE_FS             0x00000200
#define CLONE_FILES          0x00000400
#define CLONE_SIGHAND        0x00000800
#define CLONE_THREAD         0x00010000
#define CLONE_PARENT_SETTID  0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000
#define CLONE_SETTLS         0x00080000

namespace mlibc {

/* Default stack size for threads (2MB) */
static constexpr size_t default_stacksize = 0x200000;

/*
 * Thread entry point.
 * Called by the new thread after clone returns.
 * The stack was set up by sys_prepare_stack with:
 *   [stack+0] = entry function
 *   [stack+8] = user argument
 *   [stack+16] = tcb pointer
 */
extern "C" void __mlibc_thread_entry();

/*
 * Assembly trampoline that:
 * 1. Pops entry, arg, tcb from stack
 * 2. Sets up TLS (architecture-specific)
 * 3. Calls the entry function
 * 4. Calls sys_thread_exit when entry returns
 */

[[noreturn]] void sys_thread_exit() {
    __syscall1(SYS_exit, 0);
    __builtin_unreachable();
}

int sys_clone(void *tcb, pid_t *tid_out, void *stack) {
    /*
     * Clone flags for pthreads:
     * - CLONE_VM: Share address space
     * - CLONE_FS: Share filesystem info
     * - CLONE_FILES: Share file descriptors
     * - CLONE_SIGHAND: Share signal handlers
     * - CLONE_THREAD: Same thread group
     * - CLONE_SETTLS: Set TLS pointer
     * - CLONE_PARENT_SETTID: Store TID in parent
     * - CLONE_CHILD_CLEARTID: Clear TID on exit (for futex wake)
     */
    unsigned long flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                          CLONE_THREAD | CLONE_SETTLS | CLONE_PARENT_SETTID |
                          CLONE_CHILD_CLEARTID;

    /*
     * Clone syscall using inline assembly.
     * When clone returns in the child, the stack pointer is already set to
     * the prepared stack, so we can't use normal C code (stack frame is wrong).
     * The inline asm checks return value and jumps to entry point if child.
     */
    long result;

#if defined(__x86_64__)
    /*
     * x86_64 syscall ABI:
     * rax = syscall number
     * rdi = arg1 (flags)
     * rsi = arg2 (stack)
     * rdx = arg3 (parent_tid)
     * r10 = arg4 (child_tid)
     * r8  = arg5 (tls)
     *
     * Return: rax = result (child TID in parent, 0 in child, negative on error)
     */
    register unsigned long r8 __asm__("r8") = (unsigned long)tcb;
    register unsigned long r10 __asm__("r10") = (unsigned long)tid_out;

    __asm__ volatile(
        "syscall\n\t"
        "testq %%rax, %%rax\n\t"       /* Check if child (rax == 0) */
        "jnz 1f\n\t"                    /* If parent, skip to 1 */
        /* Child path: jump to thread entry */
        "jmp __mlibc_thread_entry\n\t"
        "1:\n\t"                        /* Parent continues here */
        : "=a"(result)
        : "a"((long)SYS_clone), "D"(flags), "S"(stack),
          "d"((unsigned long)tid_out), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory", "cc"
    );
#elif defined(__aarch64__)
    /* TODO: aarch64 implementation */
    result = __syscall5(SYS_clone, flags, (long)stack,
                        0, (long)tid_out, (long)tcb);
    if (result == 0) {
        __mlibc_thread_entry();
    }
#elif defined(__riscv)
    /* TODO: riscv implementation */
    result = __syscall5(SYS_clone, flags, (long)stack,
                        0, (long)tid_out, (long)tcb);
    if (result == 0) {
        __mlibc_thread_entry();
    }
#else
#error "Unsupported architecture"
#endif

    if (result < 0) {
        return -result;  /* Return positive errno */
    }

    /* Parent: result is child TID */
    *tid_out = (pid_t)result;
    return 0;
}

int sys_prepare_stack(
    void **stack,
    void *entry,
    void *user_arg,
    void *tcb,
    size_t *stack_size,
    size_t *guard_size,
    void **stack_base
) {
    /* Use default stack size if not specified */
    if (!*stack_size) {
        *stack_size = default_stacksize;
    }

    /* Guard page size */
    *guard_size = 0x1000;  /* 4KB guard page */

    if (!*stack) {
        /* Allocate stack + guard page */
        size_t total_size = *stack_size + *guard_size;
        void *map = mmap(nullptr, total_size,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1, 0);
        if (map == MAP_FAILED) {
            return errno;
        }

        /* Make guard page inaccessible */
        mprotect(map, *guard_size, PROT_NONE);

        /* Stack base is after guard page */
        *stack_base = (char *)map + *guard_size;
    } else {
        /* User-provided stack */
        *stack_base = *stack;
    }

    /*
     * Set up stack for new thread.
     * Stack grows down, so we start at the top.
     * Push: tcb, user_arg, entry (in reverse order so they pop correctly)
     *
     * Stack layout after setup (growing down):
     *   [high address]
     *   tcb
     *   user_arg
     *   entry
     *   [stack pointer] <- new thread starts here
     *   [low address]
     */
    uintptr_t sp = (uintptr_t)*stack_base + *stack_size;

    /* Align stack to 16 bytes (required by x86_64 ABI) */
    sp &= ~(uintptr_t)0xF;

    /* Push values onto stack (in reverse order) */
    sp -= sizeof(void *);
    *(void **)sp = tcb;

    sp -= sizeof(void *);
    *(void **)sp = user_arg;

    sp -= sizeof(void *);
    *(void **)sp = entry;

    /* Additional alignment for call (16-byte aligned before call) */
    sp -= sizeof(void *);
    *(void **)sp = nullptr;  /* Fake return address */

    /* Return stack pointer for clone */
    *stack = (void *)sp;

    return 0;
}

}  /* namespace mlibc */
