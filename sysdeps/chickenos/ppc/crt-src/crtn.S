.section .init,"ax",@progbits
	lwz 0,20(1)
	mtlr 0
	addi 1,1,16
	blr
.size _init, .-_init

.section .fini,"ax",@progbits
	lwz 0,20(1)
	mtlr 0
	addi 1,1,16
	blr
.size _fini, .-_fini

.section .note.GNU-stack,"",@progbits
