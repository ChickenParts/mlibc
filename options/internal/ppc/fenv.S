/* The ChickenOS PPC32 ABI is built soft-float and exposes no FP environment. */
#include "mlibc-asm/helpers.h"

.global feclearexcept
.type feclearexcept, @function
feclearexcept:
	li 3, 0
	blr

.global feraiseexcept
.type feraiseexcept, @function
feraiseexcept:
	li 3, 0
	blr

.global __fesetround
.hidden __fesetround
.type __fesetround, @function
__fesetround:
	li 3, 0
	blr

.global fegetround
.type fegetround, @function
fegetround:
	li 3, 0
	blr

.global fegetenv
.type fegetenv, @function
fegetenv:
	li 4, 0
	stw 4, 0(3)
	li 3, 0
	blr

.global fesetenv
.type fesetenv, @function
fesetenv:
	li 3, 0
	blr

.global fetestexcept
.type fetestexcept, @function
fetestexcept:
	li 3, 0
	blr

GNU_STACK_NOTE()
