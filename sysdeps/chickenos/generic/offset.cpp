#include <errno.h>
#include <mlibc/all-sysdeps.hpp>
#include <chickenos/syscall.hpp>
#include <sys/mman.h>

namespace mlibc {

int sys_anon_allocate(size_t size, void **pointer) {
	auto ret = do_syscall(SYS_mmap, 0, size,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(int e = sc_error(ret); e)
		return e;
	*pointer = sc_ptr_result<void>(ret);
	return 0;
}

int sys_seek(int fd, off_t offset, int whence, off_t *new_offset) {
	auto ret = do_syscall(SYS_lseek, fd, offset, whence);
	if(int e = sc_error(ret); e)
		return e;
	*new_offset = sc_int_result<off_t>(ret);
	return 0;
}

int sys_vm_map(void *hint, size_t size, int prot, int flags,
		int fd, off_t offset, void **window) {
	auto ret = do_syscall(SYS_mmap, hint, size, prot, flags, fd, offset);
	// mmap returns MAP_FAILED on error
	auto addr = sc_int_result<long>(ret);
	if(addr >= -4096L && addr < 0) {
		return -addr;
	}
	*window = reinterpret_cast<void *>(addr);
	return 0;
}

int sys_truncate(const char *path, off_t length) {
	auto ret = do_syscall(SYS_truncate, path, length);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_ftruncate(int fd, size_t size) {
	auto ret = do_syscall(SYS_ftruncate, fd, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_pread(int fd, void *buf, size_t n, off_t off, ssize_t *bytes_read) {
	auto ret = do_syscall(SYS_pread64, fd, buf, n, off);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_read = sc_int_result<ssize_t>(ret);
	return 0;
}

int sys_pwrite(int fd, const void *buf, size_t n, off_t off, ssize_t *bytes_written) {
	auto ret = do_syscall(SYS_pwrite64, fd, buf, n, off);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_written = sc_int_result<ssize_t>(ret);
	return 0;
}

} // namespace mlibc
