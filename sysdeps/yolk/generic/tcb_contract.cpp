/* SPDX-License-Identifier: MIT */
#include <mlibc/tcb.hpp>

#if defined(__x86_64__)
static_assert(sizeof(Tcb) == 144,
              "Yolk x86-64 bootstrap TCB layout drifted");
#elif defined(__aarch64__)
static_assert(sizeof(Tcb) == 136,
              "Yolk AArch64 bootstrap TCB layout drifted");
#elif defined(__riscv) && __riscv_xlen == 64
static_assert(sizeof(Tcb) == 136,
              "Yolk RISC-V 64 bootstrap TCB layout drifted");
#else
#error "Unsupported Yolk TCB architecture"
#endif
