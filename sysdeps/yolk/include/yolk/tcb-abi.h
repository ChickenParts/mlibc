/* SPDX-License-Identifier: MIT */
#ifndef MLIBC_SYSDEPS_YOLK_TCB_ABI_H
#define MLIBC_SYSDEPS_YOLK_TCB_ABI_H

/*
 * Stable bootstrap fields initialized by the Yolk kernel before mlibc takes
 * ownership of the initial thread. Keep these constants synchronized with
 * Tcb in options/internal/include/mlibc/tcb.hpp and the kernel source gate.
 */
#define YOLK_MLIBC_TCB_SELF_OFFSET          0u
#define YOLK_MLIBC_TCB_DTV_SIZE_OFFSET      8u
#define YOLK_MLIBC_TCB_DTV_POINTERS_OFFSET 16u
#define YOLK_MLIBC_TCB_TID_OFFSET          24u
#define YOLK_MLIBC_TCB_DID_EXIT_OFFSET     28u

#if defined(__x86_64__)
#define YOLK_MLIBC_TCB_SIZE                144u
#define YOLK_MLIBC_TCB_STACK_CANARY_OFFSET 40u
#define YOLK_MLIBC_TCB_CANCEL_BITS_OFFSET  48u
#define YOLK_MLIBC_THREAD_POINTER_BIAS       0u
#elif defined(__aarch64__)
#define YOLK_MLIBC_TCB_SIZE                136u
#define YOLK_MLIBC_TCB_STACK_CANARY_OFFSET 32u
#define YOLK_MLIBC_TCB_CANCEL_BITS_OFFSET  40u
#define YOLK_MLIBC_THREAD_POINTER_BIAS     120u
#elif defined(__riscv) && __riscv_xlen == 64
#define YOLK_MLIBC_TCB_SIZE                136u
#define YOLK_MLIBC_TCB_STACK_CANARY_OFFSET 32u
#define YOLK_MLIBC_TCB_CANCEL_BITS_OFFSET  40u
#define YOLK_MLIBC_THREAD_POINTER_BIAS     136u
#else
#error "Unsupported Yolk mlibc TCB architecture"
#endif

#endif
