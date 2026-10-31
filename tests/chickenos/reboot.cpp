// SPDX-License-Identifier: MIT
#include <cassert>
#include <cerrno>
#include <chickenos/syscall.hpp>
#include <cstdint>
#include <mlibc/all-sysdeps.hpp>

static long result;
static int calls;
static long arguments[5];

extern "C" long __do_syscall4(long number, long a, long b, long c, long d) {
	++calls;
	arguments[0] = number;
	arguments[1] = a;
	arguments[2] = b;
	arguments[3] = c;
	arguments[4] = d;
	return result;
}

static void check(int command, long kernel_result, int expected) {
	result = kernel_result;
	calls = 0;
	assert(mlibc::Sysdeps<mlibc::Reboot>{}(command) == expected);
	assert(calls == 1);
	assert(arguments[0] == 0x24b);
	assert(static_cast<uint32_t>(arguments[1]) == 0xfee1deadU);
	assert(arguments[2] == 672274793);
	assert(static_cast<uint32_t>(arguments[3]) == static_cast<uint32_t>(command));
	assert(arguments[4] == 0);
}

int main() {
	check(0x01234567, 0, 0);
	check(static_cast<int>(0xcdef0123U), -EPERM, EPERM);
	check(0x4321fedc, -EINVAL, EINVAL);
	check(static_cast<int>(0xa1b2c3d4U), -ENOSYS, ENOSYS);
}
