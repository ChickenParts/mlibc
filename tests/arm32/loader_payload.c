/* SPDX-License-Identifier: MIT */
/* Only the interpreter's final handoff is a fixture; bootstrap is production. */
__attribute__((visibility("default"))) int exported_value = 0x12345678;
__attribute__((visibility("default"))) int *exported_pointer = &exported_value;
static int local_value = 0x13579;
static int *volatile local_pointer = &local_value;
static unsigned initial_sp;
__attribute__((noinline,visibility("default"))) int exported_function(int x) { return x*2; }
__attribute__((noreturn)) static void finished(void) {
 register unsigned op __asm__("r0") = 4;
 register const char *message __asm__("r1") = "MLIBC_ARM32_LOADER_OK\n";
#if defined(__thumb__)
 __asm__ volatile("svc 0xab" : "+r"(op), "+r"(message) : : "memory");
#else
 __asm__ volatile("svc 0x123456" : "+r"(op), "+r"(message) : : "memory");
#endif
 op = 0x18;

 register unsigned status __asm__("r1") = 0x20026;
#if defined(__thumb__)
 __asm__ volatile("svc 0xab" : : "r"(op), "r"(status) : "memory");
#else
 __asm__ volatile("svc 0x123456" : : "r"(op), "r"(status) : "memory");
#endif
 for (;;) {}
}
static void (*volatile finish_pointer)(void) = finished;
__attribute__((visibility("hidden"))) unsigned interpreterMain(unsigned *stack) {
 initial_sp = (unsigned)stack;
 if (initial_sp != 0x40090000 || exported_pointer != &exported_value ||
     *exported_pointer != 0x12345678 || local_pointer != &local_value ||
     *local_pointer != 0x13579 || finish_pointer != finished || exported_function(21) != 42)
  __builtin_trap();
 return (unsigned)finish_pointer;
}
