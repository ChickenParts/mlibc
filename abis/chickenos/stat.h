#ifndef _ABIBITS_STAT_H
#define _ABIBITS_STAT_H

#include <abi-bits/uid_t.h>
#include <abi-bits/gid_t.h>
#include <bits/off_t.h>
#include <abi-bits/mode_t.h>
#include <abi-bits/dev_t.h>
#include <abi-bits/ino_t.h>
#include <abi-bits/blksize_t.h>
#include <abi-bits/blkcnt_t.h>
#include <abi-bits/nlink_t.h>
#include <bits/ansi/time_t.h>
#include <bits/ansi/timespec.h>

#define S_IFMT 0x0F000
#define S_IFBLK 0x06000
#define S_IFCHR 0x02000
#define S_IFIFO 0x01000
#define S_IFREG 0x08000
#define S_IFDIR 0x04000
#define S_IFLNK 0x0A000
#define S_IFSOCK 0x0C000

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 070
#define S_IRGRP 040
#define S_IWGRP 020
#define S_IXGRP 010
#define S_IRWXO 07
#define S_IROTH 04
#define S_IWOTH 02
#define S_IXOTH 01
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

#define S_IREAD  S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC  S_IXUSR

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ChickenOS unified struct stat -- same layout on all LP64 architectures.
 * This matches the kernel's include/sys/stat.h exactly.
 * Total size: 144 bytes on LP64.
 */
struct stat {
	dev_t st_dev;            /* +0   (8 bytes) */
	ino_t st_ino;            /* +8   (8 bytes) */
	nlink_t st_nlink;        /* +16  (8 bytes) */
	mode_t st_mode;          /* +24  (4 bytes) */
	uid_t st_uid;            /* +28  (4 bytes) */
	gid_t st_gid;            /* +32  (4 bytes) */
	unsigned int __pad0;     /* +36  (4 bytes) */
	dev_t st_rdev;           /* +40  (8 bytes) */
	off_t st_size;           /* +48  (8 bytes) */
	blksize_t st_blksize;   /* +56  (8 bytes) */
	blkcnt_t st_blocks;      /* +64  (8 bytes) */
	struct timespec st_atim; /* +72  (16 bytes) */
	struct timespec st_mtim; /* +88  (16 bytes) */
	struct timespec st_ctim; /* +104 (16 bytes) */
	long __unused[3];        /* +120 (24 bytes) */
};                           /* Total: 144 bytes */

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

#if defined(_DEFAULT_SOURCE) || defined(_LARGEFILE64_SOURCE)
#define stat64 stat
#endif

#ifdef __cplusplus
}
#endif

#endif /* _ABIBITS_STAT_H */
