/* rv32imac / ilp32: no FPU (sstatus.FS is never enabled by the ChickenOS
 * kernel), so the fenv.h entry points are not provided; the assembly body is
 * only assembled where the compiler advertises hardware FP (__riscv_flen). */
#ifdef __riscv_flen
#error "riscv32 hard-float fenv is not implemented: ChickenOS riscv32 is ilp32 (soft float)"
#endif
