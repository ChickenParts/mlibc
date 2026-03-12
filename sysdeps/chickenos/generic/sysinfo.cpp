/* sysinfo(), get_nprocs(), get_nprocs_conf() — provided directly by the
 * ChickenOS sysdep to avoid weak-symbol resolution issues with static
 * linking (the Linux option's sys-sysinfo.cpp dispatches through a
 * [[gnu::weak]] mlibc::sys_sysinfo, which LLD may leave as NULL in a
 * statically-linked archive). */

#include <errno.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include <chickenos/syscall.hpp>
#include <bits/syscall.h>

int sysinfo(struct sysinfo *info) {
	auto ret = mlibc::do_syscall(SYS_sysinfo, info);
	if(int e = mlibc::sc_error(ret); e) {
		errno = e;
		return -1;
	}
	return 0;
}

int get_nprocs(void) {
	return sysconf(_SC_NPROCESSORS_ONLN);
}

int get_nprocs_conf(void) {
	return sysconf(_SC_NPROCESSORS_CONF);
}
