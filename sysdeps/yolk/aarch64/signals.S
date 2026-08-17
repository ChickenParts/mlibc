/* SPDX-License-Identifier: MIT */
.section .text
.global __mlibc_signal_restore
.type __mlibc_signal_restore, %function
__mlibc_signal_restore:
	mov x8, #55
	svc #0
	brk #0
.size __mlibc_signal_restore, . - __mlibc_signal_restore

.global __mlibc_signal_restore_rt
.type __mlibc_signal_restore_rt, %function
__mlibc_signal_restore_rt:
	mov x8, #55
	svc #0
	brk #0
.size __mlibc_signal_restore_rt, . - __mlibc_signal_restore_rt
.section .note.GNU-stack,"",%progbits
