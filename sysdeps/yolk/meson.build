# Yolk sysdeps for mlibc
# SPDX-License-Identifier: MIT

sysdep_supported_options = {
	'posix': true,
	'linux': false,
	'glibc': true,  # For Dl_info, dladdr, etc.
	'bsd': true,    # For bsd-sysdeps.hpp header (weak symbols only)
}

# Provide _DYNAMIC symbol for ld.so (shared dynamic linker)
if host_machine.cpu_family() == 'x86_64'
	rtld_dso_sources += files('x86_64/rtld-syms.S')
endif

libc_sources += files(
	'generic/entry.cpp',
	'generic/generic.cpp',
	'generic/thread.cpp',
)

# Architecture-specific thread entry and signal restorer
if host_machine.cpu_family() == 'x86_64'
	libc_sources += files(
		'x86_64/thread_entry.S',
		'x86_64/signals.S',
	)
endif

yolk_include_dirs = include_directories('include')
rtld_include_dirs += yolk_include_dirs
libc_include_dirs += yolk_include_dirs

if not no_headers
	install_headers(
		'include/yolk/syscall.h',
		subdir: 'yolk',
		follow_symlinks: true
	)

	install_headers(
		'include/malloc.h',
		follow_symlinks: true
	)

	# Install abi-bits headers (symlinks to abis/linux with yolk overrides)
	install_headers(
		'include/abi-bits/auxv.h',
		'include/abi-bits/seek-whence.h',
		'include/abi-bits/vm-flags.h',
		'include/abi-bits/errno.h',
		'include/abi-bits/fcntl.h',
		'include/abi-bits/in.h',
		'include/abi-bits/stat.h',
		'include/abi-bits/statx.h',
		'include/abi-bits/signal.h',
		'include/abi-bits/reboot.h',
		'include/abi-bits/resource.h',
		'include/abi-bits/socket.h',
		'include/abi-bits/termios.h',
		'include/abi-bits/time.h',
		'include/abi-bits/blkcnt_t.h',
		'include/abi-bits/blksize_t.h',
		'include/abi-bits/dev_t.h',
		'include/abi-bits/gid_t.h',
		'include/abi-bits/ino_t.h',
		'include/abi-bits/mode_t.h',
		'include/abi-bits/nlink_t.h',
		'include/abi-bits/pid_t.h',
		'include/abi-bits/uid_t.h',
		'include/abi-bits/access.h',
		'include/abi-bits/wait.h',
		'include/abi-bits/limits.h',
		'include/abi-bits/utsname.h',
		'include/abi-bits/ptrace.h',
		'include/abi-bits/vt.h',
		'include/abi-bits/poll.h',
		'include/abi-bits/epoll.h',
		'include/abi-bits/packet.h',
		'include/abi-bits/inotify.h',
		'include/abi-bits/clockid_t.h',
		'include/abi-bits/ipc.h',
		'include/abi-bits/shm.h',
		'include/abi-bits/mqueue.h',
		'include/abi-bits/suseconds_t.h',
		'include/abi-bits/fsfilcnt_t.h',
		'include/abi-bits/fsblkcnt_t.h',
		'include/abi-bits/socklen_t.h',
		'include/abi-bits/statfs.h',
		'include/abi-bits/statvfs.h',
		'include/abi-bits/ioctls.h',
		'include/abi-bits/xattr.h',
		'include/abi-bits/msg.h',
		'include/abi-bits/random.h',
		'include/abi-bits/rlim_t.h',
		'include/abi-bits/sigval.h',
		'include/abi-bits/sigevent.h',
		'include/abi-bits/utmpx.h',
		'include/abi-bits/utmp-defines.h',
		'include/abi-bits/fd_set.h',
		'include/abi-bits/sem.h',
		'include/abi-bits/route.h',
		'include/abi-bits/mman.h',
		'include/abi-bits/vm.h',
		subdir: 'abi-bits',
		follow_symlinks: true
	)
endif

if not headers_only
	crt = custom_target('crt0',
		build_by_default: true,
		command: c_compiler.cmd_array() + ['-c', '-o', '@OUTPUT@', '@INPUT@'],
		input: host_machine.cpu_family() / 'crt0.S',
		output: 'crt0.o',
		install: true,
		install_dir: get_option('libdir')
	)

	custom_target('crti',
		build_by_default: true,
		command: c_compiler.cmd_array() + ['-c', '-o', '@OUTPUT@', '@INPUT@'],
		input: host_machine.cpu_family() / 'crti.S',
		output: 'crti.o',
		install: true,
		install_dir: get_option('libdir')
	)

	custom_target('crtn',
		build_by_default: true,
		command: c_compiler.cmd_array() + ['-c', '-o', '@OUTPUT@', '@INPUT@'],
		input: host_machine.cpu_family() / 'crtn.S',
		output: 'crtn.o',
		install: true,
		install_dir: get_option('libdir')
	)
endif
