/* SPDX-License-Identifier: MIT */
/* ChickenOS ARM32: r7 number, r0-r5 argument words, r1:r0 wide result.
 * Base AAPCS callers use r0-r3 then stack slots; preserve r4/r5/r7 and LR.
 * ARM-state assembly deliberately supports Thumb callers through interworking. */
.syntax unified
.arm
.text
.balign 4
.global __do_syscall0
.type __do_syscall0,%function
__do_syscall0:
 stmdb sp!,{r4,r5,r7,lr}
 mov r7,r0
 svc #0
 ldmia sp!,{r4,r5,r7,lr}
 bx lr
.size __do_syscall0,.-__do_syscall0
.balign 4
.global __do_syscall1
.type __do_syscall1,%function
__do_syscall1:
 stmdb sp!,{r4,r5,r7,lr}
 mov r7,r0
 mov r0,r1
 svc #0
 ldmia sp!,{r4,r5,r7,lr}
 bx lr
.size __do_syscall1,.-__do_syscall1
.balign 4
.global __do_syscall2
.type __do_syscall2,%function
__do_syscall2:
 stmdb sp!,{r4,r5,r7,lr}
 mov r7,r0
 mov r0,r1
 mov r1,r2
 svc #0
 ldmia sp!,{r4,r5,r7,lr}
 bx lr
.size __do_syscall2,.-__do_syscall2
.balign 4
.global __do_syscall3
.type __do_syscall3,%function
__do_syscall3:
 stmdb sp!,{r4,r5,r7,lr}
 mov r7,r0
 mov r0,r1
 mov r1,r2
 mov r2,r3
 svc #0
 ldmia sp!,{r4,r5,r7,lr}
 bx lr
.size __do_syscall3,.-__do_syscall3
.balign 4
.global __do_syscall4
.type __do_syscall4,%function
__do_syscall4:
 stmdb sp!,{r4,r5,r7,lr}
 mov r7,r0
 mov r0,r1
 mov r1,r2
 mov r2,r3
 ldr r3,[sp,#16]
 svc #0
 ldmia sp!,{r4,r5,r7,lr}
 bx lr
.size __do_syscall4,.-__do_syscall4
.balign 4
.global __do_syscall5
.type __do_syscall5,%function
__do_syscall5:
 stmdb sp!,{r4,r5,r7,lr}
 mov r7,r0
 mov r0,r1
 mov r1,r2
 mov r2,r3
 ldr r3,[sp,#16]
 ldr r4,[sp,#20]
 svc #0
 ldmia sp!,{r4,r5,r7,lr}
 bx lr
.size __do_syscall5,.-__do_syscall5
.balign 4
.global __do_syscall6
.type __do_syscall6,%function
__do_syscall6:
 stmdb sp!,{r4,r5,r7,lr}
 mov r7,r0
 mov r0,r1
 mov r1,r2
 mov r2,r3
 ldr r3,[sp,#16]
 ldr r4,[sp,#20]
 ldr r5,[sp,#24]
 svc #0
 ldmia sp!,{r4,r5,r7,lr}
 bx lr
.size __do_syscall6,.-__do_syscall6
.global __do_syscall4_64
.type __do_syscall4_64,%function
.set __do_syscall4_64,__do_syscall4

/* RTABI32 __aeabi_read_tp may clobber r0, IP, LR and flags, but NOT r1-r3.
 * ChickenOS must install TPIDRURO on v7-A; earlier profiles use its syscall.
 * Neither path assumes a single process-global TLS pointer. */
.balign 4
.global __aeabi_read_tp
.type __aeabi_read_tp,%function
__aeabi_read_tp:
#if __ARM_ARCH >= 7
 mrc p15,0,r0,c13,c0,3
 bx lr
#else
 stmdb sp!,{r1-r3,r7,r12,lr}
 ldr r7,=0x302 /* SYS_get_thread_area; checked against bits/syscall.h in tests. */
 svc #0
 ldmia sp!,{r1-r3,r7,r12,lr}
 bx lr
#endif
.size __aeabi_read_tp,.-__aeabi_read_tp
.section .note.GNU-stack,"",%progbits
