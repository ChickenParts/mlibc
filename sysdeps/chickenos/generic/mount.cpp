#include <errno.h>
#include <sys/mount.h>
#include <sys/statfs.h>

#include <bits/ensure.h>
#include <chickenos/syscall.hpp>

extern "C" {

int mount(const char *source, const char *target,
		const char *fstype, unsigned long flags, const void *data) {
	auto ret = mlibc::do_syscall(SYS_mount, source, target, fstype,
			(long)flags, data);
	if(int e = mlibc::sc_error(ret); e) {
		errno = e;
		return -1;
	}
	return 0;
}

int umount(const char *target) {
	return umount2(target, 0);
}

int umount2(const char *target, int flags) {
	auto ret = mlibc::do_syscall(SYS_umount2, target, (long)flags);
	if(int e = mlibc::sc_error(ret); e) {
		errno = e;
		return -1;
	}
	return 0;
}

int statfs(const char *path, struct statfs *buf) {
	auto ret = mlibc::do_syscall(SYS_statfs, path, buf);
	if(int e = mlibc::sc_error(ret); e) {
		errno = e;
		return -1;
	}
	return 0;
}

int fstatfs(int fd, struct statfs *buf) {
	auto ret = mlibc::do_syscall(SYS_fstatfs, (long)fd, buf);
	if(int e = mlibc::sc_error(ret); e) {
		errno = e;
		return -1;
	}
	return 0;
}

int fstatfs64(int fd, struct statfs64 *buf) {
	return fstatfs(fd, reinterpret_cast<struct statfs *>(buf));
}

} // extern "C"
