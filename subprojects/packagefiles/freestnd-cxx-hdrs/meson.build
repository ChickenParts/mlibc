project('freestnd-cxx-hdrs')

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

incl = include_directories(cpu / 'include', is_system: true)

freestnd_cxx_hdrs_dep = declare_dependency(include_directories: incl)
