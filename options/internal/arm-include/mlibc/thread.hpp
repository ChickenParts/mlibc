/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <mlibc/tcb.hpp>
#include <bits/ensure.h>
#include <mlibc/tls-layout.h>
extern "C" void *__aeabi_read_tp(void);
namespace mlibc {
inline Tcb *get_current_tcb() {
 uintptr_t tp = reinterpret_cast<uintptr_t>(__aeabi_read_tp());
 auto tcb = reinterpret_cast<Tcb *>(__mlibc_arm_tcb_from_tp(tp, sizeof(Tcb)));
 __ensure(tcb == tcb->selfPointer);
 return tcb;
}
inline uintptr_t get_sp() {
 uintptr_t sp;
 asm volatile("mov %0, sp" : "=r"(sp));
 return sp;
}
}
