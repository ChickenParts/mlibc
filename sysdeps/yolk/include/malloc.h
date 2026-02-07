#ifndef _MALLOC_H
#define _MALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/size_t.h>

#ifndef __MLIBC_ABI_ONLY

/* [7.22.3] Memory management functions */
void *calloc(size_t __count, size_t __size);
void free(void *__pointer);
void *malloc(size_t __size);
void *realloc(void *__pointer, size_t __size);
void *memalign(size_t __alignment, size_t __size);

/* mallopt options - stubs for compatibility */
#define M_TRIM_THRESHOLD -1
#define M_GRANULARITY -2
#define M_MMAP_THRESHOLD -3

int mallopt(int __param, int __value);

#endif /* !__MLIBC_ABI_ONLY */

#ifdef __cplusplus
}
#endif

#endif /* _MALLOC_H */
