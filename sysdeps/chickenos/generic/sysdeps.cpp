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
#include <chickenos/syscall.hpp>
#include <sys/ioctl.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sched.h>

#define STUB_ONLY { \
	mlibc::infoLogger() << "mlibc: " << __func__ << " is a stub" << frg::endlog; \
	return ENOSYS; \
}

namespace mlibc {

void sys_libc_log(const char *message) {
	size_t n = 0;
	while(message[n])
		n++;
	do_syscall(SYS_write, 2, message, n);
	char lf = '\n';
	do_syscall(SYS_write, 2, &lf, 1);
}

void sys_libc_panic() {
	sys_libc_log("mlibc: PANIC!");
	__builtin_trap();
}

int sys_tcb_set(void *pointer) {
#if defined(__x86_64__)
	auto ret = do_syscall(SYS_arch_prctl, 0x1002 /* ARCH_SET_FS */, pointer);
	if(int e = sc_error(ret); e)
		return e;
#elif defined(__aarch64__)
	// aarch64: kernel sets TPIDR_EL0 via set_thread_area
	auto ret = do_syscall(SYS_set_thread_area, pointer);
	if(int e = sc_error(ret); e)
		return e;
#elif defined(__riscv)
	// riscv64: use the tp register directly
	uintptr_t v = reinterpret_cast<uintptr_t>(pointer);
	asm volatile("mv tp, %0" :: "r"(v));
#endif
	return 0;
}

int sys_anon_allocate(size_t size, void **pointer) {
	auto ret = do_syscall(SYS_mmap, 0, size,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(int e = sc_error(ret); e)
		return e;
	*pointer = sc_ptr_result<void>(ret);
	return 0;
}

int sys_anon_free(void *pointer, size_t size) {
	auto ret = do_syscall(SYS_munmap, pointer, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_open(const char *path, int flags, mode_t mode, int *fd) {
	auto ret = do_syscall(SYS_open, path, flags, mode);
	if(int e = sc_error(ret); e)
		return e;
	*fd = sc_int_result<int>(ret);
	return 0;
}

int sys_read(int fd, void *buffer, size_t size, ssize_t *bytes_read) {
	auto ret = do_syscall(SYS_read, fd, buffer, size);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_read = sc_int_result<ssize_t>(ret);
	return 0;
}

int sys_write(int fd, const void *buffer, size_t size, ssize_t *bytes_written) {
	auto ret = do_syscall(SYS_write, fd, buffer, size);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_written = sc_int_result<ssize_t>(ret);
	return 0;
}

int sys_seek(int fd, off_t offset, int whence, off_t *new_offset) {
	auto ret = do_syscall(SYS_lseek, fd, offset, whence);
	if(int e = sc_error(ret); e)
		return e;
	*new_offset = sc_int_result<off_t>(ret);
	return 0;
}

int sys_close(int fd) {
	auto ret = do_syscall(SYS_close, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_stat(fsfd_target fsfdt, int fd, const char *path, int flags,
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

int sys_vm_unmap(void *pointer, size_t size) {
	auto ret = do_syscall(SYS_munmap, pointer, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_vm_protect(void *pointer, size_t size, int prot) {
	auto ret = do_syscall(SYS_mprotect, pointer, size, prot);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_futex_wait(int *pointer, int expected, const struct timespec *time) {
	// ChickenOS futex: FUTEX_WAIT = 0
	auto ret = do_syscall(SYS_futex, pointer, 0 /* FUTEX_WAIT */, expected, time);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_futex_wake(int *pointer, bool all) {
	// ChickenOS futex: FUTEX_WAKE = 1
	auto ret = do_syscall(SYS_futex, pointer, 1 /* FUTEX_WAKE */, all ? INT_MAX : 1);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_clock_get(int clock, time_t *secs, long *nanos) {
	struct timespec ts;
	auto ret = do_syscall(SYS_clock_gettime, clock, &ts);
	if(int e = sc_error(ret); e)
		return e;
	*secs = ts.tv_sec;
	*nanos = ts.tv_nsec;
	return 0;
}

int sys_isatty(int fd) {
	auto ret = do_syscall(SYS_isatty, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

void sys_exit(int status) {
	do_syscall(SYS_exit_group, status);
	__builtin_trap();
}

// ---------------------------------------------------------------------------
// POSIX Level 2 — File operations
// ---------------------------------------------------------------------------

#if __MLIBC_POSIX_OPTION

int sys_openat(int dirfd, const char *path, int flags, mode_t mode, int *fd) {
	auto ret = do_syscall(SYS_openat, dirfd, path, flags, mode);
	if(int e = sc_error(ret); e)
		return e;
	*fd = sc_int_result<int>(ret);
	return 0;
}

int sys_dup(int fd, int flags, int *newfd) {
	(void)flags;
	auto ret = do_syscall(SYS_dup, fd);
	if(int e = sc_error(ret); e)
		return e;
	*newfd = sc_int_result<int>(ret);
	return 0;
}

int sys_dup2(int fd, int flags, int newfd) {
	(void)flags;
	auto ret = do_syscall(SYS_dup2, fd, newfd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_fcntl(int fd, int cmd, va_list args, int *result) {
	auto arg = va_arg(args, unsigned long);
	auto ret = do_syscall(SYS_fcntl64, fd, cmd, arg);
	if(int e = sc_error(ret); e)
		return e;
	*result = sc_int_result<int>(ret);
	return 0;
}

int sys_ioctl(int fd, unsigned long request, void *arg, int *result) {
	auto ret = do_syscall(SYS_ioctl, fd, request, arg);
	if(int e = sc_error(ret); e)
		return e;
	*result = sc_int_result<int>(ret);
	return 0;
}

int sys_getcwd(char *buf, size_t size) {
	auto ret = do_syscall(SYS_getcwd, buf, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_chmod(const char *pathname, mode_t mode) {
	auto ret = do_syscall(SYS_chmod, pathname, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_fchmod(int fd, mode_t mode) {
	auto ret = do_syscall(SYS_fchmod, fd, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_fchmodat(int fd, const char *pathname, mode_t mode, int flags) {
	auto ret = do_syscall(SYS_fchmodat, fd, pathname, mode, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) {
	auto ret = do_syscall(SYS_fchownat, dirfd, pathname, owner, group, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_access(const char *path, int mode) {
	auto ret = do_syscall(SYS_access, path, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_faccessat(int dirfd, const char *pathname, int mode, int flags) {
	auto ret = do_syscall(SYS_faccessat, dirfd, pathname, mode, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_pipe(int *fds, int flags) {
	if(flags) {
		auto ret = do_syscall(SYS_pipe2, fds, flags);
		if(int e = sc_error(ret); e)
			return e;
	} else {
		auto ret = do_syscall(SYS_pipe, fds);
		if(int e = sc_error(ret); e)
			return e;
	}
	return 0;
}

int sys_unlinkat(int dfd, const char *path, int flags) {
	auto ret = do_syscall(SYS_unlinkat, dfd, path, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_mkdir(const char *path, mode_t mode) {
	auto ret = do_syscall(SYS_mkdir, path, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_mkdirat(int dirfd, const char *path, mode_t mode) {
	auto ret = do_syscall(SYS_mkdirat, dirfd, path, mode);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_symlink(const char *target_path, const char *link_path) {
	auto ret = do_syscall(SYS_symlink, target_path, link_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_symlinkat(const char *target_path, int dirfd, const char *link_path) {
	auto ret = do_syscall(SYS_symlinkat, target_path, dirfd, link_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_link(const char *old_path, const char *new_path) {
	auto ret = do_syscall(SYS_link, old_path, new_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_linkat(int olddirfd, const char *old_path, int newdirfd, const char *new_path, int flags) {
	auto ret = do_syscall(SYS_linkat, olddirfd, old_path, newdirfd, new_path, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_rename(const char *old_path, const char *new_path) {
	auto ret = do_syscall(SYS_rename, old_path, new_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_renameat(int old_dirfd, const char *old_path, int new_dirfd, const char *new_path) {
	auto ret = do_syscall(SYS_renameat, old_dirfd, old_path, new_dirfd, new_path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_rmdir(const char *path) {
	auto ret = do_syscall(SYS_rmdir, path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_readlink(const char *path, void *buf, size_t bufsiz, ssize_t *len) {
	auto ret = do_syscall(SYS_readlink, path, buf, bufsiz);
	if(int e = sc_error(ret); e)
		return e;
	*len = sc_int_result<ssize_t>(ret);
	return 0;
}

int sys_readlinkat(int dirfd, const char *path, void *buffer, size_t max_size, ssize_t *length) {
	auto ret = do_syscall(SYS_readlinkat, dirfd, path, buffer, max_size);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
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

int sys_flock(int fd, int options) {
	auto ret = do_syscall(SYS_flock, fd, options);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_chdir(const char *path) {
	auto ret = do_syscall(SYS_chdir, path);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_fchdir(int fd) {
	auto ret = do_syscall(SYS_fchdir, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_umask(mode_t mode, mode_t *old) {
	auto ret = do_syscall(SYS_umask, mode);
	if(int e = sc_error(ret); e)
		return e;
	*old = sc_int_result<mode_t>(ret);
	return 0;
}

int sys_fsync(int fd) {
	auto ret = do_syscall(SYS_fsync, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_fdatasync(int fd) {
	auto ret = do_syscall(SYS_fdatasync, fd);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

void sys_sync() {
	do_syscall(SYS_sync);
}

// ---------------------------------------------------------------------------
// Process operations
// ---------------------------------------------------------------------------

int sys_fork(pid_t *child) {
	auto ret = do_syscall(SYS_fork);
	if(int e = sc_error(ret); e)
		return e;
	*child = sc_int_result<pid_t>(ret);
	return 0;
}

int sys_execve(const char *path, char *const argv[], char *const envp[]) {
	auto ret = do_syscall(SYS_execve, path, argv, envp);
	if(int e = sc_error(ret); e)
		return e;
	// execve should not return on success
	__builtin_unreachable();
}

int sys_waitpid(pid_t pid, int *status, int flags, struct rusage *ru, pid_t *ret_pid) {
	auto ret = do_syscall(SYS_wait4, pid, status, flags, ru);
	if(int e = sc_error(ret); e)
		return e;
	*ret_pid = sc_int_result<pid_t>(ret);
	return 0;
}

int sys_kill(int pid, int sig) {
	auto ret = do_syscall(SYS_kill, pid, sig);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_tgkill(int tgid, int tid, int sig) {
	auto ret = do_syscall(SYS_tgkill, tgid, tid, sig);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_sigaction(int signum, const struct sigaction *act,
		struct sigaction *oldact) {
	auto ret = do_syscall(SYS_rt_sigaction, signum, act, oldact, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_sigprocmask(int how, const sigset_t *set, sigset_t *old) {
	auto ret = do_syscall(SYS_rt_sigprocmask, how, set, old, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_sigpending(sigset_t *set) {
	auto ret = do_syscall(SYS_rt_sigpending, set, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_sigsuspend(const sigset_t *set) {
	auto ret = do_syscall(SYS_rt_sigsuspend, set, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_sigaltstack(const stack_t *ss, stack_t *oss) {
	auto ret = do_syscall(SYS_sigaltstack, ss, oss);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

// ---------------------------------------------------------------------------
// Process identity
// ---------------------------------------------------------------------------

pid_t sys_getpid() {
	auto ret = do_syscall(SYS_getpid);
	return sc_int_result<pid_t>(ret);
}

pid_t sys_getppid() {
	auto ret = do_syscall(SYS_getppid);
	return sc_int_result<pid_t>(ret);
}

pid_t sys_gettid() {
	auto ret = do_syscall(SYS_gettid);
	return sc_int_result<pid_t>(ret);
}

uid_t sys_getuid() {
	auto ret = do_syscall(SYS_getuid);
	return sc_int_result<uid_t>(ret);
}

uid_t sys_geteuid() {
	auto ret = do_syscall(SYS_geteuid);
	return sc_int_result<uid_t>(ret);
}

gid_t sys_getgid() {
	auto ret = do_syscall(SYS_getgid);
	return sc_int_result<gid_t>(ret);
}

gid_t sys_getegid() {
	auto ret = do_syscall(SYS_getegid);
	return sc_int_result<gid_t>(ret);
}

int sys_setpgid(pid_t pid, pid_t pgid) {
	auto ret = do_syscall(SYS_setpgid, pid, pgid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_getpgid(pid_t pid, pid_t *out) {
	auto ret = do_syscall(SYS_getpgid, pid);
	if(int e = sc_error(ret); e)
		return e;
	*out = sc_int_result<pid_t>(ret);
	return 0;
}

int sys_getsid(pid_t pid, pid_t *sid) {
	auto ret = do_syscall(SYS_getsid, pid);
	if(int e = sc_error(ret); e)
		return e;
	*sid = sc_int_result<pid_t>(ret);
	return 0;
}

int sys_setsid(pid_t *sid) {
	auto ret = do_syscall(SYS_setsid);
	if(int e = sc_error(ret); e)
		return e;
	*sid = sc_int_result<pid_t>(ret);
	return 0;
}

int sys_setuid(uid_t uid) {
	auto ret = do_syscall(SYS_setuid, uid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_setgid(gid_t gid) {
	auto ret = do_syscall(SYS_setgid, gid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_getgroups(size_t size, gid_t *list, int *retval) {
	auto ret = do_syscall(SYS_getgroups, size, list);
	if(int e = sc_error(ret); e)
		return e;
	*retval = sc_int_result<int>(ret);
	return 0;
}

// ---------------------------------------------------------------------------
// I/O multiplexing
// ---------------------------------------------------------------------------

int sys_poll(struct pollfd *fds, nfds_t count, int timeout, int *num_events) {
	auto ret = do_syscall(SYS_poll, fds, count, timeout);
	if(int e = sc_error(ret); e)
		return e;
	*num_events = sc_int_result<int>(ret);
	return 0;
}

int sys_epoll_create(int flags, int *fd) {
	auto ret = do_syscall(SYS_epoll_create1, flags);
	if(int e = sc_error(ret); e)
		return e;
	*fd = sc_int_result<int>(ret);
	return 0;
}

int sys_epoll_ctl(int epfd, int mode, int fd, struct epoll_event *ev) {
	auto ret = do_syscall(SYS_epoll_ctl, epfd, mode, fd, ev);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_epoll_pwait(int epfd, struct epoll_event *ev, int n, int timeout,
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

int sys_socket(int domain, int type, int protocol, int *fd) {
	auto ret = do_syscall(SYS_socket, domain, type, protocol);
	if(int e = sc_error(ret); e)
		return e;
	*fd = sc_int_result<int>(ret);
	return 0;
}

int sys_socketpair(int domain, int type_and_flags, int proto, int *fds) {
	auto ret = do_syscall(SYS_socketpair, domain, type_and_flags, proto, fds);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_bind(int fd, const struct sockaddr *addr_ptr, socklen_t addr_length) {
	auto ret = do_syscall(SYS_bind, fd, addr_ptr, addr_length);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_listen(int fd, int backlog) {
	auto ret = do_syscall(SYS_listen, fd, backlog);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_accept(int fd, int *newfd, struct sockaddr *addr_ptr,
		socklen_t *addr_length, int flags) {
	auto ret = do_syscall(SYS_accept4, fd, addr_ptr, addr_length, flags);
	if(int e = sc_error(ret); e)
		return e;
	*newfd = sc_int_result<int>(ret);
	return 0;
}

int sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	auto ret = do_syscall(SYS_connect, sockfd, addr, addrlen);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_shutdown(int sockfd, int how) {
	auto ret = do_syscall(SYS_shutdown, sockfd, how);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_msg_send(int sockfd, const struct msghdr *msg, int flags, ssize_t *length) {
	auto ret = do_syscall(SYS_sendmsg, sockfd, msg, flags);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
	return 0;
}

int sys_msg_recv(int sockfd, struct msghdr *msg, int flags, ssize_t *length) {
	auto ret = do_syscall(SYS_recvmsg, sockfd, msg, flags);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
	return 0;
}

int sys_setsockopt(int fd, int layer, int number, const void *buffer, socklen_t size) {
	auto ret = do_syscall(SYS_setsockopt, fd, layer, number, buffer, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_getsockopt(int fd, int layer, int number, void *__restrict buffer,
		socklen_t *__restrict size) {
	auto ret = do_syscall(SYS_getsockopt, fd, layer, number, buffer, size);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_sockname(int fd, struct sockaddr *addr_ptr, socklen_t max_addr_length,
		socklen_t *actual_length) {
	auto ret = do_syscall(SYS_getsockname, fd, addr_ptr, &max_addr_length);
	if(int e = sc_error(ret); e)
		return e;
	*actual_length = max_addr_length;
	return 0;
}

int sys_peername(int fd, struct sockaddr *addr_ptr, socklen_t max_addr_length,
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

int sys_tcgetattr(int fd, struct termios *attr) {
	auto ret = do_syscall(SYS_tcgetattr, fd, attr);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_tcsetattr(int fd, int optional_action, const struct termios *attr) {
	auto ret = do_syscall(SYS_tcsetattr, fd, optional_action, attr);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_tcgetwinsize(int fd, struct winsize *winsz) {
	auto ret = do_syscall(SYS_tcgetwinsize, fd, winsz);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_tcsetwinsize(int fd, const struct winsize *winsz) {
	auto ret = do_syscall(SYS_tcsetwinsize, fd, winsz);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

// ---------------------------------------------------------------------------
// Sleep
// ---------------------------------------------------------------------------

int sys_sleep(time_t *secs, long *nanos) {
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

int sys_uname(struct utsname *buf) {
	auto ret = do_syscall(SYS_uname, buf);
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

int sys_open_dir(const char *path, int *fd) {
	return sys_open(path, O_DIRECTORY, 0, fd);
}

int sys_read_entries(int handle, void *buffer, size_t max_size, size_t *bytes_read) {
	auto ret = do_syscall(SYS_getdents64, handle, buffer, max_size);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_read = sc_int_result<size_t>(ret);
	return 0;
}

int sys_getrlimit(int resource, struct rlimit *limit) {
	auto ret = do_syscall(SYS_getrlimit, resource, limit);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_setrlimit(int resource, const struct rlimit *limit) {
	auto ret = do_syscall(SYS_setrlimit, resource, limit);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_getrusage(int scope, struct rusage *usage) {
	auto ret = do_syscall(SYS_getrusage, scope, usage);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

// mount/umount/statfs wrappers are in mount.cpp (ChickenOS-native, not linux option)

void sys_yield() {
	do_syscall(SYS_sched_yield);
}

int sys_getrandom(void *buffer, size_t length, int flags, ssize_t *bytes_written) {
	auto ret = do_syscall(SYS_getrandom, buffer, length, flags);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_written = sc_int_result<ssize_t>(ret);
	return 0;
}

int sys_getpriority(int which, id_t who, int *value) {
	auto ret = do_syscall(SYS_getpriority, which, who);
	if(int e = sc_error(ret); e)
		return e;
	*value = sc_int_result<int>(ret);
	return 0;
}

int sys_setpriority(int which, id_t who, int prio) {
	auto ret = do_syscall(SYS_setpriority, which, who, prio);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value) {
	auto ret = do_syscall(SYS_setitimer, which, new_value, old_value);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_ptrace(long req, pid_t pid, void *addr, void *data, long *out) {
	auto ret = do_syscall(SYS_ptrace, req, pid, addr, data);
	if(int e = sc_error(ret); e)
		return e;
	*out = sc_int_result<long>(ret);
	return 0;
}

void sys_thread_exit() {
	do_syscall(SYS_exit, 0);
	__builtin_trap();
}

int sys_madvise(void *addr, size_t length, int advice) {
	auto ret = do_syscall(SYS_madvise, addr, length, advice);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_msync(void *addr, size_t length, int flags) {
	auto ret = do_syscall(SYS_msync, addr, length, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

// ---------------------------------------------------------------------------
// Vectored I/O
// ---------------------------------------------------------------------------

int sys_readv(int fd, const struct iovec *iovs, int iovc, ssize_t *bytes_read) {
	auto ret = do_syscall(SYS_readv, fd, iovs, iovc);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_read = sc_int_result<ssize_t>(ret);
	return 0;
}

int sys_writev(int fd, const struct iovec *iovs, int iovc, ssize_t *bytes_written) {
	auto ret = do_syscall(SYS_writev, fd, iovs, iovc);
	if(int e = sc_error(ret); e)
		return e;
	*bytes_written = sc_int_result<ssize_t>(ret);
	return 0;
}

// ---------------------------------------------------------------------------
// Filesystem info — statvfs wraps statfs
// ---------------------------------------------------------------------------

int sys_statvfs(const char *path, struct statvfs *out) {
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

int sys_fstatvfs(int fd, struct statvfs *out) {
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
int sys_fallocate(int fd, off_t offset, size_t size) {
	(void)fd; (void)offset; (void)size;
	mlibc::infoLogger() << "mlibc: sys_fallocate is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* fadvise is advisory-only; safe to ignore */
int sys_fadvise(int fd, off_t offset, off_t length, int advice) {
	(void)fd; (void)offset; (void)length; (void)advice;
	mlibc::infoLogger() << "mlibc: sys_fadvise is a stub (advisory, ignored)" << frg::endlog;
	return 0;
}

/* No kernel splice implementation */
int sys_splice(int in_fd, off_t *in_off, int out_fd, off_t *out_off, size_t size, unsigned int flags, ssize_t *out) {
	(void)in_fd; (void)in_off; (void)out_fd; (void)out_off; (void)size; (void)flags; (void)out;
	mlibc::infoLogger() << "mlibc: sys_splice is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No kernel name_to_handle_at implementation */
int sys_name_to_handle_at(int dirfd, const char *pathname, struct file_handle *handle, int *mount_id, int flags) {
	(void)dirfd; (void)pathname; (void)handle; (void)mount_id; (void)flags;
	mlibc::infoLogger() << "mlibc: sys_name_to_handle_at is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Memory management additions
// ---------------------------------------------------------------------------

int sys_vm_remap(void *pointer, size_t size, size_t new_size, void **window) {
	auto ret = do_syscall(SYS_mremap, pointer, size, new_size, 1 /* MREMAP_MAYMOVE */);
	auto addr = sc_int_result<long>(ret);
	if(addr >= -4096L && addr < 0)
		return -addr;
	*window = reinterpret_cast<void *>(addr);
	return 0;
}

/* mlock/munlock: kernel defines numbers but no implementation; safe no-ops */
int sys_mlock(const void *addr, size_t length) {
	(void)addr; (void)length;
	mlibc::infoLogger() << "mlibc: sys_mlock is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

int sys_munlock(const void *addr, size_t length) {
	(void)addr; (void)length;
	mlibc::infoLogger() << "mlibc: sys_munlock is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

int sys_mlockall(int flags) {
	(void)flags;
	mlibc::infoLogger() << "mlibc: sys_mlockall is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

int sys_munlockall(void) {
	mlibc::infoLogger() << "mlibc: sys_munlockall is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* No kernel mincore implementation */
int sys_mincore(void *addr, size_t length, unsigned char *vec) {
	(void)addr; (void)length; (void)vec;
	mlibc::infoLogger() << "mlibc: sys_mincore is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* posix_madvise maps to madvise */
int sys_posix_madvise(void *addr, size_t length, int advice) {
	return sys_madvise(addr, length, advice);
}

/* No kernel memfd_create implementation */
int sys_memfd_create(const char *name, int flags, int *fd) {
	(void)name; (void)flags; (void)fd;
	mlibc::infoLogger() << "mlibc: sys_memfd_create is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Process/User identity additions
// ---------------------------------------------------------------------------

/* seteuid: ChickenOS doesn't distinguish real/effective — forward to setuid */
int sys_seteuid(uid_t euid) {
	auto ret = do_syscall(SYS_setuid, euid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

/* setegid: ChickenOS doesn't distinguish real/effective — forward to setgid */
int sys_setegid(gid_t egid) {
	auto ret = do_syscall(SYS_setgid, egid);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

/* No kernel setreuid/setregid/setresuid/setresgid implementations */
int sys_setreuid(uid_t ruid, uid_t euid) {
	(void)ruid; (void)euid;
	mlibc::infoLogger() << "mlibc: sys_setreuid is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int sys_setregid(gid_t rgid, gid_t egid) {
	(void)rgid; (void)egid;
	mlibc::infoLogger() << "mlibc: sys_setregid is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid) {
	(void)ruid; (void)euid; (void)suid;
	mlibc::infoLogger() << "mlibc: sys_setresuid is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
	(void)rgid; (void)egid; (void)sgid;
	mlibc::infoLogger() << "mlibc: sys_setresgid is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* getresuid/getresgid: return the single uid/gid for all three */
int sys_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid) {
	uid_t u = sys_getuid();
	*ruid = u;
	*euid = u;
	*suid = u;
	return 0;
}

int sys_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid) {
	gid_t g = sys_getgid();
	*rgid = g;
	*egid = g;
	*sgid = g;
	return 0;
}

/* No kernel setgroups implementation; no-op */
int sys_setgroups(size_t size, const gid_t *list) {
	(void)size; (void)list;
	mlibc::infoLogger() << "mlibc: sys_setgroups is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* No login database support */
int sys_getlogin_r(char *name, size_t name_len) {
	(void)name; (void)name_len;
	mlibc::infoLogger() << "mlibc: sys_getlogin_r is a stub (no login database)" << frg::endlog;
	return ENOENT;
}

/* No kernel waitid implementation */
int sys_waitid(idtype_t idtype, id_t id, siginfo_t *info, int options) {
	(void)idtype; (void)id; (void)info; (void)options;
	mlibc::infoLogger() << "mlibc: sys_waitid is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* nice: implemented via getpriority/setpriority */
int sys_nice(int nice, int *new_nice) {
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
int sys_fexecve(int fd, char *const argv[], char *const envp[]) {
	(void)fd; (void)argv; (void)envp;
	mlibc::infoLogger() << "mlibc: sys_fexecve is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Signals additions
// ---------------------------------------------------------------------------

/* thread_sigmask: same as sigprocmask on ChickenOS (no per-thread masks yet) */
int sys_thread_sigmask(int how, const sigset_t *__restrict set, sigset_t *__restrict retrieve) {
	auto ret = do_syscall(SYS_rt_sigprocmask, how, set, retrieve, sizeof(sigset_t));
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

/* No kernel rt_sigtimedwait implementation */
int sys_sigtimedwait(const sigset_t *__restrict set, siginfo_t *__restrict info,
		const struct timespec *__restrict timeout, int *out_signal) {
	(void)set; (void)info; (void)timeout; (void)out_signal;
	mlibc::infoLogger() << "mlibc: sys_sigtimedwait is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* Cancellation support: not yet implemented */
int sys_before_cancellable_syscall(ucontext_t *uctx) {
	(void)uctx;
	return 0;
}

/* No kernel pause implementation */
int sys_pause() {
	mlibc::infoLogger() << "mlibc: sys_pause is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// I/O multiplexing additions
// ---------------------------------------------------------------------------

int sys_ppoll(struct pollfd *fds, nfds_t count, const struct timespec *ts,
		const sigset_t *mask, int *num_events) {
	auto ret = do_syscall(SYS_ppoll, fds, count, ts, mask);
	if(int e = sc_error(ret); e)
		return e;
	*num_events = sc_int_result<int>(ret);
	return 0;
}

int sys_pselect(int num_fds, fd_set *read_set, fd_set *write_set,
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

ssize_t sys_recvfrom(int fd, void *buffer, size_t size, int flags,
		struct sockaddr *sock_addr, socklen_t *addr_length, ssize_t *length) {
	auto ret = do_syscall(SYS_recvfrom, fd, buffer, size, flags, sock_addr, addr_length);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
	return 0;
}

ssize_t sys_sendto(int fd, const void *buffer, size_t size, int flags,
		const struct sockaddr *sock_addr, socklen_t addr_length, ssize_t *length) {
	auto ret = do_syscall(SYS_sendto, fd, buffer, size, flags, sock_addr, addr_length);
	if(int e = sc_error(ret); e)
		return e;
	*length = sc_int_result<ssize_t>(ret);
	return 0;
}

/* No kernel sockatmark implementation */
int sys_sockatmark(int sockfd, int *out) {
	(void)sockfd; (void)out;
	mlibc::infoLogger() << "mlibc: sys_sockatmark is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No network interfaces configured yet */
int sys_inet_configured(bool *ipv4, bool *ipv6) {
	*ipv4 = false;
	*ipv6 = false;
	return 0;
}

// ---------------------------------------------------------------------------
// Terminal/PTY additions
// ---------------------------------------------------------------------------

/* tcsendbreak: no kernel implementation; safe no-op */
int sys_tcsendbreak(int fd, int dur) {
	(void)fd; (void)dur;
	mlibc::infoLogger() << "mlibc: sys_tcsendbreak is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* tcflow: no kernel implementation; safe no-op */
int sys_tcflow(int fd, int action) {
	(void)fd; (void)action;
	mlibc::infoLogger() << "mlibc: sys_tcflow is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* tcflush: no kernel implementation; safe no-op */
int sys_tcflush(int fd, int queue) {
	(void)fd; (void)queue;
	mlibc::infoLogger() << "mlibc: sys_tcflush is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* tcdrain: no kernel implementation; safe no-op */
int sys_tcdrain(int fd) {
	(void)fd;
	mlibc::infoLogger() << "mlibc: sys_tcdrain is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* No kernel ttyname support; would need /proc/self/fd/N */
int sys_ttyname(int fd, char *buf, size_t size) {
	(void)fd; (void)buf; (void)size;
	mlibc::infoLogger() << "mlibc: sys_ttyname is a stub (no procfs fd support)" << frg::endlog;
	return ENOSYS;
}

/* No kernel pseudo-terminal support */
int sys_ptsname(int fd, char *buffer, size_t length) {
	(void)fd; (void)buffer; (void)length;
	mlibc::infoLogger() << "mlibc: sys_ptsname is a stub (no pty support)" << frg::endlog;
	return ENOSYS;
}

/* unlockpt: no kernel pty support; safe no-op */
int sys_unlockpt(int fd) {
	(void)fd;
	mlibc::infoLogger() << "mlibc: sys_unlockpt is a stub (no pty support, no-op)" << frg::endlog;
	return 0;
}

/* No kernel pseudo-terminal support */
int sys_openpt(int oflags, int *fd) {
	(void)oflags; (void)fd;
	mlibc::infoLogger() << "mlibc: sys_openpt is a stub (no pty support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Timer additions
// ---------------------------------------------------------------------------

int sys_getitimer(int which, struct itimerval *curr_value) {
	auto ret = do_syscall(SYS_getitimer, which, curr_value);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_clock_getres(int clock, time_t *secs, long *nanos) {
	struct timespec ts;
	auto ret = do_syscall(SYS_clock_getres, clock, &ts);
	if(int e = sc_error(ret); e)
		return e;
	*secs = ts.tv_sec;
	*nanos = ts.tv_nsec;
	return 0;
}

/* No kernel clock_settime implementation */
int sys_clock_set(int clock, time_t secs, long nanos) {
	(void)clock; (void)secs; (void)nanos;
	mlibc::infoLogger() << "mlibc: sys_clock_set is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No kernel POSIX timer implementation */
int sys_timer_create(clockid_t clk, struct sigevent *__restrict evp, timer_t *__restrict res) {
	(void)clk; (void)evp; (void)res;
	mlibc::infoLogger() << "mlibc: sys_timer_create is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int sys_timer_settime(timer_t t, int flags, const struct itimerspec *__restrict val, struct itimerspec *__restrict old) {
	(void)t; (void)flags; (void)val; (void)old;
	mlibc::infoLogger() << "mlibc: sys_timer_settime is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int sys_timer_gettime(timer_t t, struct itimerspec *val) {
	(void)t; (void)val;
	mlibc::infoLogger() << "mlibc: sys_timer_gettime is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int sys_timer_delete(timer_t t) {
	(void)t;
	mlibc::infoLogger() << "mlibc: sys_timer_delete is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int sys_timer_getoverrun(timer_t t, int *out) {
	(void)t; (void)out;
	mlibc::infoLogger() << "mlibc: sys_timer_getoverrun is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No kernel times() implementation */
int sys_times(struct tms *tms, clock_t *out) {
	(void)tms; (void)out;
	mlibc::infoLogger() << "mlibc: sys_times is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* utimensat: no kernel implementation; safe no-op for now */
int sys_utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags) {
	(void)dirfd; (void)pathname; (void)times; (void)flags;
	mlibc::infoLogger() << "mlibc: sys_utimensat is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

// ---------------------------------------------------------------------------
// SysV IPC — no kernel support for any of these
// ---------------------------------------------------------------------------

int sys_semget(key_t key, int n, int fl, int *id) {
	(void)key; (void)n; (void)fl; (void)id;
	mlibc::infoLogger() << "mlibc: sys_semget is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int sys_semctl(int semid, int semnum, int cmd, void *semun, int *ret) {
	(void)semid; (void)semnum; (void)cmd; (void)semun; (void)ret;
	mlibc::infoLogger() << "mlibc: sys_semctl is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int sys_shmat(void **seg_start, int shmid, const void *shmaddr, int shmflg) {
	(void)seg_start; (void)shmid; (void)shmaddr; (void)shmflg;
	mlibc::infoLogger() << "mlibc: sys_shmat is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int sys_shmctl(int *idx, int shmid, int cmd, struct shmid_ds *buf) {
	(void)idx; (void)shmid; (void)cmd; (void)buf;
	mlibc::infoLogger() << "mlibc: sys_shmctl is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int sys_shmdt(const void *shmaddr) {
	(void)shmaddr;
	mlibc::infoLogger() << "mlibc: sys_shmdt is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

int sys_shmget(int *shm_id, key_t key, size_t size, int shmflg) {
	(void)shm_id; (void)key; (void)size; (void)shmflg;
	mlibc::infoLogger() << "mlibc: sys_shmget is a stub (no kernel SysV IPC support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// Scheduler/Affinity
// ---------------------------------------------------------------------------

/* No kernel sched_getaffinity syscall — stub pretends single-CPU */
int sys_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {
	(void)pid;
	mlibc::infoLogger() << "mlibc: sys_getaffinity is a stub (no kernel sched_getaffinity)" << frg::endlog;
	/* Pretend CPU 0 is the only CPU */
	memset(mask, 0, cpusetsize);
	if(cpusetsize >= 1)
		reinterpret_cast<unsigned char *>(mask)[0] = 1;
	return 0;
}

/* No kernel sched_setaffinity syscall — stub ignores request */
int sys_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask) {
	(void)pid; (void)cpusetsize; (void)mask;
	mlibc::infoLogger() << "mlibc: sys_setaffinity is a stub (no kernel sched_setaffinity)" << frg::endlog;
	return 0;
}

int sys_getthreadaffinity(pid_t tid, size_t cpusetsize, cpu_set_t *mask) {
	return sys_getaffinity(tid, cpusetsize, mask);
}

int sys_setthreadaffinity(pid_t tid, size_t cpusetsize, const cpu_set_t *mask) {
	return sys_setaffinity(tid, cpusetsize, mask);
}

// ---------------------------------------------------------------------------
// Miscellaneous additions
// ---------------------------------------------------------------------------

/* No kernel chroot implementation */
int sys_chroot(const char *path) {
	(void)path;
	mlibc::infoLogger() << "mlibc: sys_chroot is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* sysconf: return key compile-time constants */
int sys_sysconf(int num, long *ret) {
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

/* No kernel devctl implementation */
int sys_posix_devctl(int fd, int dcmd, void *__restrict dev_data_ptr, size_t nbyte, int *__restrict dev_info_ptr) {
	(void)fd; (void)dcmd; (void)dev_data_ptr; (void)nbyte; (void)dev_info_ptr;
	mlibc::infoLogger() << "mlibc: sys_posix_devctl is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No network interface enumeration support */
int sys_if_indextoname(unsigned int index, char *name) {
	(void)index; (void)name;
	mlibc::infoLogger() << "mlibc: sys_if_indextoname is a stub (no network interface enumeration)" << frg::endlog;
	return ENXIO;
}

int sys_if_nametoindex(const char *name, unsigned int *ret) {
	(void)name; (void)ret;
	mlibc::infoLogger() << "mlibc: sys_if_nametoindex is a stub (no network interface enumeration)" << frg::endlog;
	return ENOSYS;
}

/* Thread naming: no kernel prctl(PR_SET_NAME) support yet; safe no-op */
int sys_thread_setname(void *tcb, const char *name) {
	(void)tcb; (void)name;
	mlibc::infoLogger() << "mlibc: sys_thread_setname is a stub (no kernel support, no-op)" << frg::endlog;
	return 0;
}

/* No kernel thread name retrieval */
int sys_thread_getname(void *tcb, char *name, size_t size) {
	(void)tcb; (void)name; (void)size;
	mlibc::infoLogger() << "mlibc: sys_thread_getname is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No reliable way to query current stack info */
int sys_get_current_stack_info(void **stack_base, size_t *stack_size) {
	(void)stack_base; (void)stack_size;
	mlibc::infoLogger() << "mlibc: sys_get_current_stack_info is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

// ---------------------------------------------------------------------------
// glibc-specific sysdeps
// ---------------------------------------------------------------------------

#if __MLIBC_GLIBC_OPTION

/* No kernel personality() implementation */
int sys_personality(unsigned long persona, int *out) {
	(void)persona; (void)out;
	mlibc::infoLogger() << "mlibc: sys_personality is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

/* No kernel ioperm/iopl implementation — x86-specific privilege control */
int sys_ioperm(unsigned long int from, unsigned long int num, int turn_on) {
	(void)from; (void)num; (void)turn_on;
	mlibc::infoLogger() << "mlibc: sys_ioperm is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

int sys_iopl(int level) {
	(void)level;
	mlibc::infoLogger() << "mlibc: sys_iopl is a stub (no kernel support)" << frg::endlog;
	return ENOSYS;
}

#endif // __MLIBC_GLIBC_OPTION

#endif // __MLIBC_POSIX_OPTION

} // namespace mlibc

// C-linkage syscall() wrapper — used by programs that call syscalls directly.
#include <stdarg.h>

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
