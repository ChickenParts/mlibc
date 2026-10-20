/* SPDX-License-Identifier: MIT */
/* Freestanding executable test of production assembly, with a link-only signal helper shim. */
#include <bits/machine.h>
#include <mlibc-asm/jmp_buf.h>
_Static_assert(sizeof(struct __mlibc_jmpbuf_register_state) == JMP_BUF_SAVESIGS_OFFSET, "ARM32 jump layout");
extern int setjmp(void *) __attribute__((returns_twice));
extern void longjmp(void *, int) __attribute__((noreturn));
static unsigned env[64] __attribute__((aligned(8)));
static volatile unsigned phase;
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
 return probe(env) ? 0 : 3;
}
