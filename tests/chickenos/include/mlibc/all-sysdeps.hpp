// SPDX-License-Identifier: MIT
#pragma once

namespace mlibc {
struct Reboot {};
template <typename>
struct Sysdeps;
template <>
struct Sysdeps<Reboot> {
	static int operator()(int cmd);
};
} // namespace mlibc
