/*
 * Yolk aarch64 C Runtime startup - crt0.S
 * SPDX-License-Identifier: MIT
 *
 * Entry point for user programs on ARM64.
 * Stack layout from kernel:
 *   [sp]       = argc
 *   [sp+8]     = argv[0]
 *   ...
 */

.section .text
.global _start
.type _start, @function

_start:
    /* Clear frame pointer */
    mov x29, #0
    mov x30, #0

    /* x0 = sp (pointer to argc on stack) */
    mov x0, sp

    /* x1 = address of main */
    adrp x1, main
    add x1, x1, :lo12:main

    /* Call mlibc entry point */
    bl __mlibc_entry

    /* Should never return */
    brk #0

.size _start, . - _start

.weak main
