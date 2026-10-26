project('freestnd-c-hdrs')

cpu = host_machine.cpu_family()
if cpu == 'x86'
  cpu = 'i686'
endif
# ChickenOS: these are GCC's freestanding headers, which are generic across the
# riscv family (the riscv64 and i686 trees differ by two x86-only config lines);
# there is no riscv32 tree, so riscv32 borrows riscv64's.
# Architecture properties come from compiler predefined macros.
if cpu in ['riscv32', 'ppc']
  cpu = 'riscv64'
endif

include_dir_path = meson.current_source_dir() / cpu / 'include'

# We use `-idirafter` so that clang prefers its own resource headers over these
freestnd_c_hdrs_dep = declare_dependency(
	compile_args: ['-idirafter' + include_dir_path]
)
