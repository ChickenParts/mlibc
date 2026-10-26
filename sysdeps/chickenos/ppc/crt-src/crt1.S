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
	lis 4,main@ha
	addi 4,4,main@l
	bl __mlibc_entry
	trap
.size _start, .-_start

.section .note.GNU-stack,"",@progbits
