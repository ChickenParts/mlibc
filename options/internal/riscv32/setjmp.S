#include "mlibc-asm/helpers.h"
#include "mlibc-asm/jmp_buf.h"

/* riscv32, ilp32 (soft float): ra, s0-s11, sp = 14 words, then __savesigs. */

.hidden __setjmp
.type __setjmp, "function"
__setjmp:
	sw ra, 0(a0)
	sw s0, 4(a0)
	sw s1, 8(a0)
	sw s2, 12(a0)
	sw s3, 16(a0)
	sw s4, 20(a0)
	sw s5, 24(a0)
	sw s6, 28(a0)
	sw s7, 32(a0)
	sw s8, 36(a0)
	sw s9, 40(a0)
	sw s10, 44(a0)
	sw s11, 48(a0)
	sw sp, 52(a0)
	bne a2, zero, 1f
	sw zero, JMP_BUF_SAVESIGS_OFFSET(a0)
	li a0, 0
	ret

1:
	tail __sigsetjmp

.global setjmp
.type setjmp, "function"
.global _setjmp
.type _setjmp, "function"
setjmp:
_setjmp:
	li a1, 0
	li a2, 0
	tail __setjmp

.global sigsetjmp
.type sigsetjmp, "function"
sigsetjmp:
	li a2, 1
	tail __setjmp

.global longjmp
.type longjmp, "function"
.global _longjmp
.type _longjmp, "function"
longjmp:
_longjmp:
	lw ra, 0(a0)
	lw s0, 4(a0)
	lw s1, 8(a0)
	lw s2, 12(a0)
	lw s3, 16(a0)
	lw s4, 20(a0)
	lw s5, 24(a0)
	lw s6, 28(a0)
	lw s7, 32(a0)
	lw s8, 36(a0)
	lw s9, 40(a0)
	lw s10, 44(a0)
	lw s11, 48(a0)
	lw sp, 52(a0)
	seqz a0, a1
	add a0, a0, a1
	ret

GNU_STACK_NOTE()
