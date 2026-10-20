/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
/* AAELF32: (S + A) | T. Keep a Thumb function's state separate from the
 * arithmetic; odd data addresses are data and must not be treated as Thumb. */
static inline uint32_t __mlibc_arm_symbol_value(uint32_t address, uint32_t addend, int is_function) {
 if (!is_function) return address + addend;
 return ((address & ~1u) + addend) | (address & 1u);
}
