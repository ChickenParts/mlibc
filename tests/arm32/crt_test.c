/* SPDX-License-Identifier: MIT */
extern void _init(void), _fini(void);
static volatile unsigned init_state;
void init_body(void) {
 unsigned sp;
 __asm__ volatile("mov %0,sp" : "=r"(sp));
 init_state = (sp & 7) ? 99 : 1;
}
void fini_body(void) {
 unsigned sp;
 __asm__ volatile("mov %0,sp" : "=r"(sp));
 init_state = (sp & 7) || init_state != 1 ? 99 : 2;
}
extern int main
(int, char **, char **);
int main(int argc, char **argv, char **env) {
 return argc==1 && argv && argv[0] && argv[0][0]=='x' && !argv[1] && !env[0] ? 42 : 1;
}
__attribute__((noreturn)) void __mlibc_entry(unsigned *stack, int (*fn)(int,char**,char**)) {
 _init(); _fini();
 unsigned failed=init_state!=2 || fn!=main || ((unsigned)stack & 7) || stack[0]!=1;
 if(!failed && fn(1,(char**)(stack+1),(char**)(stack+3))!=42) failed=1;
 register unsigned op __asm__("r0")=4;
 register const char *msg __asm__("r1")=failed ? "ARM32_CRT_FAIL\n" : "ARM32_CRT_OK\n";
#if defined(__thumb__)
#define SEMIHOST "svc 0xab"
#else
#define SEMIHOST "svc 0x123456"
#endif
 __asm__ volatile(SEMIHOST : "+r"(op),"+r"(msg) : : "memory");
 op=0x18;
 register unsigned status __asm__("r1")=failed ? 0x20023 : 0x20026;
 __asm__ volatile(SEMIHOST : : "r"(op),"r"(status) : "memory");
 for(;;){}
}
