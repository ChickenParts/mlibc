sysdep_supported_options = {
	'posix': true,
	'linux': true,
	'glibc': true,
	'bsd': false,
}

chickenos_include_dirs = [
	include_directories('include'),
]

rtld_include_dirs += chickenos_include_dirs
libc_include_dirs += chickenos_include_dirs

rtld_sources += files(
	host_machine.cpu_family() / 'arch-syscall.cpp',
	'generic/sysdeps.cpp',
)

libc_sources += files(
	host_machine.cpu_family() / 'arch-syscall.cpp',
	'generic/entry.cpp',
	'generic/sysdeps.cpp',
	'generic/mount.cpp',
	'generic/thread.cpp',
)

if not no_headers
	install_headers(
		'include/abi-bits/access.h',
		'include/abi-bits/auxv.h',
		'include/abi-bits/blkcnt_t.h',
		'include/abi-bits/blksize_t.h',
		'include/abi-bits/clockid_t.h',
		'include/abi-bits/dev_t.h',
		'include/abi-bits/epoll.h',
		'include/abi-bits/errno.h',
		'include/abi-bits/fcntl.h',
		'include/abi-bits/fd_set.h',
		'include/abi-bits/fsblkcnt_t.h',
		'include/abi-bits/fsfilcnt_t.h',
		'include/abi-bits/gid_t.h',
		'include/abi-bits/in.h',
		'include/abi-bits/ino_t.h',
		'include/abi-bits/inotify.h',
		'include/abi-bits/ioctls.h',
		'include/abi-bits/ipc.h',
		'include/abi-bits/limits.h',
		'include/abi-bits/mode_t.h',
		'include/abi-bits/mqueue.h',
		'include/abi-bits/msg.h',
		'include/abi-bits/nlink_t.h',
		'include/abi-bits/off_t.h',
		'include/abi-bits/packet.h',
		'include/abi-bits/pid_t.h',
		'include/abi-bits/poll.h',
		'include/abi-bits/ptrace.h',
		'include/abi-bits/random.h',
		'include/abi-bits/reboot.h',
		'include/abi-bits/resource.h',
		'include/abi-bits/rlim_t.h',
		'include/abi-bits/route.h',
		'include/abi-bits/sa_family_t.h',
		'include/abi-bits/seek-whence.h',
		'include/abi-bits/sem.h',
		'include/abi-bits/shm.h',
		'include/abi-bits/sig-limits.h',
		'include/abi-bits/sigevent.h',
		'include/abi-bits/signal.h',
		'include/abi-bits/sigset_t.h',
		'include/abi-bits/sigval.h',
		'include/abi-bits/sockaddr_storage.h',
		'include/abi-bits/socket.h',
		'include/abi-bits/socklen_t.h',
		'include/abi-bits/stat.h',
		'include/abi-bits/statfs.h',
		'include/abi-bits/statx.h',
		'include/abi-bits/statvfs.h',
		'include/abi-bits/suseconds_t.h',
		'include/abi-bits/termios.h',
		'include/abi-bits/time.h',
		'include/abi-bits/uid_t.h',
		'include/abi-bits/utsname.h',
		'include/abi-bits/utmp-defines.h',
		'include/abi-bits/utmpx.h',
		'include/abi-bits/vm-flags.h',
		'include/abi-bits/vt.h',
		'include/abi-bits/wait.h',
		'include/abi-bits/xattr.h',
		subdir: 'abi-bits',
		follow_symlinks: true
	)

	install_headers(
		'include/utmp.h',
	)

	install_headers(
		'include/bits/syscall.h',
		subdir: 'bits'
	)

	install_headers(
		'include/sys/mount.h',
		'include/sys/statfs.h',
		'include/sys/syscall.h',
		subdir: 'sys'
	)

	install_headers(
		'include/asm/ioctl.h',
		'include/asm/ioctls.h',
		subdir: 'asm'
	)
endif

if not headers_only
	crt_asm_args = c_compiler.cmd_array() + get_option('c_args') + ['-c', '-o', '@OUTPUT@', '@INPUT@']

	crt = custom_target('crt1',
		build_by_default: true,
		command: crt_asm_args,
		input: host_machine.cpu_family() / 'crt-src/crt1.S',
		output: 'crt1.o',
		install: true,
		install_dir: get_option('libdir')
	)

	# riscv64 gets crti/crtn from the compiler
	if host_machine.cpu_family() != 'riscv64'
		custom_target('crti',
			build_by_default: true,
			command: crt_asm_args,
			input: host_machine.cpu_family() / 'crt-src/crti.S',
			output: 'crti.o',
			install: true,
			install_dir: get_option('libdir')
		)

		custom_target('crtn',
			build_by_default: true,
			command: crt_asm_args,
			input: host_machine.cpu_family() / 'crt-src/crtn.S',
			output: 'crtn.o',
			install: true,
			install_dir: get_option('libdir')
		)
	endif
endif
