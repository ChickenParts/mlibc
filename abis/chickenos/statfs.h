#ifndef _ABIBITS_STATFS_H
#define _ABIBITS_STATFS_H

#include <abi-bits/fsblkcnt_t.h>
#include <abi-bits/fsfilcnt_t.h>

typedef struct __mlibc_fsid {
	int __val[2];
} fsid_t;

/* WARNING: keep `statfs` and `statfs64` in sync or bad things will happen!
 * Field order matches the ChickenOS kernel ABI (include/sys/statfs.h):
 * f_frsize comes right after f_bsize (related fields together). */
struct statfs {
	unsigned long f_type;
	unsigned long f_bsize;
	unsigned long f_frsize;
	fsblkcnt_t f_blocks;
	fsblkcnt_t f_bfree;
	fsblkcnt_t f_bavail;
	fsfilcnt_t f_files;
	fsfilcnt_t f_ffree;
	fsid_t f_fsid;
	unsigned long f_namelen;
	unsigned long f_flags;
	unsigned long __f_spare[4];
};

/* WARNING: keep `statfs` and `statfs64` in sync or bad things will happen! */
struct statfs64 {
	unsigned long f_type;
	unsigned long f_bsize;
	unsigned long f_frsize;
	fsblkcnt_t f_blocks;
	fsblkcnt_t f_bfree;
	fsblkcnt_t f_bavail;
	fsfilcnt_t f_files;
	fsfilcnt_t f_ffree;
	fsid_t f_fsid;
	unsigned long f_namelen;
	unsigned long f_flags;
	unsigned long __f_spare[4];
};

#endif /* _ABIBITS_STATFS_H */
