cmake_minimum_required(VERSION 3.25)
project(mlibc_arm32_jump NONE)
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
