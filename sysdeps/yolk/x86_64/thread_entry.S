/*
 * Yolk sysdeps for mlibc - x86_64 Thread Entry Trampoline
 * SPDX-License-Identifier: MIT
 *
 * This is the entry point for newly cloned threads.
 * The stack was set up by sys_prepare_stack with:
 *   [rsp+0]  = fake return address (null)
 *   [rsp+8]  = entry function pointer
 *   [rsp+16] = user argument
 *   [rsp+24] = tcb pointer (for TLS)
 */

.section .text
.global __mlibc_thread_entry
.type __mlibc_thread_entry, @function

__mlibc_thread_entry:
    /*
     * New thread entry point.
     * Stack layout:
     *   rsp+0:  null (fake return address)
     *   rsp+8:  entry function
     *   rsp+16: user_arg
     *   rsp+24: tcb
     */

    /* Skip fake return address */
    add $8, %rsp

    /* Pop entry function into r12 (callee-saved) */
    pop %r12

    /* Pop user_arg into r13 (callee-saved) */
    pop %r13

    /* Pop tcb into r14 (callee-saved) */
    pop %r14

    /* Set up TLS: write tcb to FS base */
    mov %r14, %rdi
    wrfsbase %rdi

    /* Align stack so the callee sees SysV-compliant entry alignment.
     * Before `call`, RSP must be 16-byte aligned; the call push then
     * leaves callee RSP % 16 == 8.
     */
    and $-16, %rsp

    /* Call entry function: entry(user_arg) */
    mov %r13, %rdi
    call *%r12

    /* Entry function returned - exit thread */
    /* sys_thread_exit() */
    mov $2, %rax        /* SYS_exit */
    xor %edi, %edi      /* status = 0 */
    syscall

    /* Should never reach here */
    ud2

.size __mlibc_thread_entry, . - __mlibc_thread_entry
