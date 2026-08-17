/*
 * Yolk riscv64 C Runtime init - crti.S
 * SPDX-License-Identifier: MIT
 */

.section .init
.global _init
.type _init, @function
_init:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd fp, 0(sp)
    addi fp, sp, 16

.section .fini
.global _fini
.type _fini, @function
_fini:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd fp, 0(sp)
    addi fp, sp, 16
