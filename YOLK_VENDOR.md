# Yolk vendor branch provenance

This branch reconstructs the Yolk mlibc port after the historical submodule
commit `8d72308ac224755f5917db304b2c88ab3c817d5b` became unreachable from every
configured GitHub repository.

The branch is deliberately rooted at upstream-era commit
`dc186d3384b162f76029ec96a92cd8a7d5ee25a5` (2026-02-06), immediately before
Yolk moved its then-in-tree port into the mlibc submodule. The Yolk sysdeps are
recovered from Yolk commit `0a88ed8f7147e09ebba968eb27b5c1481fb1f350`;
the complete Yolk-owned ABI header tree is recovered from the current
superproject.

`sysdeps/menix` is a repository symlink to `sysdeps/yolk`. This uses the
pre-existing root dispatch slot without rewriting the upstream top-level Meson
file; `sysdeps/yolk/meson.build` restores `MLIBC_SYSTEM_NAME` to `yolk` before
configuration headers are generated. Yolk cross files for this recovery branch
must therefore use `system = 'menix'` while the produced libc continues to
identify its target system as Yolk.

This commit is a durable, auditable recovery baseline, not a claim that the
entire libc or every Yolk syscall wrapper has passed the final Yolk architecture
matrix. Follow-up fixes must remain ordinary commits on `vendor/yolk`, and the
Yolk superproject must pin each accepted commit exactly.
