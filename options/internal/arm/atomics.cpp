/* SPDX-License-Identifier: MIT */
/* The first complete libc profile uses compiler-emitted v7-A exclusives.
 * Older ISA support needs explicit kernel-assisted atomics; the jump/syscall
 * components already support it, but the full library must not fake it. */
#if __ARM_ARCH < 7
#error Full pre-v7 ARM libc requires a platform atomic fallback
#endif
static_assert(__atomic_always_lock_free(4, nullptr));
static_assert(__atomic_always_lock_free(8, nullptr));
