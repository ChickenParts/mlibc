/*
 * Yolk x86_64 C Runtime startup - crt0.S
 * SPDX-License-Identifier: MIT
 *
 * This is the entry point for user programs. The kernel sets up the stack as:
 *   [rsp]       = argc
 *   [rsp+8]     = argv[0]
 *   ...
 *   [rsp+8*argc+8] = NULL (argv terminator)
 *   [next]      = envp[0]
 *   ...
 *   [...]       = NULL (envp terminator)
 *   [next]      = auxv entries...
 */

.section .text
.global _start
.type _start, @function

_start:
    /* Clear frame pointer for clean backtraces */
    xor %rbp, %rbp

    /* Align stack to 16 bytes (required by System V ABI) */
    and $-16, %rsp

    /* rsp points to argc - pass as first argument to __mlibc_entry */
    mov %rsp, %rdi

    /* Second argument: pointer to main */
    lea main(%rip), %rsi

    /* Call mlibc entry point */
    call __mlibc_entry

    /* Should never return, but just in case */
    ud2

.size _start, . - _start

/* Provide weak symbol for main in case it's not defined */
.weak main
