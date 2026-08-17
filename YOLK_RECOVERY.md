# Yolk mlibc recovery branch

This branch reconstructs the Yolk port from Git objects still reachable in the
Yolk superproject after the historical submodule commit was lost.

Provenance:

- upstream base: `dc186d3384b162f76029ec96a92cd8a7d5ee25a5`;
- recovered Yolk sysdeps and architecture startup objects: Yolk commit
  `0a88ed8f7147e09ebba968eb27b5c1481fb1f350`;
- Yolk ABI overrides: the retained `mlibc-port/abis/yolk` tree;
- all unmodified ABI headers: the upstream base's `abis/linux` tree.

The root Meson file on this upstream snapshot has no `yolk` branch. To avoid a
large and conflict-prone top-level fork, `sysdeps/demo` is a symlink to
`sysdeps/yolk`; Yolk cross files select Meson system `demo`, while the Yolk
sysdep immediately writes `MLIBC_SYSTEM_NAME` as `yolk`. This alias is a build
selection mechanism only and is covered by Yolk's source-pin gate.

Recovery hardening adds:

- current Core-Yolk syscall numbers and six-argument entry paths on x86-64,
  AArch64, and RISC-V 64;
- syscall-mediated x86 FS-base management;
- byte-count `getdents` with complete record/cursor validation;
- `msync` support;
- real directory seek/tell state and bounded `readdir` parsing;
- clone trampolines, thread entry, TLS setup, and signal-restorer assembly on
  all three maintained architectures;
- compile-time `Tcb` size assertions: 144 bytes on x86-64 and 136 bytes on
  AArch64/RISC-V 64.

The stale recovered `generic/thread.cpp` implementation is intentionally absent;
`generic/thread_recovery.cpp` is the only compiled thread sysdep.

This branch is immutable once referenced by a Yolk lock file. Further port work
must use a descendant branch and update the superproject lock and gitlink
together.
