#include <errno.h>
#include <string.h>
#include <limits.h>

#include <type_traits>

#include <bits/ensure.h>
#include <abi-bits/fcntl.h>
#include <abi-bits/socklen_t.h>
#include <mlibc/allocator.hpp>
#include <mlibc/debug.hpp>
#include <mlibc/all-sysdeps.hpp>
#include <mlibc/dlapi.hpp>
#include <chickenos/syscall.hpp>
#include <sys/ioctl.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sched.h>

namespace {
	/* Resolved once at load time.  __dlapi_vdsosym lives in the rtld, so a
	 * statically linked program can never bind a vDSO symbol.
	 *
	 * ⚠ It is NOT enough to let it return nullptr, which is what an
	 * earlier version of this comment claimed.  In a static link
	 * __dlapi_vdsosym is not absent, it is an undefined REFERENCE, and
	 * libc.a contains the rtld's objects (meson's libc_static
	 * link_wholes ld_static_lib).  So lld satisfies it by extracting
	 * options_rtld_generic_main.cpp.o, which drags in the rtld's own
	 * copies of sysdeps.cpp, the allocator, frigg and six more objects
	 * -- all of which the libc copies already define:
	 *
	 *   ld.lld: error: duplicate symbol: mlibc::sys_exit(int)
	 *   >>> ... sysdeps_chickenos_generic_sysdeps.cpp.o in archive libc.a
	 *   >>> ... sysdeps_chickenos_generic_sysdeps.cpp.o in archive libc.a
	 *
	 * and no static binary can link against the libc at all.  Confirmed
	 * with --why-extract: the edge is
	 *   sysdeps.cpp.o -> options_rtld_generic_main.cpp.o  __dlapi_vdsosym
	 *
	 * MLIBC_STATIC_BUILD is defined for libc_static and ld_static_lib
	 * only (meson.build's static_cpp_args), so the shared build keeps the
	 * vDSO fast path -- which is the only build that could ever use it.
	 *
	 * The pointer is namespace-scope and constant-initialised, following
	 * upstream sysdeps/linux/generic/sysdeps.cpp.  It must NOT be a
	 * function-local static: a local static with a non-constant
	 * initialiser emits __cxa_guard_acquire/__cxa_guard_release, which
	 * ld.so cannot provide (it is linked -nostdlib and --no-undefined), so
	 * the rtld link died on
	 *   ld.lld: error: undefined symbol: __cxa_guard_acquire
	 *   >>> referenced by sysdeps.cpp:49 ... SysdepImpl<ClockGet>::operator()
	 * The rtld must not call into the vDSO at all, so MLIBC_BUILDING_RTLD
	 * pins the pointer to nullptr there.
	 */
#ifndef MLIBC_STATIC_BUILD
	using vdso_clock_gettime_t = int (*)(int, struct timespec *);

#if !MLIBC_BUILDING_RTLD
	vdso_clock_gettime_t vdso_clock_gettime = reinterpret_cast<vdso_clock_gettime_t>(
			__dlapi_vdsosym("__vdso_clock_gettime", "CHICKEN_1.0"));
#else
	vdso_clock_gettime_t vdso_clock_gettime = nullptr;
#endif

	using vdso_getcpu_t = long (*)(unsigned *, unsigned *, void *);

#if !MLIBC_BUILDING_RTLD
	vdso_getcpu_t vdso_getcpu = reinterpret_cast<vdso_getcpu_t>(
			__dlapi_vdsosym("__vdso_getcpu", "CHICKEN_1.0"));
#else
	vdso_getcpu_t vdso_getcpu = nullptr;
#endif
#endif
}

#define STUB_ONLY { \
	mlibc::infoLogger() << "mlibc: " << __func__ << " is a stub" << frg::endlog; \
	return ENOSYS; \
}

