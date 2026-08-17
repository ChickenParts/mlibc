/* SPDX-License-Identifier: MIT */

.section .text
.global __mlibc_thread_entry
.type __mlibc_thread_entry, %function

/*
 * The kernel returns from clone with SP pointing at the frame prepared by
 * sys_prepare_stack():
 *   [sp + 0]  fake return address
 *   [sp + 8]  entry function
 *   [sp + 16] user argument
 *   [sp + 24] final ABI thread pointer
 */
__mlibc_thread_entry:
	ldr x19, [sp, #8]
	ldr x20, [sp, #16]
	ldr x21, [sp, #24]
	add sp, sp, #32

	msr tpidr_el0, x21
	mov x0, x20
	blr x19

	mov x8, #2
	mov x0, #0
	svc #0
	brk #0

.size __mlibc_thread_entry, . - __mlibc_thread_entry
