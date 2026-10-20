/* SPDX-License-Identifier: MIT */
#include <bits/syscall.h>
#if defined(__thumb__)
#define SEMIHOST "svc 0xab"
#else
#define SEMIHOST "svc 0x123456"
#endif
_Static_assert(SYS_get_thread_area == 0x302 && SYS_clone == 0x107, "assembly syscall ABI");
extern int tls_probe(void);
static unsigned current;
static unsigned failed;
unsigned requested_child_sp;
static unsigned clone_child;
static unsigned child_stack[512] __attribute__((aligned(8)));
#define CHILD_SP (child_stack+510)
extern long __mlibc_clone_arm(unsigned long,void *,void *,void *,void *);
__attribute__((noreturn)) void __mlibc_enter_thread(void *entry, void *arg) {
 unsigned sp;
 __asm__ volatile("mov %0,sp" : "=r"(sp));
 unsigned bad=(unsigned)entry!=0x1212 || (unsigned)arg!=0x3434 || (sp&7);
 __do_syscall1(0xff,bad);
 for(;;){}
}

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
 if(sc==SYS_clone) {
  if(regs[0]!=0x111 || regs[1]!=(unsigned)CHILD_SP || regs[2]!=0x1234 || regs[3]!=0x5566 || regs[4]!=0x7788) failed=1;
  regs[0]=clone_child ? 0 : 123;
  if(clone_child) requested_child_sp=(unsigned)CHILD_SP;
  return;
 }
 if(sc != 0x50+current)
 failed=1;
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
 if(__mlibc_clone_arm(0x111,CHILD_SP,(void*)0x1234,(void*)0x5566,(void*)0x7788)!=123) return 10;
 child_stack[510]=0x1212;child_stack[511]=0x3434;
 clone_child=1;
 __mlibc_clone_arm(0x111,CHILD_SP,(void*)0x1234,(void*)0x5566,(void*)0x7788);
 return 11;

}
