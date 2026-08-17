/* SPDX-License-Identifier: MIT */
.section .text

.global __mlibc_clone_start
.type __mlibc_clone_start, @function
__mlibc_clone_start:
	/* C ABI: flags, stack, parent_tid, child_tid, tcb. */
	movq %rcx, %r10
	movq $59, %rax
	syscall
	testq %rax, %rax
	jz __mlibc_thread_entry
	retq
.size __mlibc_clone_start, . - __mlibc_clone_start

.global __mlibc_thread_entry
.type __mlibc_thread_entry, @function
__mlibc_thread_entry:
	addq $8, %rsp
	popq %r12
	popq %r13
	popq %r14

	/* Set FS through the kernel so hardware policy stays centralized. */
	movq $352, %rax
	movq $0x1002, %rdi
	movq %r14, %rsi
	syscall
	testq %rax, %rax
	js .Lthread_exit

	andq $-16, %rsp
	movq %r13, %rdi
	call *%r12
.Lthread_exit:
	movq $2, %rax
	xorl %edi, %edi
	syscall
	ud2
.size __mlibc_thread_entry, . - __mlibc_thread_entry
.section .note.GNU-stack,"",@progbits
