# SPDX-License-Identifier: MIT
# Materialize ELF PT_LOAD bytes, preserving the loader's ELF header at offset0.
image = File.binread(ARGV.fetch(0))
abort 'expected ELF32 little-endian' unless image.start_with?("\x7fELF\x01\x01".b)
entry, phoff = image.byteslice(24, 8).unpack('V2')
phsize, count = image.byteslice(42, 4).unpack('v2')
abort 'bad program-header size' unless phsize == 32
flat = ''.b
count.times do |i|
 type, off, va, _, filesz, memsz = image.byteslice(phoff + i * phsize, 32).unpack('V8')
 next unless type == 1
 abort 'invalid load segment' unless filesz <= memsz && va + memsz <= 1024 * 1024 && off + filesz <= image.bytesize
 flat << "\0" * (va + memsz - flat.bytesize) if va + memsz > flat.bytesize
 flat[va, filesz] = image.byteslice(off, filesz)
end
abort 'missing header/entry' unless flat.start_with?("\x7fELF".b) && entry < flat.bytesize
File.binwrite(ARGV.fetch(1), flat)
File.write(ARGV.fetch(2), "#define ARM32_LOADER_ENTRY #{entry}\n")
