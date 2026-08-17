
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include <bits/ensure.h>
#include <frg/allocation.hpp>
#include <mlibc-config.h>
#include <mlibc/allocator.hpp>
#include <mlibc/posix-sysdeps.hpp>
#include <mlibc/debug.hpp>

int alphasort(const struct dirent **a, const struct dirent **b) {
	return strcoll((*a)->d_name, (*b)->d_name);
}

int closedir(DIR *dir) {
	int result = close(dir->__handle);
	frg::destruct(getAllocator(), dir);
	return result;
}

int dirfd(DIR *dir) {
	return dir->__handle;
}

DIR *fdopendir(int fd) {
	struct stat st;
	if(fstat(fd, &st) < 0)
		return nullptr;
	if(!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		return nullptr;
	}
	auto dir = frg::construct<__mlibc_dir_struct>(getAllocator());
	__ensure(dir);
	dir->__ent_next = 0;
	dir->__ent_limit = 0;
	dir->__seek_offset = 0;
	int flags = fcntl(fd, F_GETFD);
	fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	dir->__handle = fd;
	return dir;
}

DIR *opendir(const char *path) {
	auto dir = frg::construct<__mlibc_dir_struct>(getAllocator());
	__ensure(dir);
	dir->__ent_next = 0;
	dir->__ent_limit = 0;
	dir->__seek_offset = 0;

	MLIBC_CHECK_OR_ENOSYS(mlibc::sys_open_dir, nullptr);
	if(int e = mlibc::sys_open_dir(path, &dir->__handle); e) {
		errno = e;
		frg::destruct(getAllocator(), dir);
		return nullptr;
	}
	return dir;
}

static bool current_entry(DIR *dir, struct dirent **entry, size_t *copy_size) {
	const size_t header = offsetof(struct dirent, d_name);
	if(dir->__ent_next > dir->__ent_limit ||
	    dir->__ent_limit > sizeof(dir->__ent_buffer) ||
	    dir->__ent_limit - dir->__ent_next < header + 1u) {
		return false;
	}

	auto candidate = reinterpret_cast<struct dirent *>(
		dir->__ent_buffer + dir->__ent_next);
	size_t record = (size_t)candidate->d_reclen;
	if(record < header + 1u || record > dir->__ent_limit - dir->__ent_next)
		return false;

	size_t name_capacity = record - header;
	size_t name_length = 0;
	while(name_length < name_capacity && candidate->d_name[name_length] != '\0')
		name_length++;
	if(name_length == name_capacity)
		return false;

	*entry = candidate;
	*copy_size = header + name_length + 1u;
	return true;
}

static int refill(DIR *dir) {
	MLIBC_CHECK_OR_ENOSYS(mlibc::sys_read_entries, ENOSYS);
	size_t bytes = 0;
	if(int e = mlibc::sys_read_entries(dir->__handle, dir->__ent_buffer,
	                                  sizeof(dir->__ent_buffer), &bytes); e)
		return e;
	if(bytes > sizeof(dir->__ent_buffer))
		return EIO;
	dir->__ent_next = 0;
	dir->__ent_limit = bytes;
	return 0;
}

struct dirent *readdir(DIR *dir) {
	if(dir->__ent_next == dir->__ent_limit) {
		if(int e = refill(dir); e) {
			errno = e;
			return nullptr;
		}
		if(!dir->__ent_limit)
			return nullptr;
	}

	struct dirent *entry = nullptr;
	size_t copy_size = 0;
	if(!current_entry(dir, &entry, &copy_size)) {
		dir->__ent_next = dir->__ent_limit = 0;
		errno = EIO;
		return nullptr;
	}
	memcpy(&dir->__current, entry, copy_size);
	dir->__seek_offset = entry->d_off;
	dir->__ent_next += entry->d_reclen;
	return &dir->__current;
}

ssize_t posix_getdents(int fildes, void *buf, size_t nbyte, int flags) {
	if(flags) {
		errno = EINVAL;
		return -1;
	}
	MLIBC_CHECK_OR_ENOSYS(mlibc::sys_read_entries, -1);
	size_t bytes_read = 0;
	if(int e = mlibc::sys_read_entries(fildes, buf, nbyte, &bytes_read); e) {
		errno = e;
		return -1;
	}
	return (ssize_t)bytes_read;
}

#if __MLIBC_LINUX_OPTION
[[gnu::alias("readdir")]] struct dirent64 *readdir64(DIR *dir);
#endif

int readdir_r(DIR *dir, struct dirent *output, struct dirent **result) {
	if(!mlibc::sys_read_entries) {
		MLIBC_MISSING_SYSDEP();
		return ENOSYS;
	}
	if(dir->__ent_next == dir->__ent_limit) {
		if(int e = refill(dir); e)
			return e;
		if(!dir->__ent_limit) {
			*result = nullptr;
			return 0;
		}
	}

	struct dirent *entry = nullptr;
	size_t copy_size = 0;
	if(!current_entry(dir, &entry, &copy_size)) {
		dir->__ent_next = dir->__ent_limit = 0;
		*result = nullptr;
		return EIO;
	}
	memcpy(output, entry, copy_size);
	dir->__seek_offset = entry->d_off;
	dir->__ent_next += entry->d_reclen;
	*result = output;
	return 0;
}

void rewinddir(DIR *dir) {
	off_t result = lseek(dir->__handle, 0, SEEK_SET);
	if(result >= 0)
		dir->__seek_offset = result;
	dir->__ent_next = 0;
	dir->__ent_limit = 0;
}

int scandir(const char *path, struct dirent ***res,
		int (*select)(const struct dirent *),
		int (*compare)(const struct dirent **, const struct dirent **)) {
	DIR *dir = opendir(path);
	if(!dir)
		return -1;

	int old_errno = errno;
	errno = 0;
	struct dirent *dir_ent;
	struct dirent **array = nullptr, **tmp = nullptr;
	int length = 0;
	int count = 0;
	while((dir_ent = readdir(dir)) && !errno) {
		if(select && !select(dir_ent))
			continue;
		if(count >= length) {
			length = 2 * length + 1;
			tmp = static_cast<struct dirent **>(
				realloc(array, (size_t)length * sizeof(struct dirent *)));
			if(!tmp)
				break;
			array = tmp;
		}
		array[count] = static_cast<struct dirent *>(malloc(dir_ent->d_reclen));
		if(!array[count])
			break;
		memcpy(array[count], dir_ent, dir_ent->d_reclen);
		count++;
	}
	int scan_errno = errno;
	closedir(dir);
	if(scan_errno) {
		while(count-- > 0)
			free(array[count]);
		free(array);
		errno = scan_errno;
		return -1;
	}
	errno = old_errno;
	if(compare)
		qsort(array, count, sizeof(struct dirent *),
		      reinterpret_cast<int (*)(const void *, const void *)>(compare));
	*res = array;
	return count;
}

void seekdir(DIR *dir, long offset) {
	off_t result = lseek(dir->__handle, (off_t)offset, SEEK_SET);
	if(result < 0)
		return;
	dir->__seek_offset = result;
	dir->__ent_next = 0;
	dir->__ent_limit = 0;
}

long telldir(DIR *dir) {
	return (long)dir->__seek_offset;
}

#if __MLIBC_GLIBC_OPTION
int versionsort(const struct dirent **a, const struct dirent **b) {
	return strverscmp((*a)->d_name, (*b)->d_name);
}
#endif
