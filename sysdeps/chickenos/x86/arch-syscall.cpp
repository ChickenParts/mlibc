#include <bits/syscall.h>

using sc_word_t = __sc_word_t;

/*
 * i386 ChickenOS syscall convention (src/arch/x86/i386/syscall.c):
 *   int $0x80
 *   EAX = syscall number
 *   EBX, ECX, EDX, ESI, EDI, EBP = arguments 1-6
 *   EAX = return value (negative = -errno)
 *
 * EBX is the PIC register in position-independent code and EBP is the frame
 * pointer, so neither can be named as an asm constraint portably.  Both are
 * loaded and restored by hand through "xchg" with a scratch register, which
 * works with and without -fPIC / -fno-omit-frame-pointer.
 */

sc_word_t __do_syscall0(long sc) {
	sc_word_t ret;
	asm volatile("int $0x80"
		: "=a"(ret)
		: "a"(sc)
		: "memory");
	return ret;
}

sc_word_t __do_syscall1(long sc, sc_word_t arg1) {
	sc_word_t ret;
	asm volatile("xchgl %%ebx, %2\n\t"
		"int $0x80\n\t"
		"xchgl %%ebx, %2"
		: "=a"(ret)
		: "a"(sc), "r"(arg1)
		: "memory");
	return ret;
}

sc_word_t __do_syscall2(long sc, sc_word_t arg1, sc_word_t arg2) {
	sc_word_t ret;
	asm volatile("xchgl %%ebx, %2\n\t"
		"int $0x80\n\t"
		"xchgl %%ebx, %2"
		: "=a"(ret)
		: "a"(sc), "r"(arg1), "c"(arg2)
		: "memory");
	return ret;
}

sc_word_t __do_syscall3(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3) {
	sc_word_t ret;
	asm volatile("xchgl %%ebx, %2\n\t"
		"int $0x80\n\t"
		"xchgl %%ebx, %2"
		: "=a"(ret)
		: "a"(sc), "r"(arg1), "c"(arg2), "d"(arg3)
		: "memory");
	return ret;
}

sc_word_t __do_syscall4(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4) {
	sc_word_t ret;
	asm volatile("xchgl %%ebx, %2\n\t"
		"int $0x80\n\t"
		"xchgl %%ebx, %2"
		: "=a"(ret)
		: "a"(sc), "r"(arg1), "c"(arg2), "d"(arg3), "S"(arg4)
		: "memory");
	return ret;
}

sc_word_t __do_syscall5(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4, sc_word_t arg5) {
	sc_word_t ret;
	asm volatile("xchgl %%ebx, %2\n\t"
		"int $0x80\n\t"
		"xchgl %%ebx, %2"
		: "=a"(ret)
		: "a"(sc), "r"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5)
		: "memory");
	return ret;
}

sc_word_t __do_syscall6(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4, sc_word_t arg5, sc_word_t arg6) {
	/* Six register args need EBP too, and the compiler may be using EBP as
	 * the frame pointer; pass the argument block by pointer instead and
	 * save/restore EBX and EBP around the trap. */
	sc_word_t ret;
	sc_word_t block[3] = {arg1, arg6, static_cast<sc_word_t>(sc)};
	asm volatile("pushl %%ebx\n\t"
		"pushl %%ebp\n\t"
		"movl 4(%%eax), %%ebp\n\t"
		"movl (%%eax), %%ebx\n\t"
		"movl 8(%%eax), %%eax\n\t"
		"int $0x80\n\t"
		"popl %%ebp\n\t"
		"popl %%ebx"
		: "=a"(ret)
		: "a"(block), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5)
		: "memory");
	return ret;
}

/* lseek: 64-bit result returned in EDX:EAX by the kernel's i386 entry. */
long long __do_syscall4_64(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4) {
	unsigned int lo, hi;
	asm volatile("xchgl %%ebx, %3\n\t"
		"int $0x80\n\t"
		"xchgl %%ebx, %3"
		: "=a"(lo), "=d"(hi)
		: "a"(sc), "r"(arg1), "c"(arg2), "1"(arg3), "S"(arg4)
		: "memory");
	return static_cast<long long>((static_cast<unsigned long long>(hi) << 32) | lo);
}
