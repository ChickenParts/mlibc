/*
 * Yolk riscv64 C Runtime fini - crtn.S
 * SPDX-License-Identifier: MIT
 */

.section .init
    ld ra, 8(sp)
    ld fp, 0(sp)
    addi sp, sp, 16
    ret

.section .fini
    ld ra, 8(sp)
    ld fp, 0(sp)
    addi sp, sp, 16
    ret
