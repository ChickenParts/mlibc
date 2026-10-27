/* PPC32 uses eager Secure PLT binding. Lazy resolution is unsupported. */
.global pltRelocateStub
.type pltRelocateStub, @function
pltRelocateStub:
	tw 31, 0, 0

.section .note.GNU-stack,"",%progbits
