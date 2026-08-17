/*
 * Yolk ABI definitions for mlibc
 * SPDX-License-Identifier: MIT
 */

#ifndef _ABIS_YOLK_ABI_H
#define _ABIS_YOLK_ABI_H

/* Basic integer types */
typedef long long __mlibc_int64;
typedef unsigned long long __mlibc_uint64;
typedef __mlibc_int64 __mlibc_int_least64;
typedef __mlibc_uint64 __mlibc_uint_least64;

/* Type sizes (LP64 model) */
#define __MLIBC_INTPTR_SIGNED  1
#define __MLIBC_INTPTR_WIDTH   64
#define __MLIBC_LONG_WIDTH     64
#define __MLIBC_INTMAX_WIDTH   64

/* NULL pointer constant */
#define __MLIBC_NULL_TYPE void *

#endif /* _ABIS_YOLK_ABI_H */
