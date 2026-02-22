#include <bits/syscall.h>

using sc_word_t = __sc_word_t;

/*
 * x86_64 ChickenOS syscall convention:
 *   RAX = syscall number
 *   RDI, RSI, RDX, R10, R8, R9 = arguments 1-6
 *   RAX = return value (negative = -errno)
 *   RCX, R11 clobbered by SYSCALL instruction
 */

sc_word_t __do_syscall0(long sc) {
	sc_word_t ret;
	asm volatile("syscall"
		: "=a"(ret)
		: "a"(sc)
		: "rcx", "r11", "memory");
	return ret;
}

sc_word_t __do_syscall1(long sc, sc_word_t arg1) {
	sc_word_t ret;
	asm volatile("syscall"
		: "=a"(ret)
		: "a"(sc), "D"(arg1)
		: "rcx", "r11", "memory");
	return ret;
}

sc_word_t __do_syscall2(long sc, sc_word_t arg1, sc_word_t arg2) {
	sc_word_t ret;
	asm volatile("syscall"
		: "=a"(ret)
		: "a"(sc), "D"(arg1), "S"(arg2)
		: "rcx", "r11", "memory");
	return ret;
}

sc_word_t __do_syscall3(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3) {
	sc_word_t ret;
	asm volatile("syscall"
		: "=a"(ret)
		: "a"(sc), "D"(arg1), "S"(arg2), "d"(arg3)
		: "rcx", "r11", "memory");
	return ret;
}

sc_word_t __do_syscall4(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4) {
	sc_word_t ret;
	register sc_word_t r10 asm("r10") = arg4;
	asm volatile("syscall"
		: "=a"(ret)
		: "a"(sc), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
		: "rcx", "r11", "memory");
	return ret;
}

sc_word_t __do_syscall5(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4, sc_word_t arg5) {
	sc_word_t ret;
	register sc_word_t r10 asm("r10") = arg4;
	register sc_word_t r8 asm("r8") = arg5;
	asm volatile("syscall"
		: "=a"(ret)
		: "a"(sc), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
		: "rcx", "r11", "memory");
	return ret;
}

sc_word_t __do_syscall6(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4, sc_word_t arg5, sc_word_t arg6) {
	sc_word_t ret;
	register sc_word_t r10 asm("r10") = arg4;
	register sc_word_t r8 asm("r8") = arg5;
	register sc_word_t r9 asm("r9") = arg6;
	asm volatile("syscall"
		: "=a"(ret)
		: "a"(sc), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
		: "rcx", "r11", "memory");
	return ret;
}
