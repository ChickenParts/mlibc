/*
 * Yolk aarch64 C Runtime fini - crtn.S
 * SPDX-License-Identifier: MIT
 */

.section .init
    ldp x29, x30, [sp], #16
    ret

.section .fini
    ldp x29, x30, [sp], #16
    ret
