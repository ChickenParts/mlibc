/* SPDX-License-Identifier: MIT */
/* AAELF32 dynamic REL relocations for the interpreter's OWN trusted image.
 * No libc, TLS, allocation, external-symbol lookup or pre-relocated GOT use.
 * This is not an ELF admission/validation API for untrusted input. */
#include <elf.h>
#include <stdint.h>
#include <stddef.h>

static int relocate_table(uintptr_t base, uintptr_t table, uint32_t bytes,
                         const Elf32_Sym *symbols) {
 if (bytes % sizeof(Elf32_Rel)) return 1;
 const Elf32_Rel *rels = (const void *)(base + table);
 for (uint32_t i = 0; i < bytes / sizeof(*rels); ++i) {
  uint32_t type = ELF32_R_TYPE(rels[i].r_info);
  uint32_t index = ELF32_R_SYM(rels[i].r_info);
  if (type == R_ARM_NONE) continue;
  if (rels[i].r_offset & 3) return 1;
  uint32_t *place = (void *)(base + rels[i].r_offset);
  if (type == R_ARM_RELATIVE) {
   if (index) return 1;
   *place += base;
   continue;
  }
  if (type != R_ARM_ABS32 && type != R_ARM_GLOB_DAT && type != R_ARM_JUMP_SLOT)
   return 1;
  if (!index || !symbols) return 1;
  const Elf32_Sym *sym = &symbols[index];
  uint32_t address, thumb = 0;
  if (!sym->st_shndx) {
   if (ELF32_ST_BIND(sym->st_info) != STB_WEAK) return 1;
   address = 0;
  } else {
   address = sym->st_value;
   if (ELF32_ST_TYPE(sym->st_info) == STT_FUNC) {
    thumb = address & 1;
    address &= ~1u;
   }
   if (sym->st_shndx != SHN_ABS) address += base;
  }
  /* An unresolved PLT slot holds the resolver stub, not an addend. */
  uint32_t addend = type == R_ARM_JUMP_SLOT ? 0 : *place;
  *place = (address + addend) | thumb;
 }
 return 0;
}

__attribute__((visibility("hidden")))
int __mlibc_arm_relocate_self(uintptr_t base, const Elf32_Dyn *dynamic) {
 uintptr_t rel = 0, plt = 0, sym = 0;
 uint32_t relsz = 0, pltsz = 0, plttype = DT_REL;
 for (size_t i = 0; dynamic[i].d_tag != DT_NULL; ++i) {
  switch (dynamic[i].d_tag) {
   case DT_REL: rel = dynamic[i].d_un.d_ptr; break;
   case DT_RELSZ: relsz = dynamic[i].d_un.d_val; break;
   case DT_RELENT: if (dynamic[i].d_un.d_val != sizeof(Elf32_Rel)) return 1; break;
   case DT_JMPREL: plt = dynamic[i].d_un.d_ptr; break;
   case DT_PLTRELSZ: pltsz = dynamic[i].d_un.d_val; break;
   case DT_PLTREL: plttype = dynamic[i].d_un.d_val; break;
   case DT_SYMTAB: sym = dynamic[i].d_un.d_ptr; break;
   case DT_SYMENT: if (dynamic[i].d_un.d_val != sizeof(Elf32_Sym)) return 1; break;
   case DT_RELASZ: case DT_RELRSZ:
    if (dynamic[i].d_un.d_val) return 1;
    break;
  }
 }
 if ((relsz && !rel) || (pltsz && (!plt || plttype != DT_REL))) return 1;
 const Elf32_Sym *symbols = sym ? (const void *)(base + sym) : NULL;
 if (relocate_table(base, rel, relsz, symbols)) return 1;
 return relocate_table(base, plt, pltsz, symbols);
}
