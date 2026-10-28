/* SPDX-License-Identifier: MIT */
#include <bits/syscall.h>
#include <chickenos/offset-syscalls.h>
#ifdef TEST_KERNEL_ABI
#include <chicken/syscall_numbers.h>
_Static_assert(__CHICKEN_LSEEK64 == SYS_LSEEK64 && __CHICKEN_TRUNCATE64 == SYS_TRUNCATE64 &&
 __CHICKEN_FTRUNCATE64 == SYS_FTRUNCATE64 && __CHICKEN_MMAP_PAGES == SYS_MMAP_PAGES &&
 SYS_pread64 == SYS_PREAD64 && SYS_pwrite64 == SYS_PWRITE64, "ChickenOS kernel offset syscall numbers");
#endif

#if defined(__thumb__)
#define SEMIHOST "svc 0xab"
#else
#define SEMIHOST "svc 0x123456"
#endif
_Static_assert(SYS_get_thread_area == 0x302 && SYS_clone == 0x107, "assembly syscall ABI");
#ifdef TEST_SYSDEPS
extern int actual_seek(int, long long, int, long long *) __asm__("_ZN5mlibc8sys_seekEixiPx");
extern int actual_pread(int, void *, unsigned, long long, int *) __asm__("_ZN5mlibc9sys_preadEiPvjxPi");
extern int actual_pwrite(int, const void *, unsigned, long long, int *) __asm__("_ZN5mlibc10sys_pwriteEiPKvjxPi");
extern int actual_truncate(const char *, long long) __asm__("_ZN5mlibc12sys_truncateEPKcx");
extern int actual_vm_map(void *, unsigned, int, int, int, long long, void **) __asm__("_ZN5mlibc10sys_vm_mapEPvjiiixPS0_");
#endif
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
if(sc==0x407) { if(regs[0]!=3 || regs[1]!=0xfffffffeu || regs[2]!=0xfffffffeu || regs[3]!=2) failed=1; regs[0]=0x89abcdefu; regs[1]=0x12345678; return; }
if(sc==0x408 || sc==0x409) { if(regs[0]!=3 || regs[1]!=0x1234 || regs[2]!=8 || regs[3]!=0x89abcdefu || regs[4]!=0x12345678) failed=1; regs[0]=71; return; }
if(sc==0x415 || sc==0x416) { if(regs[0]!=3 || regs[1]!=0xfffffffeu || regs[2]!=0xfffffffeu) failed=1; regs[0]=72; return; }
if(sc==0xce) { if(regs[0]!=1 || regs[1]!=2 || regs[2]!=3 || regs[3]!=4 || regs[4]!=5 || regs[5]!=0x123456) failed=1; regs[0]=73; return; }
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
if (__chicken_seek(3,-0x100000002LL,2)!=0x1234567889abcdefLL) return 20;
if (__chicken_pread(3,0x1234,8,0x1234567889abcdefLL)!=71) return 21;
if (__chicken_pwrite(3,0x1234,8,0x1234567889abcdefLL)!=71) return 22;
if (__chicken_truncate(3,-0x100000002LL)!=72 || __chicken_ftruncate(3,-0x100000002LL)!=72) return 23;
if (!__chicken_mmap_offset_valid(0x123456000LL) || __chicken_mmap(1,2,3,4,5,0x123456000LL)!=73) return 24;
if (__chicken_mmap_offset_valid(-4096) || __chicken_mmap_offset_valid(4097) || __chicken_mmap_offset_valid(0x100000000000LL)) return 25;
#ifdef TEST_SYSDEPS
 long long position=0; int count=0; void *window=0;
 if(actual_seek(3,-0x100000002LL,2,&position) || position!=0x1234567889abcdefLL) return 26;
 if(actual_pread(3,(void *)0x1234,8,0x1234567889abcdefLL,&count) || count!=71) return 27;
 if(actual_pwrite(3,(void *)0x1234,8,0x1234567889abcdefLL,&count) || count!=71) return 28;
 if(actual_truncate((const char *)3,-0x100000002LL)) return 29;
 if(actual_vm_map((void *)1,2,3,4,5,0x123456000LL,&window) || window!=(void *)73) return 30;
 if(actual_vm_map((void *)1,2,3,4,5,-4096,&window)!=0x50) return 31;
#endif
 if(__mlibc_clone_arm(0x111,CHILD_SP,(void*)0x1234,(void*)0x5566,(void*)0x7788)!=123) return 10;
 child_stack[510]=0x1212;child_stack[511]=0x3434;
 clone_child=1;
 __mlibc_clone_arm(0x111,CHILD_SP,(void*)0x1234,(void*)0x5566,(void*)0x7788);
 return 11;

}
