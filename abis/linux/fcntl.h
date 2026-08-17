/*
 * Yolk fcntl definitions for mlibc
 * SPDX-License-Identifier: MIT
 */

#ifndef _ABIS_YOLK_FCNTL_H
#define _ABIS_YOLK_FCNTL_H

/* File access modes */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003

/* File creation flags */
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_NOCTTY    0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800
#define O_DSYNC     0x1000
#define O_ASYNC     0x2000
#define O_DIRECT    0x4000
#define O_LARGEFILE 0x8000
#define O_DIRECTORY 0x10000
#define O_NOFOLLOW  0x20000
#define O_NOATIME   0x40000
#define O_CLOEXEC   0x80000
#define O_SYNC      (O_DSYNC | 0x100000)
#define O_PATH      0x200000
#define O_TMPFILE   0x400000

#define O_NDELAY    O_NONBLOCK

/* fcntl commands */
#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_GETLK         5
#define F_SETLK         6
#define F_SETLKW        7
#define F_SETOWN        8
#define F_GETOWN        9
#define F_DUPFD_CLOEXEC 1030

/* File descriptor flags */
#define FD_CLOEXEC  1

/* Advisory lock types */
#define F_RDLCK     0
#define F_WRLCK     1
#define F_UNLCK     2

/* Seek whence values */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* Access test modes */
#define F_OK        0
#define R_OK        4
#define W_OK        2
#define X_OK        1

/* AT_* constants */
#define AT_FDCWD            -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_EACCESS          0x200
#define AT_EMPTY_PATH       0x1000

#endif /* _ABIS_YOLK_FCNTL_H */
