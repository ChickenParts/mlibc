#include <limits.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>

cpu_set_t *__mlibc_cpu_alloc(int num_cpus) {
	return reinterpret_cast<cpu_set_t *>(calloc(1, CPU_ALLOC_SIZE(num_cpus)));
}

#define CPU_MASK_BITS (CHAR_BIT * sizeof(__cpu_mask))

size_t __mlibc_cpu_alloc_size(int num_cpus) {
	/* calculate the (unaligned) remainder that doesn't neatly fit in one __cpu_mask; 0 or 1 */
	size_t remainder = ((num_cpus % CPU_MASK_BITS) + CPU_MASK_BITS - 1) / CPU_MASK_BITS;
	return sizeof(__cpu_mask) * (num_cpus / CPU_MASK_BITS + remainder);
}

void __mlibc_cpu_zero(const size_t setsize, cpu_set_t *set) {
	memset(set, 0, setsize);
}

void __mlibc_cpu_set(const int cpu, const size_t setsize, cpu_set_t *set) {
	if(cpu < 0) {
		return;
	}

	size_t word_index = static_cast<unsigned>(cpu) / CPU_MASK_BITS;
	if(word_index >= setsize / sizeof(__cpu_mask))
		return;

	__cpu_mask *words = reinterpret_cast<__cpu_mask *>(set);
	unsigned bit_index = static_cast<unsigned>(cpu) % CPU_MASK_BITS;
	words[word_index] |= static_cast<__cpu_mask>(1) << bit_index;
}

void __mlibc_cpu_clear(const int cpu, const size_t setsize, cpu_set_t *set) {
	if(cpu < 0) {
		return;
	}

	size_t word_index = static_cast<unsigned>(cpu) / CPU_MASK_BITS;
	if(word_index >= setsize / sizeof(__cpu_mask))
		return;

	__cpu_mask *words = reinterpret_cast<__cpu_mask *>(set);
	unsigned bit_index = static_cast<unsigned>(cpu) % CPU_MASK_BITS;
	words[word_index] &= ~(static_cast<__cpu_mask>(1) << bit_index);
}


int __mlibc_cpu_isset(const int cpu, const size_t setsize, const cpu_set_t *set) {
	if(cpu < 0) {
		return false;
	}

	size_t word_index = static_cast<unsigned>(cpu) / CPU_MASK_BITS;
	if(word_index >= setsize / sizeof(__cpu_mask))
		return false;

	const __cpu_mask *words = reinterpret_cast<const __cpu_mask *>(set);
	unsigned bit_index = static_cast<unsigned>(cpu) % CPU_MASK_BITS;
	return (words[word_index] & (static_cast<__cpu_mask>(1) << bit_index)) != 0;
}

int __mlibc_cpu_count(const size_t setsize, const cpu_set_t *set) {
	size_t count = 0;
	const unsigned char *ptr = reinterpret_cast<const unsigned char *>(set);

	for(size_t i = 0; i < setsize; i++) {
		for(size_t bit = 0; bit < CHAR_BIT; bit++) {
			if((1 << bit) & ptr[i])
				count++;
		}
	}

	return count;
}
