.section .text
.global _start
_start:
	xorl %ebp, %ebp
	movl %esp, %eax
	andl $-16, %esp
	subl $8, %esp
	pushl $main
	pushl %eax
	call __mlibc_entry
	ud2

.section .note.GNU-stack,"",@progbits
