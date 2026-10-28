/* SPDX-License-Identifier: MIT */
#include <chickenos/offset-syscalls.h>
#include <chickenos/syscall.hpp>
#include <errno.h>
#include <mlibc/all-sysdeps.hpp>
#include <sys/mman.h>

namespace mlibc {

int sys_anon_allocate(size_t size, void **pointer) {
	auto ret = static_cast<sc_result_t>(
	    __chicken_mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
	);
	if (int e = sc_error(ret); e)
		return e;
	*pointer = sc_ptr_result<void>(ret);
	return 0;
}
int sys_seek(int fd, off_t offset, int whence, off_t *new_offset) {
	auto ret = __chicken_seek(fd, offset, whence);
	if (ret < 0 && ret >= -4095)
		return -static_cast<int>(ret);
	*new_offset = static_cast<off_t>(ret);
	return 0;
}
int sys_vm_map(void *hint, size_t size, int prot, int flags, int fd, off_t offset, void **window) {
	if (!__chicken_mmap_offset_valid(offset))
		return EINVAL;
	auto ret = static_cast<sc_result_t>(
	    __chicken_mmap(reinterpret_cast<long>(hint), size, prot, flags, fd, offset)
	);
	auto addr = sc_int_result<long>(ret);
	if (addr >= -4096L && addr < 0)
		return -addr;
	*window = reinterpret_cast<void *>(addr);
	return 0;
}
int sys_truncate(const char *path, off_t length) {
	auto ret = static_cast<sc_result_t>(__chicken_truncate(reinterpret_cast<long>(path), length));
	if (int e = sc_error(ret); e)
		return e;
	return 0;
}
int sys_ftruncate(int fd, size_t size) {
	auto ret = static_cast<sc_result_t>(__chicken_ftruncate(fd, size));
	if (int e = sc_error(ret); e)
		return e;
	return 0;
}
int sys_pread(int fd, void *buf, size_t n, off_t off, ssize_t *bytes_read) {
	auto ret = static_cast<sc_result_t>(__chicken_pread(fd, reinterpret_cast<long>(buf), n, off));
	if (int e = sc_error(ret); e)
		return e;
	*bytes_read = sc_int_result<ssize_t>(ret);
	return 0;
}
int sys_pwrite(int fd, const void *buf, size_t n, off_t off, ssize_t *bytes_written) {
	auto ret = static_cast<sc_result_t>(__chicken_pwrite(fd, reinterpret_cast<long>(buf), n, off));
	if (int e = sc_error(ret); e)
		return e;
	*bytes_written = sc_int_result<ssize_t>(ret);
	return 0;
}
} // namespace mlibc
