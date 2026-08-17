/*
 * Yolk wait ABI definitions for mlibc
 * SPDX-License-Identifier: MIT
 *
 * This matches Linux wait.h - idtype_t is defined in mlibc's sys/wait.h
 */

#ifndef _ABIS_YOLK_WAIT_H
#define _ABIS_YOLK_WAIT_H

#include <mlibc-config.h>

#define WNOHANG 1
#define WUNTRACED 2
#define WSTOPPED 2
#define WEXITED 4
#define WCONTINUED 8
#define WNOWAIT 0x01000000

#define WCOREFLAG 0x80

#define WEXITSTATUS(x) (((x) & 0xff00) >> 8)
#define WTERMSIG(x) ((x) & 0x7f)
#define WSTOPSIG(x) WEXITSTATUS(x)
#define WIFEXITED(x) (WTERMSIG(x) == 0)
#define WIFSIGNALED(x) (((signed char) (((x) & 0x7f) + 1) >> 1) > 0)
#define WIFSTOPPED(x) (((x) & 0xff) == 0x7f)
#define WIFCONTINUED(x) ((x) == 0xffff)
#define WCOREDUMP(x) ((x) & WCOREFLAG)

/* glibc extension, but also useful for kernels */
#define W_EXITCODE(ret, sig) (((ret) << 8) | (sig))

#endif /* _ABIS_YOLK_WAIT_H */
