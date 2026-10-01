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

/*
 * clone(2) for a new thread.  cdecl: (flags, stack, parent_tid, tls_desc, child_tid).
 * The child resumes right after the `int $0x80` with EAX == 0 and ESP == `stack`,
 * whose top two words Sysdeps<PrepareStack> filled with [entry][user_arg]; it
 * must therefore never return into the C++ caller (that frame is on the parent's
 * stack).  It pops both and calls __mlibc_enter_thread(entry, user_arg), which
 * does not return.  ESP is 16-byte aligned at that call.
 */
#define CHK_STR2(x) #x
#define CHK_STR(x) CHK_STR2(x)
asm(".text\n"
	".globl __mlibc_clone_i386\n"
	".type __mlibc_clone_i386,@function\n"
	"__mlibc_clone_i386:\n\t"
	"pushl %ebx\n\t"
	"pushl %esi\n\t"
	"pushl %edi\n\t"
	"movl 16(%esp), %ebx\n\t"
	"movl 20(%esp), %ecx\n\t"
	"movl 24(%esp), %edx\n\t"
	"movl 28(%esp), %esi\n\t"
	"movl 32(%esp), %edi\n\t"
	"movl $" CHK_STR(SYS_clone) ", %eax\n\t"
	"int $0x80\n\t"
	"testl %eax, %eax\n\t"
	"jnz 1f\n\t"
	"popl %eax\n\t"
	"popl %ecx\n\t"
	"xorl %ebp, %ebp\n\t"
	"subl $8, %esp\n\t"
	"pushl %ecx\n\t"
	"pushl %eax\n\t"
	"call __mlibc_enter_thread\n\t"
	"ud2\n"
	"1:\n\t"
	"popl %edi\n\t"
	"popl %esi\n\t"
	"popl %ebx\n\t"
	"ret\n"
	".size __mlibc_clone_i386, .-__mlibc_clone_i386\n");
