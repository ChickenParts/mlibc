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
		ret = do_syscall(SYS_stat, path, statbuf);
	} else if(fsfdt == fsfd_target::fd) {
		ret = do_syscall(SYS_fstat, fd, statbuf);
	} else {
		__ensure(fsfdt == fsfd_target::fd_path);
		ret = do_syscall(SYS_fstat, fd, statbuf);
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
	auto ret = do_syscall(SYS_fcntl, fd, cmd, arg);
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

int sys_mount(const char *source, const char *target,
		const char *fstype, unsigned long flags, const void *data) {
	auto ret = do_syscall(SYS_mount, source, target, fstype, flags, data);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_umount2(const char *target, int flags) {
	auto ret = do_syscall(SYS_umount2, target, flags);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

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

int sys_statfs(const char *path, struct statfs *buf) {
	auto ret = do_syscall(SYS_statfs, path, buf);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

int sys_fstatfs(int fd, struct statfs *buf) {
	auto ret = do_syscall(SYS_fstatfs, fd, buf);
	if(int e = sc_error(ret); e)
		return e;
	return 0;
}

#endif // __MLIBC_POSIX_OPTION

} // namespace mlibc
