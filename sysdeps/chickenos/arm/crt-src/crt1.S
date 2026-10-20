/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.section .data
.balign 4
.hidden __dso_handle
.global __dso_handle
__dso_handle:
 .word 0
.text
.balign 4
.global _start
.type _start,%function
_start:
 mov r0,sp
 ldr r1,.Lmain
.Lmain_add:
 add r1,pc,r1
 bl __mlibc_entry
 /* __mlibc_entry calls exit and must not return. */
 .word 0xe7f000f0
1: b 1b
.balign 4
.Lmain:
 .word main - (.Lmain_add + 8)
.size _start,.-_start
.section .note.GNU-stack,"",%progbits
