/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <stddef.h>
#define MLIBC_ARM_TP_BIAS 8
static inline uintptr_t __mlibc_arm_tp_from_tcb(uintptr_t tcb, size_t size) {
 return tcb + size - MLIBC_ARM_TP_BIAS;
}
static inline uintptr_t __mlibc_arm_tcb_from_tp(uintptr_t tp, size_t size) {
 return tp + MLIBC_ARM_TP_BIAS - size;
}
static inline size_t __mlibc_arm_tls_offset(size_t current, size_t alignment) {
 return ((current + MLIBC_ARM_TP_BIAS + alignment - 1) & ~(alignment - 1)) - MLIBC_ARM_TP_BIAS;
}
static inline uintptr_t __mlibc_arm_tcb_address(uintptr_t allocation, size_t size, size_t alignment) {
 return ((allocation + size - MLIBC_ARM_TP_BIAS + alignment - 1) & ~(alignment - 1)) + MLIBC_ARM_TP_BIAS - size;
}
