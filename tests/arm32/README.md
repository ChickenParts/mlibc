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

## ChickenOS syscall/TLS helper ABI

Four `check_syscall_v5-arm`, `check_syscall_v5-thumb`, `check_syscall_v7-arm`
and `check_syscall_v7-thumb` executions enter actual user mode and trap through
an executable SVC vector. The fixture handler validates zero through six
argument registers and returns a known 64-bit r1:r0 pair. These are the port's
actual assembly entry points, called using the compiler's AAPCS stack/register
convention. The TLS helper preserves r1-r3/r7; pre-v7 calls SYS_get_thread_area,
while v7 reads TPIDRURO. The syscall number is statically checked against the
ChickenOS header. All four cases execute on Cortex-A15; the v5 cases exercise
the older-ISA library path, not an older kernel/vector implementation.

This is an ABI execution fixture, not the complete ChickenOS kernel. That kernel
must still implement ARM32 syscall dispatch, wide results and per-thread TP state.

## Process startup glue

`check_crt_arm` and `check_crt_thumb` execute the actual crt1/crti/crtn assembly
with ARMv5TE-compatible ARM or Thumb C fixtures. They verify the unchanged
entry stack, argc/argv/environment delivery to the entry shim, the main function
pointer's instruction state, and initializer/finalizer execution with 8-byte
stack alignment. The __mlibc_entry body is a fixture: actual libc initialization,
exit handling and ChickenOS process execution remain separate acceptance.

## Clone and over-aligned TLS

The four syscall fixtures also exercise the production clone wrapper: exact
register/stack arguments, parent return and child entry on a newly selected
user stack. This does not implement kernel clone or scheduling.

`check_tls_arm` and `check_tls_thumb` execute compiler-generated local-exec
accesses to real aligned(32) and aligned(64) TLS variables. Two independent
storage areas verify placement and isolation using the production TP/TCB
helpers. The old layout fails this regression. These bring the total to sixteen
QEMU execution cases.

## Complete libraries and client links

Use `ci/chickenos-armv7a.cross-file` with an ARM-capable Clang on PATH:

```
meson setup /mnt/build/biscuit/mlibc-arm32/full \
  --cross-file ci/chickenos-armv7a.cross-file \
  -Ddefault_library=both -Dbuild_tests=false -Duse_freestnd_hdrs=enabled \
  -Dlinux_kernel_headers=/home/blanham/projects/ChickenOS_/lib/glibc/sysdeps/unix/sysv/chickenos/include
ninja -C /mnt/build/biscuit/mlibc-arm32/full
DESTDIR=/mnt/build/biscuit/mlibc-arm32/sysroot ninja -C /mnt/build/biscuit/mlibc-arm32/full install
```

A separate build with `-mthumb` replacing `-marm` in the cross file covers
Thumb-2 C/C++ plus ARM assembly. Keep the explicit soft-float ABI.

Configure the architecture fixture build with
`-DARM32_LIBC_SYSROOT=/mnt/build/biscuit/mlibc-arm32/sysroot/usr/local`
to include `check_libc_links` in `check`. It links actual static and PIE clients
and a Thumb DSO with dynamic TLS, constructors and dlopen references. These
full-libc clients are link acceptance only, not executed ChickenOS processes.

The complete profile currently requires ARMv7 lock-free atomics. Earlier-ISA
assembly coverage does not imply a complete pre-v7 libc. Dynamic startup,
dlopen, signals and kernel thread/TLS lifecycle need end-to-end ChickenOS tests;
lazy PLT, TLSDESC, IFUNC and hardware FP are not admitted by this milestone.

Real-kernel integration checkpoint (2026-10-21): the ChickenOS
`feature/biscuit-playback-20261020` worktree's Ninja target
`check-arm32-kernel-userspace` boots the full shared kernel through Rooster/Crow
and executes both clients built by `check_libc_links`. Static and PIE clients
both emitted `ARM32_LIBC_OK`; the PIE exercises the real interpreter,
`dlopen`/`dlsym`/`dlclose`, a Thumb DSO, TLS, constructors and stdio.
The first real PIE run exposed rejection of the valid ARM `PT_ARM_EXIDX`
metadata header; ARM builds now accept it alongside other non-load metadata.
This does not establish C++ unwinding, normal process exit, pthread execution,
fork/exec, signal semantics, complete services or physical Echo operation.
