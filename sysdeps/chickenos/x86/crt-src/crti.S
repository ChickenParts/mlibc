.section .data
.hidden __dso_handle
.global __dso_handle
__dso_handle:
	.long 0

.section .init
.global _init
_init:
	push %ebp
	mov %esp, %ebp

.section .fini
.global _fini
_fini:
	push %ebp
	mov %esp, %ebp

.section .note.GNU-stack,"",@progbits
