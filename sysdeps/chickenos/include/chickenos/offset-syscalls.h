/* SPDX-License-Identifier: MIT */
#pragma once
#include <bits/syscall.h>
/* ARM32 packs each signed 64-bit offset into consecutive low/high words.
 * These helpers are private to the ARM sysdeps and their execution tests. */
#if !defined(__arm__)
#error "ARM32 offset syscalls require the ARM architecture"
#endif
enum {
	__CHICKEN_LSEEK64 = 0x407,
	__CHICKEN_TRUNCATE64 = 0x415,
	__CHICKEN_FTRUNCATE64 = 0x416,
	__CHICKEN_MMAP_PAGES = 0xce
};
#define __CHICKEN_OFF_LO(x) ((__sc_word_t)(unsigned int)(unsigned long long)(x))
#define __CHICKEN_OFF_HI(x) ((__sc_word_t)(unsigned int)((unsigned long long)(x) >> 32))
static inline long long __chicken_seek(long fd, long long off, long whence) {
	return __do_syscall4_64(
	    __CHICKEN_LSEEK64, fd, __CHICKEN_OFF_LO(off), __CHICKEN_OFF_HI(off), whence
	);
}
static inline long __chicken_pread(long fd, long buf, long len, long long off) {
	return __do_syscall5(SYS_pread64, fd, buf, len, __CHICKEN_OFF_LO(off), __CHICKEN_OFF_HI(off));
}
static inline long __chicken_pwrite(long fd, long buf, long len, long long off) {
	return __do_syscall5(SYS_pwrite64, fd, buf, len, __CHICKEN_OFF_LO(off), __CHICKEN_OFF_HI(off));
}
static inline long __chicken_truncate(long path, long long off) {
	return __do_syscall3(__CHICKEN_TRUNCATE64, path, __CHICKEN_OFF_LO(off), __CHICKEN_OFF_HI(off));
}
static inline long __chicken_ftruncate(long fd, long long off) {
	return __do_syscall3(__CHICKEN_FTRUNCATE64, fd, __CHICKEN_OFF_LO(off), __CHICKEN_OFF_HI(off));
}
static inline int __chicken_mmap_offset_valid(long long off) {
	return off >= 0 && !(off & 4095) && ((unsigned long long)off >> 12) <= 0xffffffffULL;
}
static inline long __chicken_mmap(long a, long b, long c, long d, long e, long long off) {
	return __do_syscall6(
	    __CHICKEN_MMAP_PAGES, a, b, c, d, e, (__sc_word_t)((unsigned long long)off >> 12)
	);
}
#undef __CHICKEN_OFF_LO
#undef __CHICKEN_OFF_HI
