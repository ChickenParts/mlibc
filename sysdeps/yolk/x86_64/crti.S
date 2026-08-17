/*
 * Yolk x86_64 C Runtime init - crti.S
 * SPDX-License-Identifier: MIT
 *
 * Provides the prologue for .init and .fini sections.
 * These are completed by crtn.S
 */

.section .init
.global _init
.type _init, @function
_init:
    push %rbp
    mov %rsp, %rbp
    /* .init section content from other objects goes here */

.section .fini
.global _fini
.type _fini, @function
_fini:
    push %rbp
    mov %rsp, %rbp
    /* .fini section content from other objects goes here */
