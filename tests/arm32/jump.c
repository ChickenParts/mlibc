/* SPDX-License-Identifier: MIT */
/* Freestanding executable test of production assembly, with a link-only signal helper shim. */
#include <bits/machine.h>
#include <mlibc-asm/jmp_buf.h>
_Static_assert(sizeof(struct __mlibc_jmpbuf_register_state) == JMP_BUF_SAVESIGS_OFFSET, "ARM32 jump layout");
extern int setjmp(void *) __attribute__((returns_twice));
extern void longjmp(void *, int) __attribute__((noreturn));
static unsigned env[64] __attribute__((aligned(8)));
static volatile unsigned phase;
extern int fegetround(void), __fesetround(int), fegetenv(unsigned *);
extern int fesetenv(const unsigned *), fetestexcept(int), feclearexcept(int), feraiseexcept(int);

extern int probe(void *);
int __sigsetjmp(void *p, int save) { (void)p; (void)save; return 0; }
int test_main(void) {
 env[26] = 0xabcdef;
 int r = setjmp(env);
 if (env[26] != 0) return 4;
 if (!phase) { phase = 1; longjmp(env, 0); }
 if (phase == 1) {
  if (r != 1) return 1;
  phase = 2; longjmp(env, -7);
 }
 if (r != -7) return 2;
 if (!probe(env)) return 3;
 unsigned fp_env=42;
 if (fegetround()!=0 || __fesetround(0)!=0 || __fesetround(1)==0) return 5;
 if (fegetenv(&fp_env)!=0 || fp_env!=0 || fesetenv(&fp_env)!=0 || fesetenv((const unsigned *)-1)!=0) return 6;
 fp_env=1;
 if (fesetenv(&fp_env)==0 || fetestexcept(-1)!=0 || feclearexcept(0)!=0 || feraiseexcept(0)!=0 || feraiseexcept(1)==0) return 7;
 return 0;

}
