int sys_reboot(int cmd) {
	constexpr int magic1 = static_cast<int>(0xfee1deadU);
	constexpr int magic2 = 672274793;
	auto ret = do_syscall(SYS_reboot, magic1, magic2, cmd, nullptr);
	return sc_error(ret);
}
