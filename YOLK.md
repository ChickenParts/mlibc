# Yolk mlibc branch

This branch carries the Yolk OS ABI and sysdeps on top of the current mlibc
`master` line.

## Provenance

The Yolk port was recovered from the historical branch
`the_missing_yolk_stuff`, whose tip is:

```text
8d72308ac224755f5917db304b2c88ab3c817d5b
```

The initial rebased branch commit has parent:

```text
477df204fd5ba836f23e3a63864c140db2c5c90c
```

which was the fork's current `master` and matched upstream when the branch was
created.

The recovery imports the final historical Yolk trees exactly:

```text
abis/yolk     cc4c823f345a9d9feb1595c4c3a638d50534370d
sysdeps/yolk  e24810901c0a436b333eb25ae84e994d879a7ca4
```

No obsolete copy of the old upstream tree is merged. Shared mlibc files are
ported separately against the current upstream source so upstream fixes are not
rolled back.

## Maintenance policy

- `master` remains an upstream-tracking branch.
- `yolk` is the durable Yolk integration branch.
- Future upstream updates should rebase or replay the small Yolk integration
  commits onto the updated `master`, followed by the Yolk cross-build and
  userspace ABI gates.
- The historical recovery branch remains available as provenance and should not
  be used as the active submodule pin.
