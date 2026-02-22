.section .data
.hidden __dso_handle
.global __dso_handle
__dso_handle:
	.quad 0

.section .init
.global _init
_init:
	stp x29, x30, [sp, #-16]!
	mov x29, sp

.section .fini
.global _fini
_fini:
	stp x29, x30, [sp, #-16]!
	mov x29, sp

.section .note.GNU-stack,"",%progbits
