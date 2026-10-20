# SPDX-License-Identifier: MIT
require 'fileutils'
require 'open3'
cc, prefix, out = ARGV
abort 'usage: check_libc_link.rb CLANG STAGED_PREFIX OUTPUT_DIR' unless cc && prefix && out
FileUtils.mkdir_p(out)
lib = File.join(prefix, 'lib')
%w[crt1.o crti.o crtn.o libc.a libc.so ld.so].each do |name|
 abort "missing staged library: #{name}" unless File.file?(File.join(lib, name))
end
compile = ['--target=arm-none-eabi', '-march=armv7-a', '-mfloat-abi=soft', "--sysroot=#{prefix}", '-O2']
runtime, status = Open3.capture2(cc, *compile, '-print-libgcc-file-name')
runtime = runtime.strip
abort 'ARM compiler-runtime archive is missing' unless status.success? && File.file?(runtime)
def run!(*args)
 abort "failed: #{args.first}" unless system(*args)
end
static_obj = File.join(out, 'static.o')
shared_obj = File.join(out, 'shared.o')
dso_obj = File.join(out, 'dso.o')
static_elf = File.join(out, 'static.elf')
shared_elf = File.join(out, 'shared.elf')
dso = File.join(out, 'libarm32-test.so')
run!(cc, *compile, '-marm', '-c', File.join(__dir__, 'libc_link_test.c'), '-o', static_obj)
run!(cc, *compile, '-marm', '-fPIE', '-DARM32_SHARED_TEST=1', '-c', File.join(__dir__, 'libc_link_test.c'), '-o', shared_obj)
run!(cc, *compile, '-mthumb', '-fPIC', '-c', File.join(__dir__, 'libc_link_dso.c'), '-o', dso_obj)
startup = %w[crt1.o crti.o].map { |name| File.join(lib, name) }
ending = File.join(lib, 'crtn.o')
run!(cc, '--target=arm-none-eabi', '-march=armv7-a', '-mfloat-abi=soft', '-nostdlib', '-fuse-ld=lld', '-static',
     *startup, static_obj, '-Wl,--start-group', File.join(lib, 'libc.a'), runtime, '-Wl,--end-group', ending, '-o', static_elf)
# Select the ELF shared-link driver; no Linux code/headers/libraries are inputs.
shared_link = ['--target=arm-unknown-linux-gnueabi', '-march=armv7-a', '-mfloat-abi=soft', '-nostdlib', '-fuse-ld=lld', "-Wl,-rpath-link,#{lib}"]
run!(cc, *shared_link, '-shared', '-Wl,--no-undefined,-soname,libarm32-test.so', dso_obj, File.join(lib, 'libc.so'), '-o', dso)
run!(cc, *shared_link, '-pie', '-Wl,--no-undefined,--dynamic-linker=/lib/ld.so,-rpath,/lib',
     *startup, shared_obj, File.join(lib, 'libc.so'), ending, '-o', shared_elf)
[[static_elf,2],[shared_elf,3],[dso,3]].each do |file,type|
 bytes = File.binread(file)
 abort "wrong ELF ABI: #{file}" unless bytes.start_with?("\x7fELF\x01\x01".b) && bytes.byteslice(16,4).unpack('v2') == [type,40]
 abort "wrong FP ABI: #{file}" unless bytes.byteslice(36,4).unpack1('V') & 0x600 == 0x200
end
puts 'PASS ARM32 libc links: static + PIE + Thumb DSO with dynamic TLS (not executed)'
