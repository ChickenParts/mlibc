/*
 * Yolk sysdeps for mlibc - x86_64 Thread Entry Trampoline
 * SPDX-License-Identifier: MIT
 *
 * This is the entry point for newly cloned threads.
 * The stack was set up by sys_prepare_stack with:
 *   [rsp+0]  = fake return address (null)
 *   [rsp+8]  = entry function pointer
 *   [rsp+16] = user argument
 *   [rsp+24] = final ABI thread pointer
 */

.section .text
.global __mlibc_thread_entry
.type __mlibc_thread_entry, @function

__mlibc_thread_entry:
    /* Skip the synthetic return slot and consume the prepared frame. */
    add $8, %rsp
    pop %r12
    pop %r13
    pop %r14

    /* The kernel already publishes the child FS base from CLONE_SETTLS. Keep
     * the explicit write for standalone compatibility and as an ABI check. */
    wrfsbase %r14

    /* sys_prepare_stack() leaves the post-frame stack 16-byte aligned, which
     * is the SysV caller-side alignment required immediately before call. */
    and $-16, %rsp
    mov %r13, %rdi
    call *%r12

    mov $2, %rax
    xor %edi, %edi
    syscall
    ud2

.size __mlibc_thread_entry, . - __mlibc_thread_entry
