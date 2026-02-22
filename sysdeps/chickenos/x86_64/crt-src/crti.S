.section .data
.hidden __dso_handle
.global __dso_handle
__dso_handle:
	.quad 0

.section .init
.global _init
_init:
	push %rbp
	mov %rsp, %rbp

.section .fini
.global _fini
_fini:
	push %rbp
	mov %rsp, %rbp

.section .note.GNU-stack,"",@progbits
