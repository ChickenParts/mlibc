#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Mount flags (Linux-compatible values for interoperability) */
#define MS_RDONLY       1
#define MS_NOSUID       2
#define MS_NODEV        4
#define MS_NOEXEC       8
#define MS_SYNCHRONOUS  16
#define MS_REMOUNT      32
#define MS_MANDLOCK     64
#define MS_DIRSYNC      128
#define MS_NOSYMFOLLOW  256
#define MS_NOATIME      1024
#define MS_NODIRATIME   2048
#define MS_BIND         4096
#define MS_MOVE         8192
#define MS_REC          16384
#define MS_SILENT       32768
#define MS_RELATIME     (1 << 21)
#define MS_STRICTATIME  (1 << 24)

/* Umount flags */
#define MNT_FORCE       1
#define MNT_DETACH      2
#define MNT_EXPIRE      4
#define UMOUNT_NOFOLLOW 8

#ifndef __MLIBC_ABI_ONLY

int mount(const char *__source, const char *__target,
		const char *__fstype, unsigned long __flags, const void *__data);
int umount(const char *__target);
int umount2(const char *__target, int __flags);

#endif /* !__MLIBC_ABI_ONLY */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MOUNT_H */
