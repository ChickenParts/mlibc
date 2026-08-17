/* SPDX-License-Identifier: MIT */
.section .text
.global __mlibc_signal_restore
.type __mlibc_signal_restore, @function
__mlibc_signal_restore:
	li a7, 55
	ecall
	ebreak
.size __mlibc_signal_restore, . - __mlibc_signal_restore

.global __mlibc_signal_restore_rt
.type __mlibc_signal_restore_rt, @function
__mlibc_signal_restore_rt:
	li a7, 55
	ecall
	ebreak
.size __mlibc_signal_restore_rt, . - __mlibc_signal_restore_rt
.section .note.GNU-stack,"",@progbits
