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

## Dynamic interpreter bootstrap

`check_loader_arm` and `check_loader_thumb` link actual ELF32 shared objects,
materialize their PT_LOAD segments, copy them to a nonzero runtime base and
execute the production interpreter entry/self-relocator. The final
`interpreterMain` is a small fixture, not the full mlibc loader. It checks local
and exported data/function pointers, calls through a relocated PLT and returns
an ARM/Thumb entry pointer. The original SP must survive. Completion requires
an explicit semihosted marker as well as successful QEMU exit; QMP quit alone
cannot pass. The generated objects exercise RELATIVE, ABS32, GLOB_DAT and
JUMP_SLOT; this is not yet shared-libc/dlopen/TLS acceptance.

`check_relocations` also checks exact 32-bit writes, Thumb pointer state,
absolute and undefined weak symbols, unresolved strong symbols, malformed
record sizes, misalignment, unsupported types and RELA PLT refusal. The bootstrap
operates only on its own trusted loaded image; it is not an untrusted ELF parser.
