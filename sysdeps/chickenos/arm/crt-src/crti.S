/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.section .init
.balign 4
.global _init
.type _init,%function
_init:
 stmdb sp!,{r4,lr}
.section .fini
.balign 4
.global _fini
.type _fini,%function
_fini:
 stmdb sp!,{r4,lr}
.section .note.GNU-stack,"",%progbits