namespace mlibc {

void Sysdeps<LibcLog>::operator()(const char *message) {
	size_t n = 0;
	while(message[n])
		n++;
	do_syscall(SYS_write, 2, message, n);
	char lf = '\n';
	do_syscall(SYS_write, 2, &lf, 1);
}

[[noreturn]] void Sysdeps<LibcPanic>::operator()() {
	sysdep<LibcLog>("mlibc: PANIC!");
	__builtin_trap();
}

int Sysdeps<TcbSet>::operator()(void *pointer) {
#if defined(__x86_64__)
	auto ret = do_syscall(SYS_arch_prctl, 0x1002 /* ARCH_SET_FS */, pointer);
	if(int e = sc_error(ret); e)
		return e;
#elif defined(__aarch64__)
	/* ⚠ + sizeof(Tcb) - 0x10, not the raw pointer.  sys_tcb_set is handed the
	 * TCB address, but the thread pointer is defined relative to the TLS
	 * blocks that FOLLOW it: aarch64-include/mlibc/thread.hpp recovers the TCB
	 * as `tpidr_el0 + 0x10 - sizeof(Tcb)`, so handing the kernel the bare TCB
	 * address makes get_current_tcb() compute an address sizeof(Tcb) - 0x10
	 * short and fail its `__ensure(tcb == tcb->selfPointer)`.  Same adjustment
	 * upstream's Linux port applies.
	 *
	 * ⚠ UNVERIFIED on hardware: aarch64 has no cross-built userland that has
	 * run yet.  Fixed by inspection off the riscv64 measurement below, which
	 * was the identical defect and did fail exactly this way. */
	uintptr_t thread_data = reinterpret_cast<uintptr_t>(pointer) + sizeof(Tcb) - 0x10;
	auto ret = do_syscall(SYS_set_thread_area, thread_data);
	if(int e = sc_error(ret); e)
		return e;
#elif defined(__riscv)
	/* ⚠ + sizeof(Tcb).  riscv64-include/mlibc/thread.hpp recovers the TCB as
	 * `tp - sizeof(Tcb)`, so setting tp to the bare TCB address puts every
	 * lookup one Tcb too low.  Measured on ChickenOS/riscv64 -- dash reached
	 * userspace, ran, and died on
	 *
	 *   In function get_current_tcb, .../riscv64-include/mlibc/thread.hpp:13
	 *   __ensure(tcb == tcb->selfPointer) failed
	 *   mlibc: PANIC!
	 *
	 * having logged it through a working write() syscall.  Matches upstream's
	 * Linux riscv branch exactly. */
	uintptr_t thread_data = reinterpret_cast<uintptr_t>(pointer) + sizeof(Tcb);
	asm volatile("mv tp, %0" :: "r"(thread_data));
#endif
	return 0;
}

int Sysdeps<AnonAllocate>::operator()(size_t size, void **pointer) {
	auto ret = do_syscall(SYS_mmap, 0, size,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(int e = sc_error(ret); e)
		return e;
	*pointer = sc_ptr_result<void>(ret);
	return 0;
}

int Sysdeps<AnonFree>::operator()(void *pointer, size_t size) {
	auto ret = do_syscall(SYS_munmap, pointer, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Open>::operator()(const char *path, int flags, mode_t mode, int *fd) {
	auto ret = do_syscall(SYS_open, path, flags, mode);
	if(int e = sc_error(ret); e)
		return e;
	*fd = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<Read>::operator()(int fd, void *buffer, size_t size, ssize_t *bytes_read) {
	auto ret = do_syscall(SYS_read, fd, buffer, size);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_read = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<Write>::operator()(int fd, const void *buffer, size_t size, ssize_t *bytes_written) {
	auto ret = do_syscall(SYS_write, fd, buffer, size);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_written = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<Seek>::operator()(int fd, off_t offset, int whence, off_t *new_offset) {
	auto ret = do_syscall(SYS_lseek, fd, offset, whence);
	if(int e = sc_error(ret); e)
		return e;
	*new_offset = sc_int_result<off_t>(ret);
	return 0;
}

int Sysdeps<Close>::operator()(int fd) {
	auto ret = do_syscall(SYS_close, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Stat>::operator()(fsfd_target fsfdt, int fd, const char *path, [[maybe_unused]] int flags,
		struct stat *statbuf) {
	sc_result_t ret;
	if(fsfdt == fsfd_target::path) {
		ret = do_syscall(SYS_stat64, path, statbuf);
	} else if(fsfdt == fsfd_target::fd) {
		ret = do_syscall(SYS_fstat64, fd, statbuf);
	} else {
		__ensure(fsfdt == fsfd_target::fd_path);
		ret = do_syscall(SYS_fstat64, fd, statbuf);
	}
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<VmMap>::operator()(void *hint, size_t size, int prot, int flags,
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

int Sysdeps<VmUnmap>::operator()(void *pointer, size_t size) {
	auto ret = do_syscall(SYS_munmap, pointer, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<VmProtect>::operator()(void *pointer, size_t size, int prot) {
	auto ret = do_syscall(SYS_mprotect, pointer, size, prot);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<FutexWait>::operator()(int *pointer, int expected, const struct timespec *time) {
	// ChickenOS futex: FUTEX_WAIT = 0
	auto ret = do_syscall(SYS_futex, pointer, 0 /* FUTEX_WAIT */, expected, time);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<FutexWake>::operator()(int *pointer, bool all) {
	// ChickenOS futex: FUTEX_WAKE = 1
	auto ret = do_syscall(SYS_futex, pointer, 1 /* FUTEX_WAKE */, all ? INT_MAX : 1);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<ClockGet>::operator()(int clock, time_t *secs, long *nanos) {
	struct timespec ts;

	/* CLOCK_MONOTONIC's sub-tick interpolation uses a per-CPU anchor
	 * userspace cannot hold, so the vDSO cannot answer it from vvar --
	 * but as of v0.4.6a's F9/R23 it no longer just refuses: it FORWARDS
	 * CLOCK_MONOTONIC (and any id it does not recognise) to the real
	 * syscall itself and returns exactly what the kernel returned, so
	 * `rc` is essentially never -ENOSYS in practice. This fallthrough on
	 * ENOSYS is still correct defence -- an unreadable vvar page still
	 * returns it -- it just is not the vDSO's everyday refusal path that
	 * an earlier version of this comment described. */
#ifndef MLIBC_STATIC_BUILD
	if (vdso_clock_gettime) {
		struct timespec vts;
		int rc = vdso_clock_gettime(clock, &vts);
		if (rc == 0) {
			*secs = vts.tv_sec;
			*nanos = vts.tv_nsec;
			return 0;
		}
		if (rc != -ENOSYS) return -rc;
	}
#endif

	auto ret = do_syscall(SYS_clock_gettime, clock, &ts);
	if(int e = sc_error(ret); e)
		return e;
	*secs = ts.tv_sec;
	*nanos = ts.tv_nsec;
	return 0;
}

int Sysdeps<Isatty>::operator()(int fd) {
	auto ret = do_syscall(SYS_isatty, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

[[noreturn]] void Sysdeps<Exit>::operator()(int status) {
	do_syscall(SYS_exit_group, status);
	__builtin_trap();
}

// ---------------------------------------------------------------------------
// POSIX Level 2 — File operations
// ---------------------------------------------------------------------------

#if __MLIBC_POSIX_OPTION

int Sysdeps<Openat>::operator()(int dirfd, const char *path, int flags, mode_t mode, int *fd) {
	auto ret = do_syscall(SYS_openat, dirfd, path, flags, mode);
	if(int e = sc_error(ret); e)
		return e;
	*fd = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<Dup>::operator()(int fd, int flags, int *newfd) {
	(void)flags;
	auto ret = do_syscall(SYS_dup, fd);
	if(int e = sc_error(ret); e)
		return e;
	*newfd = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<Dup2>::operator()(int fd, int flags, int newfd) {
	sc_result_t ret;
	if(flags == 0)
		ret = do_syscall(SYS_dup2, fd, newfd, 0);
	else
		ret = do_syscall(SYS_dup3, fd, newfd, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Fcntl>::operator()(int fd, int cmd, va_list args, int *result) {
	auto arg = va_arg(args, unsigned long);
	auto ret = do_syscall(SYS_fcntl64, fd, cmd, arg);
	if(int e = sc_error(ret); e)
		return e;
	*result = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<Ioctl>::operator()(int fd, unsigned long request, void *arg, int *result) {
	auto ret = do_syscall(SYS_ioctl, fd, request, arg);
	if(int e = sc_error(ret); e)
		return e;
	*result = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<GetCwd>::operator()(char *buf, size_t size) {
	auto ret = do_syscall(SYS_getcwd, buf, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Chmod>::operator()(const char *pathname, mode_t mode) {
	auto ret = do_syscall(SYS_chmod, pathname, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Fchmod>::operator()(int fd, mode_t mode) {
	auto ret = do_syscall(SYS_fchmod, fd, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Fchmodat>::operator()(int fd, const char *pathname, mode_t mode, int flags) {
	auto ret = do_syscall(SYS_fchmodat, fd, pathname, mode, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Fchownat>::operator()(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) {
	auto ret = do_syscall(SYS_fchownat, dirfd, pathname, owner, group, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Access>::operator()(const char *path, int mode) {
	auto ret = do_syscall(SYS_access, path, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Faccessat>::operator()(int dirfd, const char *pathname, int mode, int flags) {
	auto ret = do_syscall(SYS_faccessat, dirfd, pathname, mode, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Pipe>::operator()(int *fds, int flags) {
	auto ret = do_syscall(SYS_pipe, fds, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Unlinkat>::operator()(int dfd, const char *path, int flags) {
	auto ret = do_syscall(SYS_unlinkat, dfd, path, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Mkdir>::operator()(const char *path, mode_t mode) {
	auto ret = do_syscall(SYS_mkdir, path, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Mkdirat>::operator()(int dirfd, const char *path, mode_t mode) {
	auto ret = do_syscall(SYS_mkdirat, dirfd, path, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Symlink>::operator()(const char *target_path, const char *link_path) {
	auto ret = do_syscall(SYS_symlink, target_path, link_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Symlinkat>::operator()(const char *target_path, int dirfd, const char *link_path) {
	auto ret = do_syscall(SYS_symlinkat, target_path, dirfd, link_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Link>::operator()(const char *old_path, const char *new_path) {
	auto ret = do_syscall(SYS_link, old_path, new_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Linkat>::operator()(int olddirfd, const char *old_path, int newdirfd, const char *new_path, int flags) {
	auto ret = do_syscall(SYS_linkat, olddirfd, old_path, newdirfd, new_path, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Rename>::operator()(const char *old_path, const char *new_path) {
	auto ret = do_syscall(SYS_rename, old_path, new_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Renameat>::operator()(int old_dirfd, const char *old_path, int new_dirfd, const char *new_path) {
	auto ret = do_syscall(SYS_renameat, old_dirfd, old_path, new_dirfd, new_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Rmdir>::operator()(const char *path) {
	auto ret = do_syscall(SYS_rmdir, path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Readlink>::operator()(const char *path, void *buf, size_t bufsiz, ssize_t *len) {
	auto ret = do_syscall(SYS_readlink, path, buf, bufsiz);
	if(int e = sc_error(ret); e)
		return e;
	*len = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<Readlinkat>::operator()(int dirfd, const char *path, void *buffer, size_t max_size, ssize_t *length) {
	auto ret = do_syscall(SYS_readlinkat, dirfd, path, buffer, max_size);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<Truncate>::operator()(const char *path, off_t length) {
	auto ret = do_syscall(SYS_truncate, path, length);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Ftruncate>::operator()(int fd, size_t size) {
	auto ret = do_syscall(SYS_ftruncate, fd, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Flock>::operator()(int fd, int options) {
	auto ret = do_syscall(SYS_flock, fd, options);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Chdir>::operator()(const char *path) {
	auto ret = do_syscall(SYS_chdir, path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Fchdir>::operator()(int fd) {
	auto ret = do_syscall(SYS_fchdir, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Umask>::operator()(mode_t mode, mode_t *old) {
	auto ret = do_syscall(SYS_umask, mode);
	if(int e = sc_error(ret); e)
		return e;
	*old = sc_int_result<mode_t>(ret);
	return 0;
}

int Sysdeps<Fsync>::operator()(int fd) {
	auto ret = do_syscall(SYS_fsync, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Fdatasync>::operator()(int fd) {
	auto ret = do_syscall(SYS_fdatasync, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

void Sysdeps<Sync>::operator()() {
	do_syscall(SYS_sync);
}

// ---------------------------------------------------------------------------
// Process operations
// ---------------------------------------------------------------------------

int Sysdeps<Fork>::operator()(pid_t *child) {
	auto ret = do_syscall(SYS_fork);
	if(int e = sc_error(ret); e)
		return e;
	*child = sc_int_result<pid_t>(ret);
	return 0;
}

int Sysdeps<Execve>::operator()(const char *path, char *const argv[], char *const envp[]) {
	auto ret = do_syscall(SYS_execve, path, argv, envp);
	if(int e = sc_error(ret); e)
		return e;
	// execve should not return on success
	__builtin_unreachable();
}

int Sysdeps<Waitpid>::operator()(pid_t pid, int *status, int flags, struct rusage *ru, pid_t *ret_pid) {
	auto ret = do_syscall(SYS_wait4, pid, status, flags, ru);
	if(int e = sc_error(ret); e)
		return e;
	*ret_pid = sc_int_result<pid_t>(ret);
	return 0;
}

int Sysdeps<Kill>::operator()(int pid, int sig) {
	auto ret = do_syscall(SYS_kill, pid, sig);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Tgkill>::operator()(int tgid, int tid, int sig) {
	auto ret = do_syscall(SYS_tgkill, tgid, tid, sig);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Sigaction>::operator()(int signum, const struct sigaction *__restrict act,
		struct sigaction *__restrict oldact) {
	auto ret = do_syscall(SYS_rt_sigaction, signum, act, oldact, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Sigprocmask>::operator()(int how, const sigset_t *__restrict set, sigset_t *__restrict old) {
	auto ret = do_syscall(SYS_rt_sigprocmask, how, set, old, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Sigpending>::operator()(sigset_t *set) {
	auto ret = do_syscall(SYS_rt_sigpending, set, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Sigsuspend>::operator()(const sigset_t *set) {
	auto ret = do_syscall(SYS_rt_sigsuspend, set, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Sigaltstack>::operator()(const stack_t *ss, stack_t *oss) {
	auto ret = do_syscall(SYS_sigaltstack, ss, oss);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

// ---------------------------------------------------------------------------
// Process identity
// ---------------------------------------------------------------------------

pid_t Sysdeps<GetPid>::operator()() {
	auto ret = do_syscall(SYS_getpid);
	return sc_int_result<pid_t>(ret);
}

pid_t Sysdeps<GetPpid>::operator()() {
	auto ret = do_syscall(SYS_getppid);
	return sc_int_result<pid_t>(ret);
}

pid_t Sysdeps<GetTid>::operator()() {
	auto ret = do_syscall(SYS_gettid);
	return sc_int_result<pid_t>(ret);
}

uid_t Sysdeps<GetUid>::operator()() {
	auto ret = do_syscall(SYS_getuid);
	return sc_int_result<uid_t>(ret);
}

uid_t Sysdeps<GetEuid>::operator()() {
	auto ret = do_syscall(SYS_geteuid);
	return sc_int_result<uid_t>(ret);
}

gid_t Sysdeps<GetGid>::operator()() {
	auto ret = do_syscall(SYS_getgid);
	return sc_int_result<gid_t>(ret);
}

gid_t Sysdeps<GetEgid>::operator()() {
	auto ret = do_syscall(SYS_getegid);
	return sc_int_result<gid_t>(ret);
}

int Sysdeps<SetPgid>::operator()(pid_t pid, pid_t pgid) {
	auto ret = do_syscall(SYS_setpgid, pid, pgid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<GetPgid>::operator()(pid_t pid, pid_t *out) {
	auto ret = do_syscall(SYS_getpgid, pid);
	if(int e = sc_error(ret); e)
		return e;
	*out = sc_int_result<pid_t>(ret);
	return 0;
}

int Sysdeps<GetSid>::operator()(pid_t pid, pid_t *sid) {
	auto ret = do_syscall(SYS_getsid, pid);
	if(int e = sc_error(ret); e)
		return e;
	*sid = sc_int_result<pid_t>(ret);
	return 0;
}

int Sysdeps<SetSid>::operator()(pid_t *sid) {
	auto ret = do_syscall(SYS_setsid);
	if(int e = sc_error(ret); e)
		return e;
	*sid = sc_int_result<pid_t>(ret);
	return 0;
}

int Sysdeps<SetUid>::operator()(uid_t uid) {
	auto ret = do_syscall(SYS_setuid, uid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetGid>::operator()(gid_t gid) {
	auto ret = do_syscall(SYS_setgid, gid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<GetGroups>::operator()(size_t size, gid_t *list, int *retval) {
	auto ret = do_syscall(SYS_getgroups, size, list);
	if(int e = sc_error(ret); e)
		return e;
	*retval = sc_int_result<int>(ret);
	return 0;
}

// ---------------------------------------------------------------------------
// I/O multiplexing
// ---------------------------------------------------------------------------

int Sysdeps<Poll>::operator()(struct pollfd *fds, nfds_t count, int timeout, int *num_events) {
	auto ret = do_syscall(SYS_poll, fds, count, timeout);
	if(int e = sc_error(ret); e)
		return e;
	*num_events = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<EpollCreate>::operator()(int flags, int *fd) {
	auto ret = do_syscall(SYS_epoll_create1, flags);
	if(int e = sc_error(ret); e)
		return e;
	*fd = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<EpollCtl>::operator()(int epfd, int mode, int fd, struct epoll_event *ev) {
	auto ret = do_syscall(SYS_epoll_ctl, epfd, mode, fd, ev);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<EpollPwait>::operator()(int epfd, struct epoll_event *ev, int n, int timeout,
		const sigset_t *sigmask, int *raised) {
	auto ret = do_syscall(SYS_epoll_pwait, epfd, ev, n, timeout, sigmask);
	if(int e = sc_error(ret); e)
		return e;
	*raised = sc_int_result<int>(ret);
	return 0;
}

// ---------------------------------------------------------------------------
// Socket operations
// ---------------------------------------------------------------------------

int Sysdeps<Socket>::operator()(int domain, int type, int protocol, int *fd) {
	auto ret = do_syscall(SYS_socket, domain, type, protocol);
	if(int e = sc_error(ret); e)
		return e;
	*fd = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<Socketpair>::operator()(int domain, int type_and_flags, int proto, int *fds) {
	auto ret = do_syscall(SYS_socketpair, domain, type_and_flags, proto, fds);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Bind>::operator()(int fd, const struct sockaddr *addr_ptr, socklen_t addr_length) {
	auto ret = do_syscall(SYS_bind, fd, addr_ptr, addr_length);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Listen>::operator()(int fd, int backlog) {
	auto ret = do_syscall(SYS_listen, fd, backlog);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Accept>::operator()(int fd, int *newfd, struct sockaddr *addr_ptr,
		socklen_t *addr_length, int flags) {
	auto ret = do_syscall(SYS_accept4, fd, addr_ptr, addr_length, flags);
	if(int e = sc_error(ret); e)
		return e;
	*newfd = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<Connect>::operator()(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	auto ret = do_syscall(SYS_connect, sockfd, addr, addrlen);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Shutdown>::operator()(int sockfd, int how) {
	auto ret = do_syscall(SYS_shutdown, sockfd, how);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<MsgSend>::operator()(int sockfd, const struct msghdr *msg, int flags, ssize_t *length) {
	auto ret = do_syscall(SYS_sendmsg, sockfd, msg, flags);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<MsgRecv>::operator()(int sockfd, struct msghdr *msg, int flags, ssize_t *length) {
	auto ret = do_syscall(SYS_recvmsg, sockfd, msg, flags);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<SetSockopt>::operator()(int fd, int layer, int number, const void *buffer, socklen_t size) {
	auto ret = do_syscall(SYS_setsockopt, fd, layer, number, buffer, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<GetSockopt>::operator()(int fd, int layer, int number, void *__restrict buffer,
		socklen_t *__restrict size) {
	auto ret = do_syscall(SYS_getsockopt, fd, layer, number, buffer, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Sockname>::operator()(int fd, struct sockaddr *addr_ptr, socklen_t max_addr_length,
		socklen_t *actual_length) {
	auto ret = do_syscall(SYS_getsockname, fd, addr_ptr, &max_addr_length);
	if(int e = sc_error(ret); e)
		return e;
	*actual_length = max_addr_length;
	return 0;
}

int Sysdeps<Peername>::operator()(int fd, struct sockaddr *addr_ptr, socklen_t max_addr_length,
		socklen_t *actual_length) {
	auto ret = do_syscall(SYS_getpeername, fd, addr_ptr, &max_addr_length);
	if(int e = sc_error(ret); e)
		return e;
	*actual_length = max_addr_length;
	return 0;
}

// ---------------------------------------------------------------------------
// Terminal operations
// ---------------------------------------------------------------------------

int Sysdeps<Tcgetattr>::operator()(int fd, struct termios *attr) {
	auto ret = do_syscall(SYS_tcgetattr, fd, attr);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Tcsetattr>::operator()(int fd, int optional_action, const struct termios *attr) {
	auto ret = do_syscall(SYS_tcsetattr, fd, optional_action, attr);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Tcgetwinsize>::operator()(int fd, struct winsize *winsz) {
	auto ret = do_syscall(SYS_tcgetwinsize, fd, winsz);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Tcsetwinsize>::operator()(int fd, const struct winsize *winsz) {
	auto ret = do_syscall(SYS_tcsetwinsize, fd, winsz);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

// ---------------------------------------------------------------------------
// Sleep
// ---------------------------------------------------------------------------

int Sysdeps<Sleep>::operator()(time_t *secs, long *nanos) {
	struct timespec req = {*secs, *nanos};
	struct timespec rem = {};
	auto ret = do_syscall(SYS_nanosleep, &req, &rem);
	if(int e = sc_error(ret); e) {
		*secs = rem.tv_sec;
		*nanos = rem.tv_nsec;
		return e;
	}
	*secs = 0;
	*nanos = 0;
	return 0;
}

// ---------------------------------------------------------------------------
// Misc system info
// ---------------------------------------------------------------------------

int Sysdeps<Uname>::operator()(struct utsname *buf) {
	auto ret = do_syscall(SYS_uname, buf);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Pread>::operator()(int fd, void *buf, size_t n, off_t off, ssize_t *bytes_read) {
	auto ret = do_syscall(SYS_pread64, fd, buf, n, off);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_read = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<Pwrite>::operator()(int fd, const void *buf, size_t n, off_t off, ssize_t *bytes_written) {
	auto ret = do_syscall(SYS_pwrite64, fd, buf, n, off);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_written = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<OpenDir>::operator()(const char *path, int *fd) {
	return sysdep<Open>(path, O_DIRECTORY, 0, fd);
}

int Sysdeps<ReadEntries>::operator()(int handle, void *buffer, size_t max_size, size_t *bytes_read) {
	auto ret = do_syscall(SYS_getdents64, handle, buffer, max_size);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_read = sc_int_result<size_t>(ret);
	return 0;
}

int Sysdeps<GetRlimit>::operator()(int resource, struct rlimit *limit) {
	auto ret = do_syscall(SYS_getrlimit, resource, limit);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetRlimit>::operator()(int resource, const struct rlimit *limit) {
	auto ret = do_syscall(SYS_setrlimit, resource, limit);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<GetRusage>::operator()(int scope, struct rusage *usage) {
	auto ret = do_syscall(SYS_getrusage, scope, usage);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

// mount/umount/statfs wrappers are in mount.cpp (ChickenOS-native, not linux option)

void Sysdeps<Yield>::operator()() {
	do_syscall(SYS_sched_yield);
}

int Sysdeps<Getcpu>::operator()(int *cpu) {
#ifndef MLIBC_STATIC_BUILD
	if(vdso_getcpu) {
		unsigned cpu_out;
		long rc = vdso_getcpu(&cpu_out, nullptr, nullptr);
		if(rc == 0) {
			*cpu = static_cast<int>(cpu_out);
			return 0;
		}
		if(rc != -ENOSYS)
			return -rc;
	}
#endif
	auto ret = do_syscall(SYS_getcpu, cpu, nullptr, nullptr);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<GetEntropy>::operator()(void *buffer, size_t length) {
	auto ret = do_syscall(SYS_getrandom, buffer, length, 0);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<GetPriority>::operator()(int which, id_t who, int *value) {
	auto ret = do_syscall(SYS_getpriority, which, who);
	if(int e = sc_error(ret); e)
		return e;
	*value = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<SetPriority>::operator()(int which, id_t who, int prio) {
	auto ret = do_syscall(SYS_setpriority, which, who, prio);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetItimer>::operator()(int which, const struct itimerval *new_value, struct itimerval *old_value) {
	auto ret = do_syscall(SYS_setitimer, which, new_value, old_value);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Ptrace>::operator()(long req, pid_t pid, void *addr, void *data, long *out) {
	auto ret = do_syscall(SYS_ptrace, req, pid, addr, data);
	if(int e = sc_error(ret); e)
		return e;
	*out = sc_int_result<long>(ret);
	return 0;
}

[[noreturn]] void Sysdeps<ThreadExit>::operator()() {
	do_syscall(SYS_exit, 0);
	__builtin_trap();
}

int Sysdeps<Madvise>::operator()(void *addr, size_t length, int advice) {
	auto ret = do_syscall(SYS_madvise, addr, length, advice);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<Msync>::operator()(void *addr, size_t length, int flags) {
	auto ret = do_syscall(SYS_msync, addr, length, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

// ---------------------------------------------------------------------------
// Vectored I/O
// ---------------------------------------------------------------------------

int Sysdeps<Readv>::operator()(int fd, const struct iovec *iovs, int iovc, ssize_t *bytes_read) {
	auto ret = do_syscall(SYS_readv, fd, iovs, iovc);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_read = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<Writev>::operator()(int fd, const struct iovec *iovs, int iovc, ssize_t *bytes_written) {
	auto ret = do_syscall(SYS_writev, fd, iovs, iovc);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_written = sc_int_result<ssize_t>(ret);
	return 0;
}

// ---------------------------------------------------------------------------
// Filesystem info — statvfs wraps statfs
// ---------------------------------------------------------------------------

int Sysdeps<Statvfs>::operator()(const char *path, struct statvfs *out) {
	struct statfs buf;
	auto ret = do_syscall(SYS_statfs, path, &buf);
	if(int e = sc_error(ret); e)
		return e;
	memset(out, 0, sizeof(*out));
	out->f_bsize   = buf.f_bsize;
	out->f_frsize  = buf.f_bsize;
	out->f_blocks  = buf.f_blocks;
	out->f_bfree   = buf.f_bfree;
	out->f_bavail  = buf.f_bavail;
	out->f_files   = buf.f_files;
	out->f_ffree   = buf.f_ffree;
	out->f_favail  = buf.f_ffree;
	out->f_namemax = buf.f_namelen;
	return 0;
}

int Sysdeps<Fstatvfs>::operator()(int fd, struct statvfs *out) {
	struct statfs buf;
	auto ret = do_syscall(SYS_fstatfs, fd, &buf);
	if(int e = sc_error(ret); e)
		return e;
	memset(out, 0, sizeof(*out));
	out->f_bsize   = buf.f_bsize;
	out->f_frsize  = buf.f_bsize;
	out->f_blocks  = buf.f_blocks;
	out->f_bfree   = buf.f_bfree;
	out->f_bavail  = buf.f_bavail;
	out->f_files   = buf.f_files;
	out->f_ffree   = buf.f_ffree;
	out->f_favail  = buf.f_ffree;
	out->f_namemax = buf.f_namelen;
	return 0;
}

/* No kernel fallocate implementation */
int Sysdeps<Fallocate>::operator()(int fd, off_t offset, size_t size) {
	(void)fd; (void)offset; (void)size;
	mlibc::infoLogger() << "mlibc: sys_fallocate is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* fadvise is advisory-only; safe to ignore */
int Sysdeps<Fadvise>::operator()(int fd, off_t offset, off_t length, int advice) {
	(void)fd; (void)offset; (void)length; (void)advice;
	mlibc::infoLogger() << "mlibc: sys_fadvise is a stub (advisory, ignored)" << frg::endlog;
	return 0;
}

/* No kernel splice implementation */
int Sysdeps<Splice>::operator()(int in_fd, off_t *in_off, int out_fd, off_t *out_off, size_t size, unsigned int flags, ssize_t *out) {
	(void)in_fd; (void)in_off; (void)out_fd; (void)out_off; (void)size; (void)flags; (void)out;
	mlibc::infoLogger() << "mlibc: sys_splice is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No kernel name_to_handle_at implementation */
int Sysdeps<NameToHandleAt>::operator()(int dirfd, const char *pathname, struct file_handle *handle, int *mount_id, int flags) {
	(void)dirfd; (void)pathname; (void)handle; (void)mount_id; (void)flags;
	mlibc::infoLogger() << "mlibc: sys_name_to_handle_at is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Memory management additions
// ---------------------------------------------------------------------------

int Sysdeps<VmRemap>::operator()(void *pointer, size_t size, size_t new_size, void **window) {
	auto ret = do_syscall(SYS_mremap, pointer, size, new_size, 1 /* MREMAP_MAYMOVE */);
	auto addr = sc_int_result<long>(ret);
	if(addr >= -4096L && addr < 0)
		return -addr;
	*window = reinterpret_cast<void *>(addr);
	return 0;
}

/* mlock/munlock: kernel defines numbers but no implementation; safe no-ops */
int Sysdeps<Mlock>::operator()(const void *addr, size_t length) {
	(void)addr; (void)length;
	mlibc::infoLogger() << "mlibc: sys_mlock is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

int Sysdeps<Munlock>::operator()(const void *addr, size_t length) {
	(void)addr; (void)length;
	mlibc::infoLogger() << "mlibc: sys_munlock is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

int Sysdeps<Mlockall>::operator()(int flags) {
	(void)flags;
	mlibc::infoLogger() << "mlibc: sys_mlockall is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

int Sysdeps<Munlockall>::operator()(void) {
	mlibc::infoLogger() << "mlibc: sys_munlockall is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* No kernel mincore implementation */
int Sysdeps<Mincore>::operator()(void *addr, size_t length, unsigned char *vec) {
	(void)addr; (void)length; (void)vec;
	mlibc::infoLogger() << "mlibc: sys_mincore is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* posix_madvise maps to madvise */
int Sysdeps<PosixMadvise>::operator()(void *addr, size_t length, int advice) {
	return sysdep<Madvise>(addr, length, advice);
}

/* No kernel memfd_create implementation */
int Sysdeps<MemfdCreate>::operator()(const char *name, int flags, int *fd) {
	(void)name; (void)flags; (void)fd;
	mlibc::infoLogger() << "mlibc: sys_memfd_create is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Process/User identity additions
// ---------------------------------------------------------------------------

int Sysdeps<SetEuid>::operator()(uid_t euid) {
	auto ret = do_syscall(SYS_setresuid, -1, euid, -1);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetEgid>::operator()(gid_t egid) {
	auto ret = do_syscall(SYS_setresgid, -1, egid, -1);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetReuid>::operator()(uid_t ruid, uid_t euid) {
	auto ret = do_syscall(SYS_setreuid, ruid, euid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetRegid>::operator()(gid_t rgid, gid_t egid) {
	auto ret = do_syscall(SYS_setregid, rgid, egid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetResuid>::operator()(uid_t ruid, uid_t euid, uid_t suid) {
	auto ret = do_syscall(SYS_setresuid, ruid, euid, suid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetResgid>::operator()(gid_t rgid, gid_t egid, gid_t sgid) {
	auto ret = do_syscall(SYS_setresgid, rgid, egid, sgid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<GetResuid>::operator()(uid_t *ruid, uid_t *euid, uid_t *suid) {
	auto ret = do_syscall(SYS_getresuid, ruid, euid, suid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<GetResgid>::operator()(gid_t *rgid, gid_t *egid, gid_t *sgid) {
	auto ret = do_syscall(SYS_getresgid, rgid, egid, sgid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetGroups>::operator()(size_t size, const gid_t *list) {
	auto ret = do_syscall(SYS_setgroups, size, list);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

/* No login database support */
int Sysdeps<GetLoginR>::operator()(char *name, size_t name_len) {
	(void)name; (void)name_len;
	mlibc::infoLogger() << "mlibc: sys_getlogin_r is a stub (no login database)" << frg::endlog;
	return ENOENT;
}

/* No kernel waitid implementation */
int Sysdeps<Waitid>::operator()(idtype_t idtype, id_t id, siginfo_t *info, int options) {
	(void)idtype; (void)id; (void)info; (void)options;
	mlibc::infoLogger() << "mlibc: sys_waitid is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* nice: implemented via getpriority/setpriority */
int Sysdeps<Nice>::operator()(int nice, int *new_nice) {
	auto ret = do_syscall(SYS_getpriority, 0 /* PRIO_PROCESS */, 0);
	if(int e = sc_error(ret); e)
		return e;
	int prio = sc_int_result<int>(ret);
	prio += nice;
	ret = do_syscall(SYS_setpriority, 0 /* PRIO_PROCESS */, 0, prio);
	if(int e = sc_error(ret); e)
		return e;
	*new_nice = prio;
	return 0;
}

/* No kernel fexecve implementation */
int Sysdeps<Fexecve>::operator()(int fd, char *const argv[], char *const envp[]) {
	(void)fd; (void)argv; (void)envp;
	mlibc::infoLogger() << "mlibc: sys_fexecve is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Signals additions
// ---------------------------------------------------------------------------

/* thread_sigmask: same as sigprocmask on ChickenOS (no per-thread masks yet) */
int Sysdeps<ThreadSigmask>::operator()(int how, const sigset_t *__restrict set, sigset_t *__restrict retrieve) {
	auto ret = do_syscall(SYS_rt_sigprocmask, how, set, retrieve, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

/* No kernel rt_sigtimedwait implementation */
int Sysdeps<Sigtimedwait>::operator()(const sigset_t *__restrict set, siginfo_t *__restrict info,
		const struct timespec *__restrict timeout, int *out_signal) {
	(void)set; (void)info; (void)timeout; (void)out_signal;
	mlibc::infoLogger() << "mlibc: sys_sigtimedwait is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* Cancellation support: not yet implemented */
int Sysdeps<BeforeCancellableSyscall>::operator()(ucontext_t *uctx) {
	(void)uctx;
	return 0;
}

/* No kernel pause implementation */
int Sysdeps<Pause>::operator()() {
	mlibc::infoLogger() << "mlibc: sys_pause is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// I/O multiplexing additions
// ---------------------------------------------------------------------------

int Sysdeps<Ppoll>::operator()(struct pollfd *fds, nfds_t count, const struct timespec *ts,
		const sigset_t *mask, int *num_events) {
	auto ret = do_syscall(SYS_ppoll, fds, count, ts, mask);
	if(int e = sc_error(ret); e)
		return e;
	*num_events = sc_int_result<int>(ret);
	return 0;
}

int Sysdeps<Pselect>::operator()(int num_fds, fd_set *read_set, fd_set *write_set,
		fd_set *except_set, const struct timespec *timeout,
		const sigset_t *sigmask, int *num_events) {
	auto ret = do_syscall(SYS_pselect6, num_fds, read_set, write_set, except_set, timeout, sigmask);
	if(int e = sc_error(ret); e)
		return e;
	*num_events = sc_int_result<int>(ret);
	return 0;
}

// ---------------------------------------------------------------------------
// Socket additions
// ---------------------------------------------------------------------------

int Sysdeps<Recvfrom>::operator()(int fd, void *buffer, size_t size, int flags,
		struct sockaddr *sock_addr, socklen_t *addr_length, ssize_t *length) {
	auto ret = do_syscall(SYS_recvfrom, fd, buffer, size, flags, sock_addr, addr_length);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
	return 0;
}

int Sysdeps<Sendto>::operator()(int fd, const void *buffer, size_t size, int flags,
		const struct sockaddr *sock_addr, socklen_t addr_length, ssize_t *length) {
	auto ret = do_syscall(SYS_sendto, fd, buffer, size, flags, sock_addr, addr_length);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
	return 0;
}

/* No kernel sockatmark implementation */
int Sysdeps<Sockatmark>::operator()(int sockfd, int *out) {
	(void)sockfd; (void)out;
	mlibc::infoLogger() << "mlibc: sys_sockatmark is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No network interfaces configured yet */
int Sysdeps<InetConfigured>::operator()(bool *ipv4, bool *ipv6) {
	*ipv4 = false;
	*ipv6 = false;
	return 0;
}

// ---------------------------------------------------------------------------
// Terminal/PTY additions
// ---------------------------------------------------------------------------

/* tcsendbreak: no kernel implementation; safe no-op */
int Sysdeps<Tcsendbreak>::operator()(int fd, int dur) {
	(void)fd; (void)dur;
	mlibc::infoLogger() << "mlibc: sys_tcsendbreak is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* tcflow: no kernel implementation; safe no-op */
int Sysdeps<Tcflow>::operator()(int fd, int action) {
	(void)fd; (void)action;
	mlibc::infoLogger() << "mlibc: sys_tcflow is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* tcflush: no kernel implementation; safe no-op */
int Sysdeps<Tcflush>::operator()(int fd, int queue) {
	(void)fd; (void)queue;
	mlibc::infoLogger() << "mlibc: sys_tcflush is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* tcdrain: no kernel implementation; safe no-op */
int Sysdeps<Tcdrain>::operator()(int fd) {
	(void)fd;
	mlibc::infoLogger() << "mlibc: sys_tcdrain is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* No kernel ttyname support; would need /proc/self/fd/N */
int Sysdeps<Ttyname>::operator()(int fd, char *buf, size_t size) {
	(void)fd; (void)buf; (void)size;
	mlibc::infoLogger() << "mlibc: sys_ttyname is a stub (no procfs fd support)" << frg::endlog;
	return ENOSYS;
}

/* No kernel pseudo-terminal support */
int Sysdeps<Ptsname>::operator()(int fd, char *buffer, size_t length) {
	(void)fd; (void)buffer; (void)length;
	mlibc::infoLogger() << "mlibc: sys_ptsname is a stub (no pty support)" << frg::endlog;
	return ENOSYS;
}

/* unlockpt: no kernel pty support; safe no-op */
int Sysdeps<Unlockpt>::operator()(int fd) {
	(void)fd;
	mlibc::infoLogger() << "mlibc: sys_unlockpt is a stub (no pty support, no-op)" << frg::endlog;
	return 0;
}

/* No kernel pseudo-terminal support */
int Sysdeps<Openpt>::operator()(int oflags, int *fd) {
	(void)oflags; (void)fd;
	mlibc::infoLogger() << "mlibc: sys_openpt is a stub (no pty support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Timer additions
// ---------------------------------------------------------------------------

int Sysdeps<GetItimer>::operator()(int which, struct itimerval *curr_value) {
	auto ret = do_syscall(SYS_getitimer, which, curr_value);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<ClockGetres>::operator()(int clock, time_t *secs, long *nanos) {
	struct timespec ts;
	auto ret = do_syscall(SYS_clock_getres, clock, &ts);
	if(int e = sc_error(ret); e)
		return e;
	*secs = ts.tv_sec;
	*nanos = ts.tv_nsec;
	return 0;
}

/* No kernel clock_settime implementation */
int Sysdeps<ClockSet>::operator()(int clock, time_t secs, long nanos) {
	(void)clock; (void)secs; (void)nanos;
	mlibc::infoLogger() << "mlibc: sys_clock_set is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No kernel POSIX timer implementation */
int Sysdeps<TimerCreate>::operator()(clockid_t clk, struct sigevent *__restrict evp, timer_t *__restrict res) {
	(void)clk; (void)evp; (void)res;
	mlibc::infoLogger() << "mlibc: sys_timer_create is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<TimerSettime>::operator()(timer_t t, int flags, const struct itimerspec *__restrict val, struct itimerspec *__restrict old) {
	(void)t; (void)flags; (void)val; (void)old;
	mlibc::infoLogger() << "mlibc: sys_timer_settime is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<TimerGettime>::operator()(timer_t t, struct itimerspec *val) {
	(void)t; (void)val;
	mlibc::infoLogger() << "mlibc: sys_timer_gettime is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<TimerDelete>::operator()(timer_t t) {
	(void)t;
	mlibc::infoLogger() << "mlibc: sys_timer_delete is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<TimerGetoverrun>::operator()(timer_t t, int *out) {
	(void)t; (void)out;
	mlibc::infoLogger() << "mlibc: sys_timer_getoverrun is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No kernel times() implementation */
int Sysdeps<Times>::operator()(struct tms *tms, clock_t *out) {
	(void)tms; (void)out;
	mlibc::infoLogger() << "mlibc: sys_times is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* utimensat: no kernel implementation; safe no-op for now */
int Sysdeps<Utimensat>::operator()(int dirfd, const char *pathname, const struct timespec times[2], int flags) {
	(void)dirfd; (void)pathname; (void)times; (void)flags;
	mlibc::infoLogger() << "mlibc: sys_utimensat is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

// ---------------------------------------------------------------------------
// SysV IPC — no kernel support for any of these
// ---------------------------------------------------------------------------

int Sysdeps<Semget>::operator()(key_t key, int n, int fl, int *id) {
	(void)key; (void)n; (void)fl; (void)id;
	mlibc::infoLogger() << "mlibc: sys_semget is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<Semctl>::operator()(int semid, int semnum, int cmd, void *semun, int *ret) {
	(void)semid; (void)semnum; (void)cmd; (void)semun; (void)ret;
	mlibc::infoLogger() << "mlibc: sys_semctl is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<Shmat>::operator()(void **seg_start, int shmid, const void *shmaddr, int shmflg) {
	(void)seg_start; (void)shmid; (void)shmaddr; (void)shmflg;
	mlibc::infoLogger() << "mlibc: sys_shmat is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<Shmctl>::operator()(int *idx, int shmid, int cmd, struct shmid_ds *buf) {
	(void)idx; (void)shmid; (void)cmd; (void)buf;
	mlibc::infoLogger() << "mlibc: sys_shmctl is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<Shmdt>::operator()(const void *shmaddr) {
	(void)shmaddr;
	mlibc::infoLogger() << "mlibc: sys_shmdt is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<Shmget>::operator()(int *shm_id, key_t key, size_t size, int shmflg) {
	(void)shm_id; (void)key; (void)size; (void)shmflg;
	mlibc::infoLogger() << "mlibc: sys_shmget is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Scheduler/Affinity
// ---------------------------------------------------------------------------

int Sysdeps<GetAffinity>::operator()(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {
	auto ret = do_syscall(SYS_sched_getaffinity, pid, cpusetsize, mask);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<SetAffinity>::operator()(pid_t pid, size_t cpusetsize, const cpu_set_t *mask) {
	auto ret = do_syscall(SYS_sched_setaffinity, pid, cpusetsize, mask);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int Sysdeps<GetThreadaffinity>::operator()(pid_t tid, size_t cpusetsize, cpu_set_t *mask) {
	return sysdep<GetAffinity>(tid, cpusetsize, mask);
}

int Sysdeps<SetThreadaffinity>::operator()(pid_t tid, size_t cpusetsize, const cpu_set_t *mask) {
	return sysdep<SetAffinity>(tid, cpusetsize, mask);
}

// ---------------------------------------------------------------------------
// Miscellaneous additions
// ---------------------------------------------------------------------------

/* No kernel chroot implementation */
int Sysdeps<Chroot>::operator()(const char *path) {
	(void)path;
	mlibc::infoLogger() << "mlibc: sys_chroot is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* ⚠ The `|| defined(MLIBC_STATIC_BUILD)` in the gates below is load-bearing,
 * and the reason is archive-member selection.  meson's libc_static
 * link_wholes ld_static_lib, so libc.a holds TWO builds of this file: the libc
 * one and the rtld one.  A static program pulls the rtld's main.cpp in for
 * __dlapi_enter (crt1 -> __mlibc_entry -> __dlapi_enter), so both copies are
 * candidates for extraction.  While the two define the SAME symbols that is
 * harmless -- the armap names one member per symbol and lld extracts only
 * that one.  Gating functions out of the rtld build breaks the parity: a
 * program calling sysconf() forces the libc copy while __dlapi_enter forces
 * the rtld chain, lld extracts both, and every static link dies on
 *
 *   ld.lld: error: duplicate symbol: mlibc::sys_exit(int)
 *   >>> ... sysdeps_chickenos_generic_sysdeps.cpp.o in archive libc.a
 *   >>> ... sysdeps_chickenos_generic_sysdeps.cpp.o in archive libc.a
 *
 * 17 of them, on x86_64 and aarch64 alike -- toybox, busybox, dash and gawk
 * all stopped linking.  The gates exist for the SHARED ld.so, which is linked
 * standalone with -Wl,--no-undefined and really cannot carry these; the static
 * rtld has the whole libc in the same archive and can.  So gate on the shared
 * build only.
 */

/* sysconf: return key compile-time constants.  Gated out of the SHARED rtld
 * (ld.so, ld_static_lib): nothing in the rtld calls sys_sysconf, and the
 * _SC_PHYS_PAGES/_SC_AVPHYS_PAGES cases below pull in ::sysinfo(), which is
 * a libc_all_sources symbol (generic/sysinfo.cpp) never linked into ld.so --
 * left in, `-Wl,--no-undefined` fails ld.so's link over a function it never
 * calls. */
#if !defined(MLIBC_BUILDING_RTLD) || defined(MLIBC_STATIC_BUILD)
int Sysdeps<Sysconf>::operator()(int num, long *ret) {
	switch(num) {
		case _SC_PAGE_SIZE:
			*ret = 4096;
			return 0;
		case _SC_OPEN_MAX:
			*ret = 256;
			return 0;
		case _SC_NPROCESSORS_ONLN:
		case _SC_NPROCESSORS_CONF:
			*ret = 1;
			return 0;
		case _SC_PHYS_PAGES: {
			struct sysinfo info{};
			if(::sysinfo(&info) == 0) {
				*ret = info.totalram * info.mem_unit / 4096;
				return 0;
			}
			*ret = 1024 * 1024; /* fallback ~4 GB at 4K pages */
			return 0;
		}
		case _SC_AVPHYS_PAGES: {
			struct sysinfo info{};
			if(::sysinfo(&info) == 0) {
				*ret = info.freeram * info.mem_unit / 4096;
				return 0;
			}
			*ret = 1024 * 1024;
			return 0;
		}
		case _SC_CHILD_MAX:
			*ret = 256;
			return 0;
		case _SC_CLK_TCK:
			*ret = 100;
			return 0;
		case _SC_LINE_MAX:
			*ret = 2048;
			return 0;
		case _SC_ARG_MAX:
			*ret = 131072;
			return 0;
		case _SC_NGROUPS_MAX:
			*ret = 32;
			return 0;
		case _SC_TZNAME_MAX:
			*ret = 6; /* POSIX minimum */
			return 0;
		default:
			mlibc::infoLogger() << "mlibc: sys_sysconf unhandled num=" << num << frg::endlog;
			return EINVAL;
	}
}
#endif // !MLIBC_BUILDING_RTLD || MLIBC_STATIC_BUILD

/* No kernel devctl implementation */
int Sysdeps<PosixDevctl>::operator()(int fd, int dcmd, void *__restrict dev_data_ptr, size_t nbyte, int *__restrict dev_info_ptr) {
	(void)fd; (void)dcmd; (void)dev_data_ptr; (void)nbyte; (void)dev_info_ptr;
	mlibc::infoLogger() << "mlibc: sys_posix_devctl is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No network interface enumeration support */
int Sysdeps<IfIndextoname>::operator()(unsigned int index, char *name) {
	(void)index; (void)name;
	mlibc::infoLogger() << "mlibc: sys_if_indextoname is a stub (no network interface enumeration)" << frg::endlog;
	return ENXIO;
}

int Sysdeps<IfNametoindex>::operator()(const char *name, unsigned int *ret) {
	(void)name; (void)ret;
	mlibc::infoLogger() << "mlibc: sys_if_nametoindex is a stub (no network interface enumeration)" << frg::endlog;
	return ENOSYS;
}

/* Thread naming: no kernel prctl(PR_SET_NAME) support yet; safe no-op */
int Sysdeps<ThreadSetname>::operator()(void *tcb, const char *name) {
	(void)tcb; (void)name;
	mlibc::infoLogger() << "mlibc: sys_thread_setname is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* No kernel thread name retrieval */
int Sysdeps<ThreadGetname>::operator()(void *tcb, char *name, size_t size) {
	(void)tcb; (void)name; (void)size;
	mlibc::infoLogger() << "mlibc: sys_thread_getname is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No reliable way to query current stack info */
int Sysdeps<GetCurrentStackInfo>::operator()(void **stack_base, size_t *stack_size) {
	(void)stack_base; (void)stack_size;
	mlibc::infoLogger() << "mlibc: sys_get_current_stack_info is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// glibc-specific sysdeps
// ---------------------------------------------------------------------------

#if __MLIBC_GLIBC_OPTION

int Sysdeps<Personality>::operator()(unsigned long persona, int *out) {
	auto ret = do_syscall(SYS_personality, persona);
	if(int e = sc_error(ret); e)
		return e;
	*out = sc_int_result<int>(ret);
	return 0;
}

/* No kernel ioperm/iopl implementation — x86-specific privilege control */
int Sysdeps<Ioperm>::operator()(unsigned long int from, unsigned long int num, int turn_on) {
	(void)from; (void)num; (void)turn_on;
	mlibc::infoLogger() << "mlibc: sys_ioperm is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int Sysdeps<Iopl>::operator()(int level) {
	(void)level;
	mlibc::infoLogger() << "mlibc: sys_iopl is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

#endif // __MLIBC_GLIBC_OPTION

#endif // __MLIBC_POSIX_OPTION

} // namespace mlibc

// C-linkage syscall() wrapper — used by programs that call syscalls directly.
// Gated out of the rtld build: both functions below set `errno`, which is
// thread-local storage the rtld cannot resolve (it needs __tls_get_addr,
// which is a libc_all_sources symbol, undefined in ld.so's own link).
// Neither is ever called from inside the rtld itself.
#include <stdarg.h>

#if !defined(MLIBC_BUILDING_RTLD) || defined(MLIBC_STATIC_BUILD)
extern "C" long syscall(long number, ...) {
	va_list ap;
	va_start(ap, number);
	long a1 = va_arg(ap, long);
	long a2 = va_arg(ap, long);
	long a3 = va_arg(ap, long);
	long a4 = va_arg(ap, long);
	long a5 = va_arg(ap, long);
	long a6 = va_arg(ap, long);
	va_end(ap);

	auto ret = __do_syscall6(number, a1, a2, a3, a4, a5, a6);
	long v = static_cast<long>(ret);
	if (v < 0 && v > -4096L) {
		errno = -v;
		return -1;
	}
	return v;
}

// readahead() stub — not yet implemented in ChickenOS.
extern "C" __attribute__((weak))
ssize_t readahead(int, off64_t, size_t) {
	errno = ENOSYS;
	return -1;
}
#endif // !MLIBC_BUILDING_RTLD || MLIBC_STATIC_BUILD
