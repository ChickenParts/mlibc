/*
 * Yolk x86_64 C Runtime fini - crtn.S
 * SPDX-License-Identifier: MIT
 *
 * Provides the epilogue for .init and .fini sections.
 * Completes what crti.S started.
 */

.section .init
    pop %rbp
    ret

.section .fini
    pop %rbp
    ret
