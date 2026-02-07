/*
 * Yolk RTLD symbols for x86_64
 * SPDX-License-Identifier: MIT
 *
 * Currently using static linking only to avoid binutils 2.43+ _DYNAMIC issues.
 * This file just ensures proper .note.GNU-stack section.
 */

.section .note.GNU-stack, "", @progbits
