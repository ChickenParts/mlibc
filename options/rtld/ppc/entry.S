#include "mlibc-asm/helpers.h"

PROC_START(_start)
	bl relocateSelf
	mr 3, 1
	bl interpreterMain
	mtctr 3
	bctr
PROC_END(_start)

GNU_STACK_NOTE()
