/* SPDX-License-Identifier: MIT */

.section .text
.global __mlibc_thread_entry
.type __mlibc_thread_entry, @function

/*
 * The kernel returns from clone with SP pointing at the frame prepared by
 * sys_prepare_stack():
 *   [sp + 0]  fake return address
 *   [sp + 8]  entry function
 *   [sp + 16] user argument
 *   [sp + 24] final ABI thread pointer
 */
__mlibc_thread_entry:
	ld s0, 8(sp)
	ld s1, 16(sp)
	ld s2, 24(sp)
	addi sp, sp, 32

	mv tp, s2
	mv a0, s1
	jalr ra, s0, 0

	li a7, 2
	li a0, 0
	ecall
	unimp

.size __mlibc_thread_entry, . - __mlibc_thread_entry
