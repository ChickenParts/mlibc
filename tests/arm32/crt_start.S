/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.text
.global test_start
.type test_start,%function
test_start:
 ldr sp,=0x40090000
 sub sp,sp,#16
 mov r0,#1
 str r0,[sp]
 adr r0,name
 str r0,[sp,#4]
 mov r0,#0
 str r0,[sp,#8]
 str r0,[sp,#12]
 b _start
name: .asciz "x"
.section .note.GNU-stack,"",%progbits
