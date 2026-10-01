#pragma once

#include <elf.h>
#include <sys/hwprobe.h>

#define ELF_CLASS ELFCLASS32
#define ELF_MACHINE EM_RISCV

using elf_ehdr = Elf32_Ehdr;
using elf_phdr = Elf32_Phdr;
using elf_dyn = Elf32_Dyn;
using elf_rel = Elf32_Rel;
using elf_rela = Elf32_Rela;
using elf_relr = Elf32_Relr;
using elf_sym = Elf32_Sym;
using elf_addr = Elf32_Addr;

using elf_info = Elf32_Word;
using elf_addend = Elf32_Sword;

using elf_version = Elf32_Half;
using elf_verdef = Elf32_Verdef;
using elf_verdaux = Elf32_Verdaux;
using elf_verneed = Elf32_Verneed;
using elf_vernaux = Elf32_Vernaux;

#define ELF_R_SYM ELF32_R_SYM
#define ELF_R_TYPE ELF32_R_TYPE
#define ELF_ST_BIND ELF32_ST_BIND
#define ELF_ST_TYPE ELF32_ST_TYPE

#define R_NONE R_RISCV_NONE
#define R_JUMP_SLOT R_RISCV_JUMP_SLOT
#define R_ABSOLUTE R_RISCV_32
#define R_GLOB_DAT R_RISCV_32
#define R_RELATIVE R_RISCV_RELATIVE
#define R_IRELATIVE R_RISCV_IRELATIVE
#define R_COPY R_RISCV_COPY
#define R_TLS_DTPMOD R_RISCV_TLS_DTPMOD32
#define R_TLS_DTPREL R_RISCV_TLS_DTPREL32
#define R_TLS_TPREL R_RISCV_TLS_TPREL32
#define R_TLSDESC R_RISCV_TLSDESC

#define TP_TCB_OFFSET 0

using ifunc_handler = elf_addr (*)(uint64_t, __riscv_hwprobe_t, void *);
