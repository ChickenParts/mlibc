/* Dynamic PLT resolution is unsupported by the static-only PPC32 port. */
.global pltRelocateStub
.type pltRelocateStub, @function
pltRelocateStub:
	tw 31, 0, 0

.section .note.GNU-stack,"",%progbits
