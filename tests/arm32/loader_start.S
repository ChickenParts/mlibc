/* SPDX-License-Identifier: MIT */
#include "loader_entry.h"
.syntax unified
.arm
.text
.global _start
.type _start,%function
_start:
 ldr sp,=0x40090000
 ldr r0,=payload
 ldr r1,=0x41000000
 ldr r2,=payload_end
1:
 cmp r0,r2
 ldrblo r3,[r0],#1
 strblo r3,[r1],#1
 blo 1b
 ldr r0,=(0x41000000+ARM32_LOADER_ENTRY)
 bx r0
.ltorg
.balign 4
payload:
.incbin ARM32_LOADER_IMAGE
payload_end:
.section .note.GNU-stack,"",%progbits
