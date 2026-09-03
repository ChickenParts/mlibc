#include <errno.h>
#include <sys/mount.h>
#include <sys/statfs.h>

#include <bits/ensure.h>
#include <chickenos/syscall.hpp>

/* These are the [[gnu::weak]] sysdeps options/linux/generic/sys-mount.cpp and
 * sys-statfs.cpp call through (mlibc/linux-sysdeps.hpp); the public mount(2)/
 * statfs(2)/etc. C symbols already live there, over mlibc::sys_mount and
 * friends. This file used to define mount()/umount()/umount2()/statfs()/
 * fstatfs()/fstatfs64() itself instead -- the exact same public symbols --
 * which built fine under static linking only because archive member
 * selection picks whichever definition satisfies a reference first and
 * never complains about the shadowed one. Linking libc.so as one shared
 * object with `-Wl,--no-undefined` surfaces it for what it is: six
 * duplicate-symbol errors. */
namespace mlibc {

int sys_mount(const char *source, const char *target,
		const char *fstype, unsigned long flags, const void *data) {
	auto ret = do_syscall(SYS_mount, source, target, fstype, (long)flags, data);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_umount2(const char *target, int flags) {
	auto ret = do_syscall(SYS_umount2, target, (long)flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_statfs(const char *path, struct statfs *buf) {
	auto ret = do_syscall(SYS_statfs, path, buf);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_fstatfs(int fd, struct statfs *buf) {
	auto ret = do_syscall(SYS_fstatfs, (long)fd, buf);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

} // namespace mlibc
