/* SPDX-License-Identifier: MIT */
#if defined(__ARM_FP) && __ARM_FP
#error This initial ARM32 fenv implementation supports soft float only
#endif
.syntax unified
.arm
.text
.global fegetround
.type fegetround,%function
fegetround:
 mov r0,#0
 bx lr
.global __fesetround
.hidden __fesetround
.type __fesetround,%function
__fesetround:
 cmp r0,#0
 movne r0,#1
 bx lr
.global fetestexcept
.type fetestexcept,%function
fetestexcept:
 mov r0,#0
 bx lr
.global feclearexcept
.type feclearexcept,%function
feclearexcept:
 mov r0,#0
 bx lr
.global feraiseexcept
.type feraiseexcept,%function
feraiseexcept:
 cmp r0,#0
 movne r0,#1
 bx lr
.global fegetenv
.type fegetenv,%function
fegetenv:
 mov r1,#0
 str r1,[r0]
 mov r0,#0
 bx lr
.global fesetenv
.type fesetenv,%function
fesetenv:
 cmn r0,#1
 moveq r0,#0
 bxeq lr
 ldr r0,[r0]
 cmp r0,#0
 movne r0,#1
 bx lr
.section .note.GNU-stack,"",%progbits
