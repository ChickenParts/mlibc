/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.text
/* The shared loader currently binds eagerly on every architecture. Refuse
 * unexpected lazy entry; never return an unresolved function address. */
.global pltRelocateStub
.type pltRelocateStub,%function
pltRelocateStub:
 .word 0xe7f000f0
 b pltRelocateStub
.size pltRelocateStub,.-pltRelocateStub
.section .note.GNU-stack,"",%progbits
