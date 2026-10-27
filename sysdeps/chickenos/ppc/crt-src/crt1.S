.section .text
.global _start
.type _start,@function
_start:
	li 0,0
	li 31,0
	mr 3,1
	/* Keep the argc/argv/envp/auxv block untouched: a normal PPC caller frame
	 * is required so __mlibc_entry's LR save cannot land on argv[0]. The kernel
	 * enters with r1 16-byte aligned; this preserves that alignment. */
	stwu 1,-16(1)
	bl .Lentry_pc
.Lentry_pc:
	mflr 4
	addis 5,4,(.Lentry_pointer-.Lentry_pc)@ha
	lwz 12,(.Lentry_pointer-.Lentry_pc)@l(5)
	addis 4,4,(main-.Lentry_pc)@ha
	addi 4,4,(main-.Lentry_pc)@l
	/* The interpreter resolves this data pointer before entering _start.
	 * Avoid a PLT stub here: no application GOT register is established yet. */
	mtctr 12
	bctrl
	trap
.size _start, .-_start

.section .data
.balign 4
.Lentry_pointer:
	.long __mlibc_entry

.section .note.GNU-stack,"",@progbits
