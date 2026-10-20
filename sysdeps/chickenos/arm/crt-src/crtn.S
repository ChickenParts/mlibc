/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.section .init
 ldmia sp!,{r4,lr}
 bx lr
.section .fini
 ldmia sp!,{r4,lr}
 bx lr
.section .note.GNU-stack,"",%progbits
