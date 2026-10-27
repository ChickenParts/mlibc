#include "mlibc-asm/helpers.h"

PROC_START(_start)
	// Preserve the kernel's initial stack across C calls and provide the
	// caller linkage area required by the PPC32 SysV ABI.
	mr 31, 1
	stwu 1, -32(1)
	bl .Lbootstrap_pc
.Lbootstrap_pc:
	mflr 3
	mr 4, 3
	addis 3, 3, (__ehdr_start-.Lbootstrap_pc)@ha
	addi 3, 3, (__ehdr_start-.Lbootstrap_pc)@l
	addis 4, 4, (_DYNAMIC-.Lbootstrap_pc)@ha
	addi 4, 4, (_DYNAMIC-.Lbootstrap_pc)@l
	bl relocateSelf
	mr 3, 31
	bl interpreterMain
	mtctr 3
	mr 1, 31
	bctr
PROC_END(_start)

.hidden __ehdr_start
.hidden _DYNAMIC
.hidden relocateSelf
.hidden interpreterMain

GNU_STACK_NOTE()
