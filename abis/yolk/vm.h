/*
 * Yolk VM/mmap definitions for mlibc
 * SPDX-License-Identifier: MIT
 */

#ifndef _ABIS_YOLK_VM_H
#define _ABIS_YOLK_VM_H

/* Memory protection flags */
#define PROT_NONE       0x0
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4

/* Map type flags */
#define MAP_FILE        0x00
#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_TYPE        0x0f
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_GROWSDOWN   0x0100
#define MAP_DENYWRITE   0x0800
#define MAP_EXECUTABLE  0x1000
#define MAP_LOCKED      0x2000
#define MAP_NORESERVE   0x4000
#define MAP_POPULATE    0x8000
#define MAP_NONBLOCK    0x10000
#define MAP_STACK       0x20000
#define MAP_HUGETLB     0x40000

/* Map failure return */
#define MAP_FAILED      ((void *)-1)

/* msync flags */
#define MS_ASYNC        1
#define MS_SYNC         2
#define MS_INVALIDATE   4

/* madvise advice values */
#define MADV_NORMAL     0
#define MADV_RANDOM     1
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED   3
#define MADV_DONTNEED   4
#define MADV_FREE       8
#define MADV_REMOVE     9
#define MADV_DONTFORK   10
#define MADV_DOFORK     11
#define MADV_MERGEABLE  12
#define MADV_UNMERGEABLE 13

/* mlockall flags */
#define MCL_CURRENT     1
#define MCL_FUTURE      2

/* mremap flags */
#define MREMAP_MAYMOVE  1
#define MREMAP_FIXED    2

#endif /* _ABIS_YOLK_VM_H */
