.section .text
.global _start
_start:
	mov x29, #0
	mov x30, #0
	mov x0, sp
	adrp x1, main
	add x1, x1, :lo12:main
	bl __mlibc_entry
	brk #1

.section .note.GNU-stack,"",%progbits
