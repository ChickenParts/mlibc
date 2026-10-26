.section .data
.hidden __dso_handle
.global __dso_handle
.type __dso_handle,@object
__dso_handle:
	.long 0
.size __dso_handle,4

.section .init,"ax",@progbits
.global _init
.type _init,@function
_init:
	stwu 1,-16(1)
	mflr 0
	stw 0,20(1)

.section .fini,"ax",@progbits
.global _fini
.type _fini,@function
_fini:
	stwu 1,-16(1)
	mflr 0
	stw 0,20(1)

.section .note.GNU-stack,"",@progbits
