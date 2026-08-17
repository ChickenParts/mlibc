/* SPDX-License-Identifier: MIT */
.section .text

.global __mlibc_clone_start
.type __mlibc_clone_start, @function
__mlibc_clone_start:
	li a7, 59
	ecall
	beqz a0, __mlibc_thread_entry
	ret
.size __mlibc_clone_start, . - __mlibc_clone_start

.global __mlibc_thread_entry
.type __mlibc_thread_entry, @function
__mlibc_thread_entry:
	ld s1, 8(sp)
	ld s2, 16(sp)
	ld tp, 24(sp)
	addi sp, sp, 32
	mv a0, s2
	jalr ra, s1, 0
	li a7, 2
	li a0, 0
	ecall
	ebreak
.size __mlibc_thread_entry, . - __mlibc_thread_entry
.section .note.GNU-stack,"",@progbits
