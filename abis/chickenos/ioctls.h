#ifndef _ABIBITS_IOCTLS_H
#define _ABIBITS_IOCTLS_H

#include <stddef.h>

/* ioctl encoding macros (Linux-compatible) */
#define _IOC_NRBITS    8
#define _IOC_TYPEBITS  8
#define _IOC_SIZEBITS  14
#define _IOC_DIRBITS   2

#define _IOC_NRSHIFT   0
#define _IOC_TYPESHIFT (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT  (_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define _IOC_NONE  0U
#define _IOC_WRITE 1U
#define _IOC_READ  2U

#define _IOC(dir,type,nr,size) \
	(((dir)  << _IOC_DIRSHIFT) | \
	 ((type) << _IOC_TYPESHIFT) | \
	 ((nr)   << _IOC_NRSHIFT) | \
	 ((size) << _IOC_SIZESHIFT))

#define _IO(type,nr)        _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,sz)    _IOC(_IOC_READ,(type),(nr),sizeof(sz))
#define _IOW(type,nr,sz)    _IOC(_IOC_WRITE,(type),(nr),sizeof(sz))
#define _IOWR(type,nr,sz)   _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(sz))

/* Terminal ioctls */
#define TCGETS      0x5401
#define TCSETS      0x5402
#define TCSETSW     0x5403
#define TCSETSF     0x5404
#define TIOCGPGRP   0x540F
#define TIOCSPGRP   0x5410
#define TIOCOUTQ    0x5411
#define TIOCSTI     0x5412
#define TIOCGWINSZ  0x5413
#define TIOCSWINSZ  0x5414
#define TIOCMGET    0x5415
#define TIOCMBIS    0x5416
#define TIOCMBIC    0x5417
#define TIOCMSET    0x5418
#define TIOCGSOFTCAR 0x5419
#define TIOCSSOFTCAR 0x541A
#define FIONREAD    0x541B
#define TIOCLINUX   0x541C
#define TIOCCONS    0x541D
#define TIOCGSERIAL 0x541E
#define TIOCSSERIAL 0x541F
#define TIOCPKT     0x5420
#define FIONBIO     0x5421
#define TIOCNOTTY   0x5422
#define TIOCSETD    0x5423
#define TIOCGETD    0x5424
#define TCSBRKP     0x5425
#define TIOCEXCL    0x540C
#define TIOCNXCL    0x540D
#define TIOCSCTTY   0x540E
#define TIOCGSID    0x5429
#define FIONCLEX    0x5450
#define FIOCLEX     0x5451
#define FIOASYNC    0x5452
#define FIOQSIZE    0x5460

/* Socket/network ioctls */
#define SIOCATMARK      0x8905
#define SIOCGSTAMP      0x8906
#define SIOCGIFNAME     0x8910
#define SIOCSIFLINK     0x8911
#define SIOCGIFCONF     0x8912
#define SIOCGIFFLAGS    0x8913
#define SIOCSIFFLAGS    0x8914
#define SIOCGIFADDR     0x8915
#define SIOCSIFADDR     0x8916
#define SIOCGIFDSTADDR  0x8917
#define SIOCSIFDSTADDR  0x8918
#define SIOCGIFBRDADDR  0x8919
#define SIOCSIFBRDADDR  0x891A
#define SIOCGIFNETMASK  0x891B
#define SIOCSIFNETMASK  0x891C
#define SIOCGIFMETRIC   0x891D
#define SIOCSIFMETRIC   0x891E
#define SIOCGIFMEM      0x891F
#define SIOCSIFMEM      0x8920
#define SIOCGIFMTU      0x8921
#define SIOCSIFMTU      0x8922
#define SIOCSIFHWADDR   0x8924
#define SIOCGIFHWADDR   0x8927
#define SIOCGIFINDEX    0x8933
#define SIOCGIFTXQLEN   0x8942
#define SIOCSIFTXQLEN   0x8943
#define SIOCDIFADDR     0x8936
#define SIOCSIFNAME     0x8923
#define SIOCETHTOOL     0x8946
#define SIOCGIFMAP      0x8970
#define SIOCSIFMAP      0x8971
#define SIOCPROTOPRIVATE 0x89E0

#endif /* _ABIBITS_IOCTLS_H */
