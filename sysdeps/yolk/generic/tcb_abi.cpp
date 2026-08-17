/* SPDX-License-Identifier: MIT */
#include <mlibc/tcb.hpp>
#include <yolk/tcb-abi.h>
#include <stddef.h>

static_assert(offsetof(Tcb, selfPointer) == YOLK_MLIBC_TCB_SELF_OFFSET);
static_assert(offsetof(Tcb, dtvSize) == YOLK_MLIBC_TCB_DTV_SIZE_OFFSET);
static_assert(offsetof(Tcb, dtvPointers) == YOLK_MLIBC_TCB_DTV_POINTERS_OFFSET);
static_assert(offsetof(Tcb, tid) == YOLK_MLIBC_TCB_TID_OFFSET);
static_assert(offsetof(Tcb, didExit) == YOLK_MLIBC_TCB_DID_EXIT_OFFSET);
static_assert(offsetof(Tcb, stackCanary) == YOLK_MLIBC_TCB_STACK_CANARY_OFFSET);
static_assert(offsetof(Tcb, cancelBits) == YOLK_MLIBC_TCB_CANCEL_BITS_OFFSET);
static_assert(sizeof(Tcb) == YOLK_MLIBC_TCB_SIZE);

#if defined(__aarch64__)
static_assert(YOLK_MLIBC_THREAD_POINTER_BIAS == sizeof(Tcb) - TP_TCB_OFFSET);
#elif defined(__riscv) && __riscv_xlen == 64
static_assert(YOLK_MLIBC_THREAD_POINTER_BIAS == sizeof(Tcb));
#else
static_assert(YOLK_MLIBC_THREAD_POINTER_BIAS == 0);
#endif
