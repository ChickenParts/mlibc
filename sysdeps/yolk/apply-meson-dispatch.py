#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Enable the recovered Yolk sysdeps in the pinned mlibc Meson graph."""

from __future__ import annotations

import hashlib
from pathlib import Path
import sys

BASE_BLOB = "a43c6a09a3873888e50af23950fdbc808421b525"
RESULT_BLOB = "081379e1bdc51353b487263645aa0a036aecde9f"

OLD = """elif host_machine.system() == 'menix'
\trtld_include_dirs += include_directories('sysdeps/menix/include')
\tlibc_include_dirs += include_directories('sysdeps/menix/include')
\tinternal_conf.set10('MLIBC_MAP_DSO_SEGMENTS', true)
\tinternal_conf.set10('MLIBC_MMAP_ALLOCATE_DSO', true)
\tinternal_conf.set10('MLIBC_MAP_FILE_WINDOWS', true)
\tsubdir('sysdeps/menix')
# ANCHOR: demo-sysdeps
"""

NEW = """elif host_machine.system() == 'menix'
\trtld_include_dirs += include_directories('sysdeps/menix/include')
\tlibc_include_dirs += include_directories('sysdeps/menix/include')
\tinternal_conf.set10('MLIBC_MAP_DSO_SEGMENTS', true)
\tinternal_conf.set10('MLIBC_MMAP_ALLOCATE_DSO', true)
\tinternal_conf.set10('MLIBC_MAP_FILE_WINDOWS', true)
\tsubdir('sysdeps/menix')
elif host_machine.system() == 'yolk'
\trtld_include_dirs += include_directories('sysdeps/yolk/include')
\tlibc_include_dirs += include_directories('sysdeps/yolk/include')
\tinternal_conf.set10('MLIBC_MAP_DSO_SEGMENTS', true)
\tinternal_conf.set10('MLIBC_MMAP_ALLOCATE_DSO', true)
\tinternal_conf.set10('MLIBC_MAP_FILE_WINDOWS', true)
\tsubdir('sysdeps/yolk')
# ANCHOR: demo-sysdeps
"""


def git_blob_id(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def main() -> int:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else Path(__file__).resolve().parents[2]
    meson = root / "meson.build"
    data = meson.read_bytes()
    current = git_blob_id(data)
    if current == RESULT_BLOB:
        print(f"[yolk-meson] already applied: {RESULT_BLOB}")
        return 0
    if current != BASE_BLOB:
        raise SystemExit(f"[yolk-meson] unexpected meson.build blob {current}; expected {BASE_BLOB}")

    text = data.decode("utf-8")
    if text.count(OLD) != 1:
        raise SystemExit("[yolk-meson] dispatch anchor is missing or ambiguous")
    result = text.replace(OLD, NEW).encode("utf-8")
    actual = git_blob_id(result)
    if actual != RESULT_BLOB:
        raise SystemExit(f"[yolk-meson] generated blob {actual}; expected {RESULT_BLOB}")
    meson.write_bytes(result)
    print(f"[yolk-meson] PASS: {BASE_BLOB} -> {RESULT_BLOB}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
