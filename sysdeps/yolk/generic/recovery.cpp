/* SPDX-License-Identifier: MIT */
#include <mlibc/all-sysdeps.hpp>
#include <yolk/syscall.h>
#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

namespace mlibc {

static bool valid_dirent_batch(const void *buffer, size_t bytes)
{
	const unsigned char *cursor = static_cast<const unsigned char *>(buffer);
	const size_t minimum = offsetof(struct dirent, d_name) + 1u;
	size_t offset = 0;

	while (offset < bytes) {
		if (bytes - offset < minimum) return false;
		const struct dirent *entry =
			reinterpret_cast<const struct dirent *>(cursor + offset);
		size_t record = (size_t)entry->d_reclen;
		if (record < minimum || record > bytes - offset) return false;

		size_t name_capacity = record - offsetof(struct dirent, d_name);
		bool terminated = false;
		for (size_t i = 0; i < name_capacity; i++) {
			if (entry->d_name[i] == '\0') {
				terminated = true;
				break;
			}
		}
		if (!terminated) return false;
		offset += record;
	}
	return offset == bytes;
}

int sys_read_entries(int handle, void *buffer, size_t max_size,
                     size_t *bytes_read)
{
	if (!buffer || !bytes_read) return EINVAL;
	*bytes_read = 0;
	long result = __syscall3(SYS_getdents, handle, (long)buffer,
	                         (long)max_size);
	if (result < 0) return (int)-result;
	if ((unsigned long)result > max_size ||
	    !valid_dirent_batch(buffer, (size_t)result)) {
		return EIO;
	}
	*bytes_read = (size_t)result;
	return 0;
}

int sys_msync(void *address, size_t length, int flags)
{
	long result = __syscall3(SYS_msync, (long)address, (long)length, flags);
	return result < 0 ? (int)-result : 0;
}

} // namespace mlibc
