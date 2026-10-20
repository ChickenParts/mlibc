/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.text
.global _start
.type _start,%function
_start:
 ldr sp,=0x40090000
 bl test_main
 cmp r0,#0
 ldreq r1,=0x20026
 ldrne r1,=0x20023
 mov r0,#0x18
 svc 0x123456
1: b 1b
.section .note.GNU-stack,"",%progbits
