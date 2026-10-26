#include <bits/syscall.h>

using sc_word_t = __sc_word_t;

/*
 * ChickenOS PPC32 syscall convention (PowerPC `sc`):
 *   r0 = syscall number
 *   r3-r8 = arguments 1-6
 *   r3 = return value (negative = -errno)
 *   cr0 and memory are clobbered by the exception path.
 *
 * The kernel's 32-bit pair ABI deliberately passes 64-bit syscall arguments
 * low word first, even though PPC is big endian. Keep these wrappers in terms
 * of syscall registers rather than letting the C ABI split a long long.
 */
sc_word_t __do_syscall0(long sc) {
	register sc_word_t r0 asm("r0") = sc;
	register sc_word_t r3 asm("r3");
	asm volatile("sc" : "=r"(r3) : "r"(r0) : "cr0", "memory");
	return r3;
}

sc_word_t __do_syscall1(long sc, sc_word_t arg1) {
	register sc_word_t r0 asm("r0") = sc;
	register sc_word_t r3 asm("r3") = arg1;
	asm volatile("sc" : "+r"(r3) : "r"(r0) : "cr0", "memory");
	return r3;
}

sc_word_t __do_syscall2(long sc, sc_word_t arg1, sc_word_t arg2) {
	register sc_word_t r0 asm("r0") = sc;
	register sc_word_t r3 asm("r3") = arg1;
	register sc_word_t r4 asm("r4") = arg2;
	asm volatile("sc" : "+r"(r3) : "r"(r0), "r"(r4) : "cr0", "memory");
	return r3;
}

sc_word_t __do_syscall3(long sc, sc_word_t arg1, sc_word_t arg2, sc_word_t arg3) {
	register sc_word_t r0 asm("r0") = sc;
	register sc_word_t r3 asm("r3") = arg1;
	register sc_word_t r4 asm("r4") = arg2;
	register sc_word_t r5 asm("r5") = arg3;
	asm volatile("sc" : "+r"(r3) : "r"(r0), "r"(r4), "r"(r5) : "cr0", "memory");
	return r3;
}

sc_word_t __do_syscall4(long sc, sc_word_t arg1, sc_word_t arg2,
		sc_word_t arg3, sc_word_t arg4) {
	register sc_word_t r0 asm("r0") = sc;
	register sc_word_t r3 asm("r3") = arg1;
	register sc_word_t r4 asm("r4") = arg2;
	register sc_word_t r5 asm("r5") = arg3;
	register sc_word_t r6 asm("r6") = arg4;
	asm volatile("sc" : "+r"(r3) : "r"(r0), "r"(r4), "r"(r5), "r"(r6) : "cr0", "memory");
	return r3;
}

sc_word_t __do_syscall5(long sc, sc_word_t arg1, sc_word_t arg2,
		sc_word_t arg3, sc_word_t arg4, sc_word_t arg5) {
	register sc_word_t r0 asm("r0") = sc;
	register sc_word_t r3 asm("r3") = arg1;
	register sc_word_t r4 asm("r4") = arg2;
	register sc_word_t r5 asm("r5") = arg3;
	register sc_word_t r6 asm("r6") = arg4;
	register sc_word_t r7 asm("r7") = arg5;
	asm volatile("sc" : "+r"(r3) : "r"(r0), "r"(r4), "r"(r5), "r"(r6), "r"(r7) : "cr0", "memory");
	return r3;
}

sc_word_t __do_syscall6(long sc, sc_word_t arg1, sc_word_t arg2,
		sc_word_t arg3, sc_word_t arg4, sc_word_t arg5, sc_word_t arg6) {
	register sc_word_t r0 asm("r0") = sc;
	register sc_word_t r3 asm("r3") = arg1;
	register sc_word_t r4 asm("r4") = arg2;
	register sc_word_t r5 asm("r5") = arg3;
	register sc_word_t r6 asm("r6") = arg4;
	register sc_word_t r7 asm("r7") = arg5;
	register sc_word_t r8 asm("r8") = arg6;
	asm volatile("sc" : "+r"(r3) : "r"(r0), "r"(r4), "r"(r5), "r"(r6), "r"(r7), "r"(r8) : "cr0", "memory");
	return r3;
}

/*
 * lseek64 receives (fd, offset-low, offset-high, whence) in r3-r6 and
 * returns (low, high) in r3-r4. The C ABI returns a uint64_t with the high
 * word in r3 and low word in r4, so explicitly assemble the value before
 * returning it through the compiler's normal C ABI lowering.
 */
long long __do_syscall4_64(long sc, sc_word_t arg1, sc_word_t arg2,
		sc_word_t arg3, sc_word_t arg4) {
	register sc_word_t r0 asm("r0") = sc;
	register sc_word_t r3 asm("r3") = arg1;
	register sc_word_t r4 asm("r4") = arg2;
	register sc_word_t r5 asm("r5") = arg3;
	register sc_word_t r6 asm("r6") = arg4;
	asm volatile("sc" : "+r"(r3), "+r"(r4)
		: "r"(r0), "r"(r5), "r"(r6) : "cr0", "memory");
	auto result = (static_cast<unsigned long long>(static_cast<unsigned int>(r4)) << 32)
		| static_cast<unsigned int>(r3);
	return static_cast<long long>(result);
}

/*
 * clone(flags, stack, parent_tid, tls_desc, child_tid). The child resumes on
 * the supplied stack with [entry, user_arg] at its top and must not return to
 * the parent's C++ frame. Pop those words, make a 16-byte aligned caller frame,
 * and enter mlibc's non-returning thread trampoline.
 */
#define CHK_STR2(x) #x
#define CHK_STR(x) CHK_STR2(x)
asm(".text\n"
	".globl __mlibc_clone_powerpc\n"
	".type __mlibc_clone_powerpc,@function\n"
	"__mlibc_clone_powerpc:\n\t"
	"li 0," CHK_STR(SYS_clone) "\n\t"
	"sc\n\t"
	"cmpwi 3,0\n\t"
	"bne 1f\n\t"
	"lwz 3,0(1)\n\t"
	"lwz 4,4(1)\n\t"
	"addi 1,1,8\n\t"
	"li 31,0\n\t"
	"stwu 1,-16(1)\n\t"
	"bl __mlibc_enter_thread\n\t"
	"trap\n"
	"1:\n\t"
	"blr\n"
	".size __mlibc_clone_powerpc, .-__mlibc_clone_powerpc\n");
