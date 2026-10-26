#include <assert.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>

#define SET_SIZE 15

int main() {
	cpu_set_t *set = CPU_ALLOC(SET_SIZE);
	size_t setsize = CPU_ALLOC_SIZE(SET_SIZE);

	CPU_ZERO_S(setsize, set);

	assert(!CPU_ISSET_S(11, setsize, set));

	CPU_SET_S(11, setsize, set);
	assert(CPU_ISSET_S(11, setsize, set));
	assert(CPU_COUNT_S(setsize, set) == 1);
	unsigned long native_word = 0;
	memcpy(&native_word, set, sizeof(native_word));
	assert(native_word == (1UL << 11));

	assert(!CPU_ISSET_S(CPU_SETSIZE - 1, setsize, set));

	CPU_CLR_S(11, setsize, set);
	assert(!CPU_ISSET_S(11, setsize, set));

	/* CPU masks are arrays of native unsigned-long words. In particular,
	 * CPU 0 must be bit 0 of the first word on both endian variants. */
	CPU_SET_S(0, setsize, set);
	memcpy(&native_word, set, sizeof(native_word));
	assert(native_word == 1UL);
	assert(CPU_ISSET_S(0, setsize, set));
	CPU_CLR_S(0, setsize, set);
	assert(!CPU_ISSET_S(0, setsize, set));

	/* Negative CPU indices and a size that cannot contain a whole mask word
	 * must not access or alter the set. */
	CPU_SET_S(-1, setsize, set);
	CPU_CLR_S(-1, setsize, set);
	assert(!CPU_ISSET_S(-1, setsize, set));
	CPU_SET_S(0, sizeof(unsigned long) - 1, set);
	assert(!CPU_ISSET_S(0, sizeof(unsigned long) - 1, set));
	memcpy(&native_word, set, sizeof(native_word));
	assert(native_word == 0);

	CPU_FREE(set);

	// Regression test: CPU_ZERO_S should clear the entire allocated size.
	int large_num_cpus = 1024;
	cpu_set_t *large_set = CPU_ALLOC(large_num_cpus);
	size_t large_setsize = CPU_ALLOC_SIZE(large_num_cpus);

	memset(large_set, 0xFF, large_setsize);
	CPU_ZERO_S(large_setsize, large_set);

	unsigned char *ptr = (unsigned char *)large_set;
	for (size_t i = 0; i < large_setsize; i++) {
		assert(ptr[i] == 0);
	}

	CPU_FREE(large_set);

	return 0;
}
