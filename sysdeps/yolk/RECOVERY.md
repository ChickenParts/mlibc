# Yolk sysdeps recovery provenance

This branch reconstructs Yolk's mlibc port from reachable, content-addressed
history after the former superproject gitlink became unreachable.

The immutable inputs are:

- upstream-adjacent mlibc base commit:
  `dc186d3384b162f76029ec96a92cd8a7d5ee25a5`;
- Yolk superproject source commit:
  `0a88ed8f7147e09ebba968eb27b5c1481fb1f350`;
- recovered `mlibc-port/sysdeps/yolk` tree:
  `dd88468174c8d8fb46d4b2bfad970d2ef69610bc`;
- recovered `mlibc-port/abis/yolk` tree:
  `ccec320fc96bfd63245670edf49773ede54949c6`.

Every recovered source blob and symlink is reused byte-for-byte. The only
adaptation is the deterministic top-level Meson dispatch performed by
`apply-meson-dispatch.py`. It accepts only base blob
`a43c6a09a3873888e50af23950fdbc808421b525` and produces only
`081379e1bdc51353b487263645aa0a036aecde9f`.

Yolk stages this exact commit into an isolated build-source directory, runs the
checked transformer, and records the transformed source identity. The retained
git checkout itself remains clean and independently verifiable.
