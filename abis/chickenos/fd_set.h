#ifndef _ABIBITS_FD_SET_H
#define _ABIBITS_FD_SET_H

#include <bits/size_t.h>

#define FD_SETSIZE 1024

typedef struct {
	unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

#define FD_ZERO(set) do { \
	for (unsigned long __i = 0; __i < sizeof(fd_set) / sizeof(unsigned long); __i++) \
		((unsigned long *)(set))->fds_bits[__i] = 0; \
} while (0)
#define FD_SET(d, set) ((set)->fds_bits[(d) / (8 * sizeof(unsigned long))] |= (1UL << ((d) % (8 * sizeof(unsigned long)))))
#define FD_CLR(d, set) ((set)->fds_bits[(d) / (8 * sizeof(unsigned long))] &= ~(1UL << ((d) % (8 * sizeof(unsigned long)))))
#define FD_ISSET(d, set) (!!((set)->fds_bits[(d) / (8 * sizeof(unsigned long))] & (1UL << ((d) % (8 * sizeof(unsigned long))))))

#endif /* _ABIBITS_FD_SET_H */
