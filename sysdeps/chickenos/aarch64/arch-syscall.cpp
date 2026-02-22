#include <bits/syscall.h>

using sc_word_t = __sc_word_t;

/*
 * aarch64 ChickenOS syscall convention:
 *   X8  = syscall number
 *   X0-X5 = arguments 1-6
 *   X0 = return value (negative = -errno)
 */

sc_word_t __do_syscall0(long sc) {
	register sc_word_t x8 asm("x8") = sc;
	register sc_word_t x0 asm("x0");
	asm volatile("svc #0"
		: "=r"(x0)
		: "r"(x8)
		: "memory");
	return x0;
}

sc_word_t __do_syscall1(long sc, sc_word_t arg1) {
	register sc_word_t x8 asm("x8") = sc;
	register sc_word_t x0 asm("x0") = arg1;
	asm volatile("svc #0"
		: "+r"(x0)
		: "r"(x8)
		: "memory");
	return x0;
}

sc_word_t __do_syscall2(long sc, sc_word_t arg1, sc_word_t arg2) {
	register sc_word_t x8 asm("x8") = sc;
	register sc_word_t x0 asm("x0") = arg1;
	register sc_word_t x1 asm("x1") = arg2;
	asm volatile("svc #0"
		: "+r"(x0)
		: "r"(x8), "r"(x1)
		: "memory");
	return x0;
}

sc_word_t __do_syscall3(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3) {
	register sc_word_t x8 asm("x8") = sc;
	register sc_word_t x0 asm("x0") = arg1;
	register sc_word_t x1 asm("x1") = arg2;
	register sc_word_t x2 asm("x2") = arg3;
	asm volatile("svc #0"
		: "+r"(x0)
		: "r"(x8), "r"(x1), "r"(x2)
		: "memory");
	return x0;
}

sc_word_t __do_syscall4(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4) {
	register sc_word_t x8 asm("x8") = sc;
	register sc_word_t x0 asm("x0") = arg1;
	register sc_word_t x1 asm("x1") = arg2;
	register sc_word_t x2 asm("x2") = arg3;
	register sc_word_t x3 asm("x3") = arg4;
	asm volatile("svc #0"
		: "+r"(x0)
		: "r"(x8), "r"(x1), "r"(x2), "r"(x3)
		: "memory");
	return x0;
}

sc_word_t __do_syscall5(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4, sc_word_t arg5) {
	register sc_word_t x8 asm("x8") = sc;
	register sc_word_t x0 asm("x0") = arg1;
	register sc_word_t x1 asm("x1") = arg2;
	register sc_word_t x2 asm("x2") = arg3;
	register sc_word_t x3 asm("x3") = arg4;
	register sc_word_t x4 asm("x4") = arg5;
	asm volatile("svc #0"
		: "+r"(x0)
		: "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
		: "memory");
	return x0;
}

sc_word_t __do_syscall6(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4, sc_word_t arg5, sc_word_t arg6) {
	register sc_word_t x8 asm("x8") = sc;
	register sc_word_t x0 asm("x0") = arg1;
	register sc_word_t x1 asm("x1") = arg2;
	register sc_word_t x2 asm("x2") = arg3;
	register sc_word_t x3 asm("x3") = arg4;
	register sc_word_t x4 asm("x4") = arg5;
	register sc_word_t x5 asm("x5") = arg6;
	asm volatile("svc #0"
		: "+r"(x0)
		: "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
		: "memory");
	return x0;
}
