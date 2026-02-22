#include <bits/syscall.h>

using sc_word_t = __sc_word_t;

/*
 * riscv64 ChickenOS syscall convention:
 *   A7  = syscall number
 *   A0-A5 = arguments 1-6
 *   A0 = return value (negative = -errno)
 */

sc_word_t __do_syscall0(long sc) {
	register long a7 asm("a7") = sc;
	register sc_word_t a0 asm("a0");
	asm volatile("ecall"
		: "=r"(a0)
		: "r"(a7)
		: "memory", "a1");
	return a0;
}

sc_word_t __do_syscall1(long sc, sc_word_t arg1) {
	register long a7 asm("a7") = sc;
	register sc_word_t a0 asm("a0") = arg1;
	asm volatile("ecall"
		: "+r"(a0)
		: "r"(a7)
		: "memory", "a1");
	return a0;
}

sc_word_t __do_syscall2(long sc, sc_word_t arg1, sc_word_t arg2) {
	register long a7 asm("a7") = sc;
	register sc_word_t a0 asm("a0") = arg1;
	register sc_word_t a1 asm("a1") = arg2;
	asm volatile("ecall"
		: "+r"(a0)
		: "r"(a7), "r"(a1)
		: "memory");
	return a0;
}

sc_word_t __do_syscall3(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3) {
	register long a7 asm("a7") = sc;
	register sc_word_t a0 asm("a0") = arg1;
	register sc_word_t a1 asm("a1") = arg2;
	register sc_word_t a2 asm("a2") = arg3;
	asm volatile("ecall"
		: "+r"(a0)
		: "r"(a7), "r"(a1), "r"(a2)
		: "memory");
	return a0;
}

sc_word_t __do_syscall4(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4) {
	register long a7 asm("a7") = sc;
	register sc_word_t a0 asm("a0") = arg1;
	register sc_word_t a1 asm("a1") = arg2;
	register sc_word_t a2 asm("a2") = arg3;
	register sc_word_t a3 asm("a3") = arg4;
	asm volatile("ecall"
		: "+r"(a0)
		: "r"(a7), "r"(a1), "r"(a2), "r"(a3)
		: "memory");
	return a0;
}

sc_word_t __do_syscall5(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4, sc_word_t arg5) {
	register long a7 asm("a7") = sc;
	register sc_word_t a0 asm("a0") = arg1;
	register sc_word_t a1 asm("a1") = arg2;
	register sc_word_t a2 asm("a2") = arg3;
	register sc_word_t a3 asm("a3") = arg4;
	register sc_word_t a4 asm("a4") = arg5;
	asm volatile("ecall"
		: "+r"(a0)
		: "r"(a7), "r"(a1), "r"(a2), "r"(a3), "r"(a4)
		: "memory");
	return a0;
}

sc_word_t __do_syscall6(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4, sc_word_t arg5, sc_word_t arg6) {
	register long a7 asm("a7") = sc;
	register sc_word_t a0 asm("a0") = arg1;
	register sc_word_t a1 asm("a1") = arg2;
	register sc_word_t a2 asm("a2") = arg3;
	register sc_word_t a3 asm("a3") = arg4;
	register sc_word_t a4 asm("a4") = arg5;
	register sc_word_t a5 asm("a5") = arg6;
	asm volatile("ecall"
		: "+r"(a0)
		: "r"(a7), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
		: "memory");
	return a0;
}
