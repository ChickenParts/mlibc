/* SPDX-License-Identifier: MIT */
.syntax unified
.arm

.text
.balign 4
.global __mlibc_clone_arm
.hidden __mlibc_clone_arm
.type __mlibc_clone_arm,%function
__mlibc_clone_arm:
 stmdb sp!,{r4,r7,r12,lr}
 ldr r4,[sp,#16] /* fifth AAPCS argument: child-tid pointer */
 ldr r7,=0x107 /* SYS_clone; flags, child stack, ptid, TP, ctid */
 svc #0
 cmp r0,#0
 beq .Lclone_child
 ldmia sp!,{r4,r7,r12,lr}
 bx lr
.Lclone_child:
 /* The kernel installed the child's stack and TLS. Never pop the parent's
  * saved registers or return through its C frame on this different stack. */
 ldr r0,[sp]
 ldr r1,[sp,#4]
 add sp,sp,#8
 mov r11,#0
 bl __mlibc_enter_thread
 .word 0xe7f000f0
1: b 1b
.size __mlibc_clone_arm,.-__mlibc_clone_arm
.section .note.GNU-stack,"",%progbits
