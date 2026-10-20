# ARM32 architecture execution tests

Configure with CMake/Ninja, passing an ARM-capable clang if the host clang
has no ARM backend:

```
cmake -S tests/arm32 -B /mnt/build/biscuit/mlibc-arm32 -G Ninja \
  -DARM_CC=/mnt/build/llvm/metachicken-llvm/bin/clang
ninja -C /mnt/build/biscuit/mlibc-arm32 check
```

Six actual QEMU executions cover v5TE ARM/Thumb-1 and v7-A ARM/Thumb-2
callers on Cortex-A15, plus v5TE ARM/Thumb-1 on ARM926/IntegratorCP.
The production libc jump assembly is ARM-state in every case. Thumb C callers
exercise call and longjmp return interworking; this is not a Thumb-only build.

Checks: zero longjmp result becomes one, negative result retained, savesigs
cleared, ABI buffer size matches assembly, r4-r11 deliberately clobbered and
restored, and an intentionally moved SP restored to the saved 8-byte alignment.
QEMU semihosting exits unsuccessfully on a failed check. There is no timeout.

The harness supplies a link-only __sigsetjmp shim. Signal-mask save/restore,
FP preservation, public setjmp.h integration, the dynamic loader, TLS, syscalls
and complete libc linking are not exercised by these tests. Soft-float is the
only admitted profile in this first slice; FP builds fail explicitly. The
buffer reserves space for future d8-d15 preservation without changing its
core-state offsets. Thumb-only M-profile and pre-v4T interworking are outside
this implementation.
