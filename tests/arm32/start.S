/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.text
.global _start
.type _start,%function
_start:
 ldr sp,=stack_end
 bl test_main
 cmp r0,#0
 ldreq r1,=0x20026
 ldrne r1,=0x20023
 mov r0,#0x18
 svc 0x123456
1: b 1b
.global probe
.type probe,%function
probe:
 stmdb sp!,{r4-r11,r12,lr}
 sub sp,sp,#8
 str r0,[sp]
 mov r4,#0x44
 mov r5,#0x55
 mov r6,#0x66
 mov r7,#0x77
 mov r8,#0x88
 mov r9,#0x99
 mov r10,#0xaa
 mov r11,#0xbb
 bl setjmp
 cmp r0,#0
 bne 2f
 ldr r0,[sp]
 mov r4,#0
 mov r5,#0
 mov r6,#0
 mov r7,#0
 mov r8,#0
 mov r9,#0
 mov r10,#0
 mov r11,#0
 sub sp,sp,#8
 mov r1,#1
 b longjmp
2:
 mov r0,#0
 cmp r4,#0x44
 bne 3f
 cmp r5,#0x55
 bne 3f
 cmp r6,#0x66
 bne 3f
 cmp r7,#0x77
 bne 3f
 cmp r8,#0x88
 bne 3f
 cmp r9,#0x99
 bne 3f
 cmp r10,#0xaa
 bne 3f
 cmp r11,#0xbb
 bne 3f
 tst sp,#7
 moveq r0,#1
3:
 add sp,sp,#8
 ldmia sp!,{r4-r11,r12,lr}
 bx lr
.bss
.balign 8
.space 16384
stack_end:
.section .note.GNU-stack,"",%progbits
