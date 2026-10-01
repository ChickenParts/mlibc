#include <bits/syscall.h>

using sc_word_t = __sc_word_t;

/*
 * riscv32 ChickenOS syscall convention (ilp32, rv32imac):
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

/*
 * 64-bit results (lseek64 only).  A 32-bit port returns a 64-bit value in the
 * pair of result registers: a0 = low word, a1 = high word -- the same pair the
 * psABI uses for a returned 64-bit integer, and the riscv32 analogue of i386's
 * EDX:EAX.  On error a0 = -errno and a1 = -1 (no valid offset is negative).
 * The kernel's riscv32 entry (src/arch/riscv32/syscall.c) writes a1 only for
 * SYS_LSEEK64; every other call preserves nothing in a1 (it is clobbered).
 * The off_t argument is the two consecutive argument registers (a1 = low,
 * a2 = high), NOT psABI even-register alignment (syscall_numbers.h).
 */
long long __do_syscall4_64(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3,
		sc_word_t arg4) {
	register long a7 asm("a7") = sc;
	register sc_word_t a0 asm("a0") = arg1;
	register sc_word_t a1 asm("a1") = arg2;
	register sc_word_t a2 asm("a2") = arg3;
	register sc_word_t a3 asm("a3") = arg4;
	asm volatile("ecall"
		: "+r"(a0), "+r"(a1)
		: "r"(a7), "r"(a2), "r"(a3)
		: "memory");
	return static_cast<long long>((static_cast<unsigned long long>(static_cast<unsigned int>(a1)) << 32)
			| static_cast<unsigned int>(a0));
}

/*
 * clone(2) for a new thread: (flags, stack, parent_tid, tls, child_tid).
 * The child resumes right after the ecall with a0 == 0 and sp == `stack`, whose
 * top two words Sysdeps<PrepareStack> filled with [entry][user_arg]; it must
 * never return into the C++ caller (that frame lives on the parent's stack).
 * It pops both and calls __mlibc_enter_thread(entry, user_arg), which does not
 * return.  tp (= tls) is installed in the child by the kernel's clone.
 */
#define CHK_STR2(x) #x
#define CHK_STR(x) CHK_STR2(x)
asm(".text\n"
	".globl __mlibc_clone_riscv32\n"
	".type __mlibc_clone_riscv32,@function\n"
	"__mlibc_clone_riscv32:\n\t"
	"li a7, " CHK_STR(SYS_clone) "\n\t"
	"ecall\n\t"
	"bnez a0, 1f\n\t"
	"lw a0, 0(sp)\n\t"
	"lw a1, 4(sp)\n\t"
	"addi sp, sp, 8\n\t"
	"li s0, 0\n\t"
	"call __mlibc_enter_thread\n\t"
	"unimp\n"
	"1:\n\t"
	"ret\n"
	".size __mlibc_clone_riscv32, .-__mlibc_clone_riscv32\n");
