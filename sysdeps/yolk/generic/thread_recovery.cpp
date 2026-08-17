/* SPDX-License-Identifier: MIT */
#include <mlibc/all-sysdeps.hpp>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <yolk/syscall.h>

namespace mlibc {

static constexpr size_t default_stack_size = 0x200000u;
static constexpr size_t page_size = 0x1000u;

extern "C" long __mlibc_clone_start(unsigned long flags, void *stack,
                                     pid_t *parent_tid, pid_t *child_tid,
                                     void *tcb);

[[noreturn]] void sys_thread_exit()
{
	(void)__syscall1(SYS_exit, 0);
	__builtin_unreachable();
}

int sys_clone(void *tcb, pid_t *tid_out, void *stack)
{
	if (!tcb || !tid_out || !stack) return EINVAL;
	constexpr unsigned long flags =
		0x00000100u | /* CLONE_VM */
		0x00000200u | /* CLONE_FS */
		0x00000400u | /* CLONE_FILES */
		0x00000800u | /* CLONE_SIGHAND */
		0x00010000u | /* CLONE_THREAD */
		0x00080000u | /* CLONE_SETTLS */
		0x00100000u | /* CLONE_PARENT_SETTID */
		0x00200000u;  /* CLONE_CHILD_CLEARTID */

	long result = __mlibc_clone_start(flags, stack, tid_out, tid_out, tcb);
	if (result < 0) return (int)-result;
	*tid_out = (pid_t)result;
	return 0;
}

int sys_prepare_stack(void **stack, void *entry, void *user_arg, void *tcb,
                      size_t *stack_size, size_t *guard_size,
                      void **stack_base)
{
	if (!stack || !entry || !tcb || !stack_size || !guard_size || !stack_base)
		return EINVAL;
	if (!*stack_size) *stack_size = default_stack_size;
	if (*stack_size < 4u * sizeof(void *)) return EINVAL;

	if (!*stack) {
		*guard_size = page_size;
		if (*stack_size > (size_t)-1 - *guard_size) return EOVERFLOW;
		size_t total = *stack_size + *guard_size;
		void *mapping = nullptr;
		int error = sys_vm_map(nullptr, total, PROT_READ | PROT_WRITE,
		                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, &mapping);
		if (error) return error;
		error = sys_vm_protect(mapping, *guard_size, PROT_NONE);
		if (error) {
			(void)sys_vm_unmap(mapping, total);
			return error;
		}
		*stack_base = static_cast<unsigned char *>(mapping) + *guard_size;
	} else {
		*guard_size = 0;
		*stack_base = *stack;
	}

	uintptr_t base = (uintptr_t)*stack_base;
	if (base > UINTPTR_MAX - *stack_size) return EOVERFLOW;
	uintptr_t sp = (base + *stack_size) & ~(uintptr_t)0x0fu;
	if (sp < base + 4u * sizeof(void *)) return EINVAL;

	sp -= sizeof(void *); *reinterpret_cast<void **>(sp) = tcb;
	sp -= sizeof(void *); *reinterpret_cast<void **>(sp) = user_arg;
	sp -= sizeof(void *); *reinterpret_cast<void **>(sp) = entry;
	sp -= sizeof(void *); *reinterpret_cast<void **>(sp) = nullptr;
	*stack = reinterpret_cast<void *>(sp);
	return 0;
}

} // namespace mlibc
