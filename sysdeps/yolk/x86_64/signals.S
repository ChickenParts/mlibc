/*
 * Yolk sysdeps for mlibc - x86_64 Signal Restorer Trampolines
 * SPDX-License-Identifier: MIT
 *
 * These trampolines are installed as SA_RESTORER in sigaction.
 * When a signal handler returns, it jumps to this code which
 * calls sigreturn to restore the original context.
 */

#define SYS_sigreturn 343

.section .text

/*
 * Signal restorer for regular signal handlers.
 * Called when signal handler does RET - the return address
 * on the stack points here.
 */
.global __mlibc_signal_restore
.type __mlibc_signal_restore, @function
__mlibc_signal_restore:
    mov $SYS_sigreturn, %rax
    syscall
    /* Should never reach here - sigreturn doesn't return normally */
    ud2
.size __mlibc_signal_restore, . - __mlibc_signal_restore

/*
 * Signal restorer for SA_SIGINFO handlers (three-argument form).
 * Same implementation as regular - both call sigreturn.
 */
.global __mlibc_signal_restore_rt
.type __mlibc_signal_restore_rt, @function
__mlibc_signal_restore_rt:
    mov $SYS_sigreturn, %rax
    syscall
    /* Should never reach here */
    ud2
.size __mlibc_signal_restore_rt, . - __mlibc_signal_restore_rt

.section .note.GNU-stack,"",@progbits
