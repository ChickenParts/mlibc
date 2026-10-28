/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.text
.balign 4
.global _start
.type _start,%function
.hidden __ehdr_start
.hidden _DYNAMIC
.hidden __mlibc_arm_relocate_self
.hidden interpreterMain
_start:
 /* No GOT or absolute-address dependency before self relocation. */
 mov r4,sp
 ldr r0,.Lheader
.Lheader_add:
 add r0,pc,r0
 ldr r1,.Ldynamic
.Ldynamic_add:
 add r1,pc,r1
 bl __mlibc_arm_relocate_self
 cmp r0,#0
 bne .Lbad
 mov r0,r4
 bl interpreterMain
 bx r0
.Lbad:
 udf #0
 b .Lbad
.balign 4
.Lheader:
 .word __ehdr_start - (.Lheader_add + 8)
.Ldynamic:
 .word _DYNAMIC - (.Ldynamic_add + 8)
.size _start,.-_start
.section .note.GNU-stack,"",%progbits
