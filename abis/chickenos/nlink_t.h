#ifndef _ABIBITS_NLINK_T_H
#define _ABIBITS_NLINK_T_H

/* The kernel's struct stat carries nlink_t/blksize_t as 64-bit on every port,
 * so a 32-bit port must not narrow them (they are `long` only where long is 64). */
#if defined(__LP64__)
typedef unsigned long nlink_t;
#else
typedef unsigned long long nlink_t;
#endif

#endif /* _ABIBITS_NLINK_T_H */
