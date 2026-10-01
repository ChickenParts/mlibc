#ifndef _ABIBITS_BLKSIZE_T_H
#define _ABIBITS_BLKSIZE_T_H

#if defined(__LP64__)
typedef long blksize_t;
#else
typedef long long blksize_t; /* kernel struct stat: 64-bit on all ports */
#endif

#endif /* _ABIBITS_BLKSIZE_T_H */
