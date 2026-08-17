// SPDX-License-Identifier: MIT
// Compile-time ABI contract consumed by the Yolk kernel bootstrap-TCB loader.

#include <mlibc/tcb.hpp>
#include <stddef.h>

#if defined(__x86_64__)
static_assert(sizeof(Tcb) == 144, "Yolk x86-64 TCB size drifted");
static_assert(offsetof(Tcb, selfPointer) == 0x00);
static_assert(offsetof(Tcb, dtvSize) == 0x08);
static_assert(offsetof(Tcb, dtvPointers) == 0x10);
static_assert(offsetof(Tcb, tid) == 0x18);
static_assert(offsetof(Tcb, didExit) == 0x1c);
static_assert(offsetof(Tcb, stackCanary) == 0x28);
static_assert(offsetof(Tcb, cancelBits) == 0x30);
#elif defined(__aarch64__)
static_assert(sizeof(Tcb) == 136, "Yolk AArch64 TCB size drifted");
static_assert(offsetof(Tcb, selfPointer) == 0x00);
static_assert(offsetof(Tcb, dtvSize) == 0x08);
static_assert(offsetof(Tcb, dtvPointers) == 0x10);
static_assert(offsetof(Tcb, tid) == 0x18);
static_assert(offsetof(Tcb, didExit) == 0x1c);
static_assert(offsetof(Tcb, stackCanary) == 0x20);
static_assert(offsetof(Tcb, cancelBits) == 0x28);
static_assert(sizeof(Tcb) - 16 == 120,
              "Yolk AArch64 thread-pointer offset drifted");
#elif defined(__riscv) && __riscv_xlen == 64
static_assert(sizeof(Tcb) == 136, "Yolk RISC-V 64 TCB size drifted");
static_assert(offsetof(Tcb, selfPointer) == 0x00);
static_assert(offsetof(Tcb, dtvSize) == 0x08);
static_assert(offsetof(Tcb, dtvPointers) == 0x10);
static_assert(offsetof(Tcb, tid) == 0x18);
static_assert(offsetof(Tcb, didExit) == 0x1c);
static_assert(offsetof(Tcb, stackCanary) == 0x20);
static_assert(offsetof(Tcb, cancelBits) == 0x28);
#else
#error "Yolk mlibc TCB contract missing for this architecture"
#endif
