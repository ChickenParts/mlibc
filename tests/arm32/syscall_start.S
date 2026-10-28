/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.text
.global _start
.type _start,%function
_start:
 ldr sp,=0x40090000
 adr r0,vectors
 mcr p15,0,r0,c12,c0,0
 ldr r0,=0x12345678
 mcr p15,0,r0,c13,c0,3
 msr spsr_cxsf,#0x10
 adr lr,user_start
 movs pc,lr
.balign 32
vectors:
 b bad
 b bad
 b svc_entry
 b bad
 b bad
 b bad
 b bad
 b bad
svc_entry:
 stmdb sp!,{r0-r12,lr}
 mov r0,sp
 bl handle_svc
 ldr r1,=requested_child_sp
 ldr r0,[r1]
 cmp r0,#0
 beq 2f
 mov r2,#0
 str r2,[r1]
 mrs r1,cpsr
 msr cpsr_c,#0xdf
 mov sp,r0
 msr cpsr_c,r1
2:
 ldmia sp!,{r0-r12,pc}^
user_start:
 ldr sp,=0x40080000
 bl test_main
 mov r7,#0xff
 svc #0
bad:
 b bad
.global tls_probe
.type tls_probe,%function
tls_probe:
 stmdb sp!,{r4,r7,r12,lr}
 mov r1,#0x11
 mov r2,#0x22
 mov r3,#0x33
 mov r7,#0x77
 bl __aeabi_read_tp
 ldr r4,=0x12345678
 cmp r0,r4
 mov r0,#0
 bne 1f
 cmp r1,#0x11
 bne 1f
 cmp r2,#0x22
 bne 1f
 cmp r3,#0x33
 bne 1f
 cmp r7,#0x77
 moveq r0,#1
1:
 ldmia sp!,{r4,r7,r12,lr}
 bx lr
.section .note.GNU-stack,"",%progbits
