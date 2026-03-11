/*
 * Yolk sysdeps for mlibc - rtld syscall shims
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>

#include <mlibc/all-sysdeps.hpp>

#include <yolk/syscall.h>

namespace mlibc {

static inline int sc_errno(long result) {
	return (int)(-result);
}

void sys_libc_log(const char *message) {
	if (!message) {
		return;
	}
	size_t len = 0;
	while (message[len] != '\0') {
		len++;
	}
	__syscall3(SYS_write, 2, (long)message, len);
}

[[noreturn]] void sys_libc_panic() {
	sys_libc_log("mlibc: panic!\n");
	__syscall1(SYS_exit_group_core, 127);
	__builtin_unreachable();
}

int sys_tcb_set(void *pointer) {
#if defined(__x86_64__)
	constexpr long ARCH_SET_FS = 0x1002;
	long result = __syscall2(SYS_arch_prctl, ARCH_SET_FS, (long)pointer);
	return (result < 0) ? sc_errno(result) : 0;
#elif defined(__aarch64__)
	__asm__ volatile("msr tpidr_el0, %0" :: "r"(pointer) : "memory");
	return 0;
#elif defined(__riscv)
	__asm__ volatile("mv tp, %0" :: "r"(pointer) : "memory");
	return 0;
#else
	return ENOSYS;
#endif
}

int sys_anon_allocate(size_t size, void **pointer) {
	long result = __syscall6(SYS_mmap, 0, size, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result < 0) {
		return sc_errno(result);
	}
	*pointer = (void *)result;
	return 0;
}

int sys_anon_free(void *pointer, size_t size) {
	long result = __syscall2(SYS_munmap, (long)pointer, size);
	return (result < 0) ? sc_errno(result) : 0;
}

int sys_vm_map(void *hint, size_t size, int prot, int flags, int fd, off_t offset,
	       void **window) {
	long result = __syscall6(SYS_mmap, (long)hint, size, prot, flags, fd, offset);
	if (result < 0) {
		return sc_errno(result);
	}
	*window = (void *)result;
	return 0;
}

int sys_vm_unmap(void *pointer, size_t size) {
	long result = __syscall2(SYS_munmap, (long)pointer, size);
	return (result < 0) ? sc_errno(result) : 0;
}

int sys_open(const char *path, int flags, mode_t mode, int *fd) {
	long result = __syscall4(SYS_open_core, (long)path, flags, mode, AT_FDCWD);
	if (result < 0) {
		return sc_errno(result);
	}
	*fd = (int)result;
	return 0;
}

int sys_close(int fd) {
	long result = __syscall1(SYS_close_core, fd);
	return (result < 0) ? sc_errno(result) : 0;
}

int sys_read(int fd, void *buf, size_t count, ssize_t *bytes_read) {
	long result = __syscall3(SYS_read, fd, (long)buf, count);
	if (result < 0) {
		return sc_errno(result);
	}
	*bytes_read = (ssize_t)result;
	return 0;
}

int sys_seek(int fd, off_t offset, int whence, off_t *new_offset) {
	long result = __syscall3(SYS_lseek, fd, offset, whence);
	if (result < 0) {
		return sc_errno(result);
	}
	*new_offset = (off_t)result;
	return 0;
}

int sys_futex_wait(int *pointer, int expected, const struct timespec *time) {
	long result = __syscall4(SYS_futex_wait, (long)pointer, expected, (long)time, 0);
	return (result < 0) ? sc_errno(result) : 0;
}

int sys_futex_wake(int *pointer) {
	long result = __syscall2(SYS_futex_wake, (long)pointer, INT_MAX);
	return (result < 0) ? sc_errno(result) : 0;
}

} // namespace mlibc
