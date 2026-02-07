/*
 * Yolk riscv64 C Runtime startup - crt0.S
 * SPDX-License-Identifier: MIT
 *
 * Entry point for user programs on RISC-V 64-bit.
 * Stack layout from kernel:
 *   [sp]       = argc
 *   [sp+8]     = argv[0]
 *   ...
 */

.section .text
.global _start
.type _start, @function

_start:
    /* Set global pointer */
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    /* Clear frame pointer */
    li fp, 0
    li ra, 0

    /* a0 = sp (pointer to argc on stack) */
    mv a0, sp

    /* a1 = address of main */
    la a1, main

    /* Call mlibc entry point */
    call __mlibc_entry

    /* Should never return */
    ebreak

.size _start, . - _start

.weak main
.weak __global_pointer$
