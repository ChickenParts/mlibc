/* SPDX-License-Identifier: MIT */
#include <bits/syscall.h>
#if defined(__thumb__)
#define SEMIHOST "svc 0xab"
#else
#define SEMIHOST "svc 0x123456"
#endif
_Static_assert(SYS_get_thread_area == 0x302, "TLS helper syscall ABI");
extern int tls_probe(void);
static unsigned current;
static unsigned failed;
void handle_svc(unsigned *regs) {
 unsigned sc=regs[7];
 if (sc==SYS_get_thread_area) { regs[0]=0x12345678; return; }
 if (sc==0xff) {
  register unsigned op __asm__("r0")=4;
  register const char *msg __asm__("r1")=(regs[0] || failed) ? "ARM32_SYSCALL_FAIL\n" : "ARM32_SYSCALL_OK\n";
  __asm__ volatile(SEMIHOST : "+r"(op), "+r"(msg) : : "memory");
  op=0x18;
  register unsigned result __asm__("r1")=(regs[0] || failed) ? 0x20023 : 0x20026;
  __asm__ volatile(SEMIHOST : : "r"(op),"r"(result) : "memory");
  for(;;) {}
 }
 if(sc != 0x50+current) failed=1;
 for(unsigned i=0;i<current;i++) if(regs[i] != 0x11+i) failed=1;
 regs[0]=0x12340000+current;
 regs[1]=0x55667788;
}
int test_main(void) {
 if(sizeof(__sc_word_t)!=4 || !tls_probe()) return 1;
 current=0; if(__do_syscall0(0x50)!=0x12340000) return 2;
 current=1; if(__do_syscall1(0x51,0x11)!=0x12340001) return 3;
 current=2; if(__do_syscall2(0x52,0x11,0x12)!=0x12340002) return 4;
 current=3; if(__do_syscall3(0x53,0x11,0x12,0x13)!=0x12340003) return 5;
 current=4; if(__do_syscall4(0x54,0x11,0x12,0x13,0x14)!=0x12340004) return 6;
 current=5; if(__do_syscall5(0x55,0x11,0x12,0x13,0x14,0x15)!=0x12340005) return 7;
 current=6; if(__do_syscall6(0x56,0x11,0x12,0x13,0x14,0x15,0x16)!=0x12340006) return 8;
 current=4; if((unsigned long long)__do_syscall4_64(0x54,0x11,0x12,0x13,0x14)!=0x5566778812340004ULL) return 9;
 return failed;
}
