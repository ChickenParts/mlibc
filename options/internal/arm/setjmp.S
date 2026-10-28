/* SPDX-License-Identifier: MIT */
#include "mlibc-asm/jmp_buf.h"
#if defined(__ARM_FP) && __ARM_FP
#error ARM32 FP register preservation is not implemented in this initial soft-float slice
#endif
#if defined(__ARM_ARCH_PROFILE) && __ARM_ARCH_PROFILE == 'M'
#error This ARM-state jump implementation does not support Thumb-only M-profile
#endif
.syntax unified
/* ARM-state v4T-compatible base implementation; Thumb callers interwork.
 * Thumb-only M-profile needs a separate implementation, not this file. */
.arm
.text
.balign 4
.hidden __setjmp
.type __setjmp,%function
__setjmp:
 stmia r0,{r4-r11}
 str sp,[r0,#32]
 str lr,[r0,#36]
 cmp r2,#0
 bne __sigsetjmp
 mov r3,#0
 str r3,[r0,#JMP_BUF_SAVESIGS_OFFSET]
 mov r0,#0
 bx lr
.size __setjmp,.-__setjmp
.global setjmp
.global _setjmp
.type setjmp,%function
.type _setjmp,%function
setjmp:
_setjmp:
 mov r2,#0
 b __setjmp
.size setjmp,.-setjmp
.global sigsetjmp
.type sigsetjmp,%function
sigsetjmp:
 mov r2,#1
 b __setjmp
.size sigsetjmp,.-sigsetjmp
.global longjmp
.global _longjmp
.type longjmp,%function
.type _longjmp,%function
longjmp:
_longjmp:
 ldmia r0,{r4-r11}
 ldr sp,[r0,#32]
 ldr lr,[r0,#36]
 cmp r1,#0
 moveq r1,#1
 mov r0,r1
 bx lr
.size longjmp,.-longjmp
.section .note.GNU-stack,"",%progbits
