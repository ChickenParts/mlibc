// SPDX-License-Identifier: MIT
#include <mlibc-config.h>
#if __MLIBC_LINUX_REBOOT_OPTION
#include <chickenos/syscall.hpp>
#include <mlibc/linux-reboot-sysdeps.hpp>

namespace mlibc {
int sys_reboot(int cmd) {
	constexpr int magic1 = static_cast<int>(0xfee1deadU);
	constexpr int magic2 = 672274793;
	auto ret = do_syscall(SYS_reboot, magic1, magic2, cmd, nullptr);
	return sc_error(ret);
}
} // namespace mlibc
#endif
