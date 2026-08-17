/* SPDX-License-Identifier: MIT */
.section .text

.global __mlibc_clone_start
.type __mlibc_clone_start, %function
__mlibc_clone_start:
	mov x8, #59
	svc #0
	cbz x0, __mlibc_thread_entry
	ret
.size __mlibc_clone_start, . - __mlibc_clone_start

.global __mlibc_thread_entry
.type __mlibc_thread_entry, %function
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
.section .note.GNU-stack,"",%progbits
