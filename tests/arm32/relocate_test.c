/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <elf.h>
#include <stdint.h>
#include <string.h>
int __mlibc_arm_relocate_self(uintptr_t, const Elf32_Dyn *);
static unsigned char image[1024] __attribute__((aligned(8)));
static Elf32_Dyn dynamic[10];
static uint32_t *place;
static Elf32_Rel *rel;
static Elf32_Sym *sym;
static uintptr_t base;
static void setup(unsigned type, unsigned index) {
 memset(image, 0, sizeof(image)); memset(dynamic, 0, sizeof(dynamic));
 base = (uintptr_t)image;
 sym = (void *)(image + 128); rel = (void *)(image + 256);
 place = (void *)(image + 512);
 place[0] = 5; place[1] = 0xcafebabe;
 rel->r_offset = 512; rel->r_info = ELF32_R_INFO(index, type);
 sym[1].st_shndx = 1; sym[1].st_value = 100;
 dynamic[0] = (Elf32_Dyn){.d_tag=DT_REL, .d_un.d_ptr=256};
 dynamic[1] = (Elf32_Dyn){.d_tag=DT_RELSZ, .d_un.d_val=8};
 dynamic[2] = (Elf32_Dyn){.d_tag=DT_RELENT, .d_un.d_val=8};
 dynamic[3] = (Elf32_Dyn){.d_tag=DT_SYMTAB, .d_un.d_ptr=128};
 dynamic[4] = (Elf32_Dyn){.d_tag=DT_SYMENT, .d_un.d_val=16};
}
int main(void) {
 setup(R_ARM_RELATIVE,0);
 assert(!__mlibc_arm_relocate_self(base,dynamic));
 assert(place[0] == (uint32_t)(base+5) && place[1] == 0xcafebabe);
 unsigned types[] = {R_ARM_ABS32,R_ARM_GLOB_DAT,R_ARM_JUMP_SLOT};
 for (unsigned i=0;i<3;i++) {
  setup(types[i],1); place[0]=2;
  sym[1].st_info=ELF32_ST_INFO(STB_GLOBAL,STT_FUNC); sym[1].st_value=101;
  assert(!__mlibc_arm_relocate_self(base,dynamic));
  assert(place[0] == ((uint32_t)(base+(types[i] == R_ARM_JUMP_SLOT ? 100 : 102))|1) && place[1] == 0xcafebabe);
 }
 setup(R_ARM_ABS32,1);sym[1].st_shndx=SHN_ABS;
 assert(!__mlibc_arm_relocate_self(base,dynamic) && place[0]==105);
 setup(R_ARM_ABS32,1);sym[1].st_shndx=0;sym[1].st_info=ELF32_ST_INFO(STB_WEAK,STT_FUNC);
 assert(!__mlibc_arm_relocate_self(base,dynamic) && place[0]==5);
 setup(R_ARM_ABS32,1);sym[1].st_shndx=0;sym[1].st_info=ELF32_ST_INFO(STB_GLOBAL,STT_FUNC);
 assert(__mlibc_arm_relocate_self(base,dynamic) && place[0]==5);
 setup(R_ARM_RELATIVE,1);assert(__mlibc_arm_relocate_self(base,dynamic));
 setup(R_ARM_REL32,1);assert(__mlibc_arm_relocate_self(base,dynamic));
 setup(R_ARM_RELATIVE,0);rel->r_offset=513;assert(__mlibc_arm_relocate_self(base,dynamic));
 setup(R_ARM_RELATIVE,0);dynamic[1].d_un.d_val=7;assert(__mlibc_arm_relocate_self(base,dynamic));
 setup(R_ARM_RELATIVE,0);dynamic[2].d_un.d_val=12;assert(__mlibc_arm_relocate_self(base,dynamic));
 setup(R_ARM_RELATIVE,0);dynamic[5]=(Elf32_Dyn){.d_tag=DT_RELASZ,.d_un.d_val=12};
 assert(__mlibc_arm_relocate_self(base,dynamic));
 setup(R_ARM_JUMP_SLOT,1);dynamic[1].d_un.d_val=0;
 dynamic[5]=(Elf32_Dyn){.d_tag=DT_JMPREL,.d_un.d_ptr=256};
 dynamic[6]=(Elf32_Dyn){.d_tag=DT_PLTRELSZ,.d_un.d_val=8};
 dynamic[7]=(Elf32_Dyn){.d_tag=DT_PLTREL,.d_un.d_val=DT_RELA};
 assert(__mlibc_arm_relocate_self(base,dynamic) && place[0]==5);
 dynamic[7].d_un.d_val=DT_REL;
 assert(!__mlibc_arm_relocate_self(base,dynamic));
 assert(place[0] == (uint32_t)(base+100) && place[1] == 0xcafebabe);
}
