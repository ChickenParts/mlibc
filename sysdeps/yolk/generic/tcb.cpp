/* SPDX-License-Identifier: MIT */
#include <mlibc/all-sysdeps.hpp>
#include <yolk/syscall.h>
#include <stddef.h>

#undef sys_tcb_set
#undef sys_tcb_get

namespace mlibc {

int sys_tcb_set(void *pointer)
{
#if defined(__x86_64__)
	long result = __syscall2(SYS_arch_prctl, ARCH_SET_FS, (long)pointer);
	return result < 0 ? (int)-result : 0;
#elif defined(__aarch64__)
	__asm__ volatile("msr tpidr_el0, %0" :: "r"(pointer) : "memory");
	return 0;
#elif defined(__riscv) && __riscv_xlen == 64
	__asm__ volatile("mv tp, %0" :: "r"(pointer) : "memory");
	return 0;
#else
#error "Unsupported Yolk architecture"
#endif
}

void *sys_tcb_get()
{
#if defined(__x86_64__)
	void *pointer = nullptr;
	long result = __syscall2(SYS_arch_prctl, ARCH_GET_FS, (long)&pointer);
	return result < 0 ? nullptr : pointer;
#elif defined(__aarch64__)
	void *pointer;
	__asm__ volatile("mrs %0, tpidr_el0" : "=r"(pointer));
	return pointer;
#elif defined(__riscv) && __riscv_xlen == 64
	void *pointer;
	__asm__ volatile("mv %0, tp" : "=r"(pointer));
	return pointer;
#else
#error "Unsupported Yolk architecture"
#endif
}

} // namespace mlibc
