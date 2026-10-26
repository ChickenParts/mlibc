#include "mlibc-asm/helpers.h"
#include "mlibc-asm/jmp_buf.h"

/* PPC32 soft-float: save r2, r13-r31, LR, CR and SP. */
.hidden __setjmp
.type __setjmp, @function
__setjmp:
	stw 2, 0(3)
	stw 13, 4(3)
	stw 14, 8(3)
	stw 15, 12(3)
	stw 16, 16(3)
	stw 17, 20(3)
	stw 18, 24(3)
	stw 19, 28(3)
	stw 20, 32(3)
	stw 21, 36(3)
	stw 22, 40(3)
	stw 23, 44(3)
	stw 24, 48(3)
	stw 25, 52(3)
	stw 26, 56(3)
	stw 27, 60(3)
	stw 28, 64(3)
	stw 29, 68(3)
	stw 30, 72(3)
	stw 31, 76(3)
	mflr 0
	stw 0, 80(3)
	mfcr 0
	stw 0, 84(3)
	stw 1, 88(3)
	cmpwi 5, 0
	bne 1f
	li 0, 0
	stw 0, JMP_BUF_SAVESIGS_OFFSET(3)
	li 3, 0
	blr
1:
	b __sigsetjmp

.global setjmp
.type setjmp, @function
.global _setjmp
.type _setjmp, @function
setjmp:
_setjmp:
	li 4, 0
	li 5, 0
	b __setjmp

.global sigsetjmp
.type sigsetjmp, @function
sigsetjmp:
	li 5, 1
	b __setjmp

.global longjmp
.type longjmp, @function
.global _longjmp
.type _longjmp, @function
longjmp:
_longjmp:
	mr 12, 3
	mr 3, 4
	cmpwi 3, 0
	bne 1f
	li 3, 1
1:
	lwz 0, 84(12)
	mtcrf 255, 0
	lwz 0, 80(12)
	mtlr 0
	lwz 2, 0(12)
	lwz 13, 4(12)
	lwz 14, 8(12)
	lwz 15, 12(12)
	lwz 16, 16(12)
	lwz 17, 20(12)
	lwz 18, 24(12)
	lwz 19, 28(12)
	lwz 20, 32(12)
	lwz 21, 36(12)
	lwz 22, 40(12)
	lwz 23, 44(12)
	lwz 24, 48(12)
	lwz 25, 52(12)
	lwz 26, 56(12)
	lwz 27, 60(12)
	lwz 28, 64(12)
	lwz 29, 68(12)
	lwz 30, 72(12)
	lwz 31, 76(12)
	lwz 1, 88(12)
	blr

GNU_STACK_NOTE()
