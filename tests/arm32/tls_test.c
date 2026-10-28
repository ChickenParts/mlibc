/* SPDX-License-Identifier: MIT */
#include <mlibc/tls-layout.h>
__thread int value32 __attribute__((aligned(32)))=0x11223344;
__thread int value64 __attribute__((aligned(64)))=0x22334455;
extern unsigned char __tls_image_start[], __tls_image_end[];
static unsigned char area[2048] __attribute__((aligned(8)));
__attribute__((noinline)) static int access_tls(int step) {
 if(value32!=0x11223344+step || value64!=0x22334455+step) return 1;
 ++value32; ++value64;
 return 0;
}
__attribute__((noinline)) static void set_tp(uintptr_t tp) {
 __asm__ volatile("mcr p15,0,%0,c13,c0,3\nisb" : : "r"(tp) : "memory");
}
int test_main(void) {
 unsigned failed=0;
 for(unsigned thread=0;thread<2;thread++) {
  uintptr_t allocation=(uintptr_t)area+thread*1024+1;
  uintptr_t tcb=__mlibc_arm_tcb_address(allocation,88,64);
  uintptr_t tp=__mlibc_arm_tp_from_tcb(tcb,88);
  uintptr_t tls=tcb+88+__mlibc_arm_tls_offset(0,64);
  if(tcb<allocation || (tcb&7) || (tls&63) || __mlibc_arm_tcb_from_tp(tp,88)!=tcb) failed=1;
  unsigned char *destination=(void*)tls;
  for(size_t i=0;i<(size_t)(__tls_image_end-__tls_image_start);i++) destination[i]=__tls_image_start[i];
  set_tp(tp);
  if(access_tls(0) || access_tls(1)) failed=1;
 }
 register unsigned op __asm__("r0")=4;
 register const char *msg __asm__("r1")=failed ? "ARM32_TLS_FAIL\n" : "ARM32_TLS_OK\n";
#if defined(__thumb__)
 __asm__ volatile("svc 0xab" : "+r"(op),"+r"(msg) : : "memory");
#else
 __asm__ volatile("svc 0x123456" : "+r"(op),"+r"(msg) : : "memory");
#endif
 return failed;
}
