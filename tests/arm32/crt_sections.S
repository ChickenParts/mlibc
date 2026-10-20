/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.section .init
 bl init_body
.section .fini
 bl fini_body
.section .note.GNU-stack,"",%progbits
