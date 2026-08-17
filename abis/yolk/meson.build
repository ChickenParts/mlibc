# Yolk ABI headers for mlibc
# SPDX-License-Identifier: MIT

# No sources - just headers
libc_include_dirs += include_directories('.')

# Install headers to abi-bits
# Note: We use Linux's ABI headers for most things
# Only override minimal headers where Yolk has specific requirements
install_headers(
    'abi.h',
    'fcntl.h',
    'mman.h',
    'stat.h',
    'vm.h',
    'wait.h',
    subdir: 'abi-bits'
)
