/*
 * SPDX-License-Identifier: NCSA
 * Copyright (c) 2025 Bryce Lanham, et al.
 */

#ifndef _ABIBITS_IOCTLS_H
#define _ABIBITS_IOCTLS_H

/* Core terminal ioctls */
#define TCGETS   0x5401
#define TCSETS   0x5402
#define TCSETSW  0x5403
#define TCSETSF  0x5404
#define TCSBRK   0x5409
#define TCXONC   0x540A
#define TCFLSH   0x540B
#define TIOCSCTTY 0x540E
#define TIOCGPGRP 0x540F
#define TIOCSPGRP 0x5410
#define TIOCOUTQ  0x5411
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCGSID  0x5429
#define TIOCGPTN  0x80045430

/* Terminal exclusive modes */
#define TIOCEXCL 0x540C
#define TIOCNXCL 0x540D

/* Socket/Network ioctls */
#define SIOCATMARK       0x8905  /* Is at OOB mark */
#define SIOCGSTAMP       0x8906  /* Get timestamp */
#define SIOCGIFNAME      0x8910  /* Get interface name by index */
#define SIOCGIFCONF      0x8912  /* Get interface list */
#define SIOCGIFFLAGS     0x8913  /* Get interface flags */
#define SIOCSIFFLAGS     0x8914  /* Set interface flags */
#define SIOCGIFADDR      0x8915  /* Get interface address */
#define SIOCSIFADDR      0x8916  /* Set interface address */
#define SIOCGIFBRDADDR   0x8919  /* Get broadcast address */
#define SIOCSIFBRDADDR   0x891A  /* Set broadcast address */
#define SIOCGIFNETMASK   0x891B  /* Get network mask */
#define SIOCSIFNETMASK   0x891C  /* Set network mask */
#define SIOCGIFMTU       0x8921  /* Get MTU */
#define SIOCSIFMTU       0x8922  /* Set MTU */
#define SIOCGIFHWADDR    0x8927  /* Get hardware address */
#define SIOCSIFHWADDR    0x8924  /* Set hardware address */
#define SIOCGIFINDEX     0x8933  /* Get interface index */

/* Protocol-private ioctls */
#define SIOCPROTOPRIVATE 0x89E0
#define SIOCDEVPRIVATE   0x89F0

#endif /* _ABIBITS_IOCTLS_H */
