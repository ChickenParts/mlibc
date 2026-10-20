cmake_minimum_required(VERSION 3.25)
project(mlibc_arm32_jump C)
find_program(ARM_CC clang REQUIRED)
find_program(ARM_QEMU qemu-system-arm REQUIRED)
set(root ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_custom_target(check)
foreach(profile v5-arm v5-thumb v7-arm v7-thumb v5-oldcpu-arm v5-oldcpu-thumb)
 if(profile MATCHES "^v5")
  set(isa armv5te)
 else()
  set(isa armv7-a)
 endif()
 if(profile MATCHES "thumb$")
  set(mode -mthumb)
 else()
  set(mode -marm)
 endif()
 if(profile MATCHES "oldcpu")
  set(machine integratorcp)
  set(cpu arm926)
  set(linker oldcpu.ld)
 else()
  set(machine virt)
  set(cpu cortex-a15)
  set(linker test.ld)
 endif()
 add_custom_command(OUTPUT ${profile}.elf
  COMMAND ${ARM_CC} --target=arm-none-eabi -march=${isa} ${mode} -mfloat-abi=soft
   -O2 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -static -fuse-ld=lld
   -I${root}/options/internal/arm-include -I${root}/options/internal/include -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/${linker}
   ${CMAKE_CURRENT_SOURCE_DIR}/jump.c ${CMAKE_CURRENT_SOURCE_DIR}/start.S
   ${root}/options/internal/arm/setjmp.S -o ${profile}.elf
  DEPENDS jump.c start.S ${linker} ${root}/options/internal/arm/setjmp.S ${root}/options/internal/arm-include/mlibc-asm/jmp_buf.h ${root}/options/internal/include/bits/machine.h ${root}/options/internal/include/bits/types.h VERBATIM)
 add_custom_target(check_${profile}
  COMMAND ${CMAKE_COMMAND} -E env QEMU_AUDIO_DRV=none ${ARM_QEMU} -machine ${machine} -cpu ${cpu} -m 128M -kernel ${profile}.elf
   -semihosting-config enable=on,target=native -display none -serial none -no-reboot
   -qmp unix:${CMAKE_CURRENT_BINARY_DIR}/chickenos-mlibc-${profile}.sock,server,nowait
  COMMAND ${CMAKE_COMMAND} -E echo "PASS ${profile}: setjmp/longjmp and interworking"
  DEPENDS ${profile}.elf USES_TERMINAL VERBATIM)
 add_dependencies(check check_${profile})
endforeach()
find_program(ARM_RUBY ruby REQUIRED)
foreach(state arm thumb)
 set(out ${CMAKE_CURRENT_BINARY_DIR}/loader-${state})
 add_custom_command(OUTPUT ${out}.so
  COMMAND ${ARM_CC} --target=arm-none-eabi -march=armv7-a -m${state} -mfloat-abi=soft
   -O2 -ffreestanding -fno-builtin -fno-stack-protector -fPIC -nostdlib -fuse-ld=lld
   -Wl,-shared,-e,_start,--hash-style=sysv
   -I${root}/options/elf/include -I${root}/options/internal/include -I${root}/sysdeps/chickenos/include
   ${CMAKE_CURRENT_SOURCE_DIR}/loader_payload.c ${root}/options/rtld/arm/entry.S
   ${root}/options/rtld/arm/relocate.c -o ${out}.so
  DEPENDS loader_payload.c ${root}/options/rtld/arm/entry.S ${root}/options/rtld/arm/relocate.c ${root}/options/elf/include/elf.h VERBATIM)
 add_custom_command(OUTPUT ${out}/image.bin ${out}/loader_entry.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${out}
  COMMAND ${ARM_RUBY} ${CMAKE_CURRENT_SOURCE_DIR}/materialize_loader.rb ${out}.so ${out}/image.bin ${out}/loader_entry.h
  DEPENDS ${out}.so materialize_loader.rb VERBATIM)
 add_custom_command(OUTPUT ${out}.elf
  COMMAND ${ARM_CC} --target=arm-none-eabi -march=armv7-a -marm -nostdlib -static -fuse-ld=lld
   -I${out} -DARM32_LOADER_IMAGE="${out}/image.bin" -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/test.ld
   ${CMAKE_CURRENT_SOURCE_DIR}/loader_start.S -o ${out}.elf
  DEPENDS loader_start.S test.ld ${out}/image.bin ${out}/loader_entry.h VERBATIM)
 add_custom_target(check_loader_${state}
  COMMAND ${ARM_RUBY} ${CMAKE_CURRENT_SOURCE_DIR}/run_loader.rb ${ARM_QEMU} -machine virt -cpu cortex-a15 -m 128M -kernel ${out}.elf
   -semihosting-config enable=on,target=native -display none -serial none -no-reboot
   -qmp unix:${CMAKE_CURRENT_BINARY_DIR}/chickenos-mlibc-loader-${state}.sock,server,nowait
  COMMAND ${CMAKE_COMMAND} -E echo "PASS relocated ELF32 loader: ${state} entry"
  DEPENDS ${out}.elf USES_TERMINAL VERBATIM)
 add_dependencies(check check_loader_${state})
endforeach()
add_executable(arm32_relocate_test relocate_test.c ${root}/options/rtld/arm/relocate.c)
target_include_directories(arm32_relocate_test PRIVATE ${root}/options/elf/include ${root}/options/internal/include ${root}/sysdeps/chickenos/include)
target_compile_options(arm32_relocate_test PRIVATE -O2 -UNDEBUG -Wall -Wextra -Werror)
add_custom_target(check_relocations COMMAND arm32_relocate_test DEPENDS arm32_relocate_test)
add_dependencies(check check_relocations)
foreach(profile v5-arm v5-thumb v7-arm v7-thumb)
 if(profile MATCHES "^v5")
  set(isa armv5te)
 else()
  set(isa armv7-a)
 endif()
 if(profile MATCHES "thumb$")
  set(state thumb)
 else()
  set(state arm)
 endif()
 set(out ${CMAKE_CURRENT_BINARY_DIR}/syscall-${profile})
 add_custom_command(OUTPUT ${out}.elf
  COMMAND ${ARM_CC} --target=arm-none-eabi -march=${isa} -m${state} -mfloat-abi=soft
   -O2 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -static -fuse-ld=lld
   -I${root}/sysdeps/chickenos/include -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/test.ld
   ${CMAKE_CURRENT_SOURCE_DIR}/syscall_test.c ${CMAKE_CURRENT_SOURCE_DIR}/syscall_start.S
   ${root}/sysdeps/chickenos/arm/arch-syscall.S -o ${out}.elf
  DEPENDS syscall_test.c syscall_start.S test.ld ${root}/sysdeps/chickenos/arm/arch-syscall.S ${root}/sysdeps/chickenos/include/bits/syscall.h VERBATIM)
 add_custom_target(check_syscall_${profile}
  COMMAND ${ARM_RUBY} ${CMAKE_CURRENT_SOURCE_DIR}/run_loader.rb --marker=ARM32_SYSCALL_OK
   ${ARM_QEMU} -machine virt -cpu cortex-a15 -m 128M -kernel ${out}.elf
   -semihosting-config enable=on,target=native -display none -serial none -no-reboot
   -qmp unix:${CMAKE_CURRENT_BINARY_DIR}/chickenos-mlibc-syscall-${profile}.sock,server,nowait
  DEPENDS ${out}.elf USES_TERMINAL VERBATIM)
 add_dependencies(check check_syscall_${profile})
endforeach()
foreach(state arm thumb)
 set(out ${CMAKE_CURRENT_BINARY_DIR}/crt-${state})
 add_custom_command(OUTPUT ${out}.elf
  COMMAND ${ARM_CC} --target=arm-none-eabi -march=armv5te -m${state} -mfloat-abi=soft
   -O2 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -static -fuse-ld=lld
   -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/test.ld,-e,test_start
   ${CMAKE_CURRENT_SOURCE_DIR}/crt_test.c ${CMAKE_CURRENT_SOURCE_DIR}/crt_start.S
   ${root}/sysdeps/chickenos/arm/crt-src/crt1.S ${root}/sysdeps/chickenos/arm/crt-src/crti.S ${CMAKE_CURRENT_SOURCE_DIR}/crt_sections.S ${root}/sysdeps/chickenos/arm/crt-src/crtn.S -o ${out}.elf
  DEPENDS crt_test.c crt_start.S test.ld ${root}/sysdeps/chickenos/arm/crt-src/crt1.S ${root}/sysdeps/chickenos/arm/crt-src/crti.S crt_sections.S ${root}/sysdeps/chickenos/arm/crt-src/crtn.S VERBATIM)
 add_custom_target(check_crt_${state}
  COMMAND ${ARM_RUBY} ${CMAKE_CURRENT_SOURCE_DIR}/run_loader.rb --marker=ARM32_CRT_OK
   ${ARM_QEMU} -machine virt -cpu cortex-a15 -m 128M -kernel ${out}.elf
   -semihosting-config enable=on,target=native -display none -serial none -no-reboot
   -qmp unix:${CMAKE_CURRENT_BINARY_DIR}/chickenos-mlibc-crt-${state}.sock,server,nowait
  DEPENDS ${out}.elf USES_TERMINAL VERBATIM)
 add_dependencies(check check_crt_${state})
endforeach()
