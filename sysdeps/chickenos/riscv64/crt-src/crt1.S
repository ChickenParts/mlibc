.section .data
.hidden __dso_handle
.global __dso_handle
__dso_handle:
	.quad 0

.weak __global_pointer$
.hidden __global_pointer$

.section .text
.global _start
_start:
	/* Load gp. */
.option push
.option norelax
	lla gp, __global_pointer$
.option pop

	mv a0, sp
	la a1, main
	call __mlibc_entry
	unimp

/* Load gp from .preinit_array since it may be used by .init_array. */
load_gp:
.option push
.option norelax
	lla gp, __global_pointer$
.option pop
	ret

.section .preinit_array,"aw"
	.dword load_gp

.section .note.GNU-stack,"",%progbits
