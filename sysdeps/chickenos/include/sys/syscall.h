#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#include <bits/syscall.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __MLIBC_ABI_ONLY
long syscall(long __number, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SYSCALL_H */
