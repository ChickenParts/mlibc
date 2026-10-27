#pragma once

#include <elf.h>
#include <stddef.h>
#include <stdint.h>

namespace mlibc {

// Keep this usable before any GOT entries have been relocated. Addr is the
// ELF address width, which can differ from uint64_t on 32-bit targets.
template<typename Addr, typename Rela, typename Sym>
inline void relocateSelfPlt(uintptr_t base, uintptr_t offset, size_t size,
		uintptr_t symtabOffset, unsigned jumpType) {
	if(!size || !symtabOffset)
		return;
	auto symtab = reinterpret_cast<const Sym *>(base + symtabOffset);
	for(size_t disp = 0; disp < size; disp += sizeof(Rela)) {
		auto reloc = reinterpret_cast<const Rela *>(base + offset + disp);
		auto type = sizeof(Addr) == 4 ? ELF32_R_TYPE(reloc->r_info)
			: ELF64_R_TYPE(reloc->r_info);
		auto sym = sizeof(Addr) == 4 ? ELF32_R_SYM(reloc->r_info)
			: ELF64_R_SYM(reloc->r_info);
		if(type != jumpType)
			__builtin_trap();
		auto target = reinterpret_cast<Addr *>(base + reloc->r_offset);
		*target = base + (sym ? symtab[sym].st_value : reloc->r_addend);
	}
}

} // namespace mlibc
