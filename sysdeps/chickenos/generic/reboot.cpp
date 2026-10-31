// SPDX-License-Identifier: MIT
#include <chickenos/syscall.hpp>
#include <mlibc/all-sysdeps.hpp>

#if __MLIBC_LINUX_REBOOT_OPTION
namespace mlibc {

int Sysdeps<Reboot>::operator()(int cmd) {
	constexpr int magic1 = static_cast<int>(0xfee1deadU);
	constexpr int magic2 = 672274793;
	auto ret = do_syscall(SYS_reboot, magic1, magic2, cmd, nullptr);
	return sc_error(ret);
}

} // namespace mlibc
#endif
