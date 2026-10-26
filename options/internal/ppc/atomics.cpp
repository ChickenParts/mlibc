#include <frg/hash.hpp>
#include <mlibc/lock.hpp>
#include <stdbool.h>
#include <stdint.h>

namespace {

constexpr size_t num_locks = 32;
constinit FutexLock atomic_locks[num_locks];

inline FutexLock &get_lock(const volatile void *ptr) {
	auto hash = frg::mix_bits(reinterpret_cast<uintptr_t>(ptr));
	return atomic_locks[hash & (num_locks - 1)];
}

using aliased_uint64_t [[gnu::may_alias]] = uint64_t;

inline volatile aliased_uint64_t *access(volatile void *ptr) {
	return reinterpret_cast<volatile aliased_uint64_t *>(ptr);
}

inline const volatile aliased_uint64_t *access(const volatile void *ptr) {
	return reinterpret_cast<const volatile aliased_uint64_t *>(ptr);
}

} // namespace

extern "C" {

uint64_t mlibc_atomic_load_8(const volatile void *ptr, int model)
		asm("__atomic_load_8");
void mlibc_atomic_store_8(volatile void *ptr, uint64_t val, int model)
		asm("__atomic_store_8");
uint64_t mlibc_atomic_exchange_8(volatile void *ptr, uint64_t val, int model)
		asm("__atomic_exchange_8");
bool mlibc_atomic_compare_exchange_8(volatile void *ptr, void *expected,
		uint64_t desired, int success_model, int failure_model)
		asm("__atomic_compare_exchange_8");
uint64_t mlibc_atomic_fetch_add_8(volatile void *ptr, uint64_t val, int model)
		asm("__atomic_fetch_add_8");

uint64_t mlibc_atomic_load_8(const volatile void *ptr, int model [[maybe_unused]]) {
	auto &lock = get_lock(ptr);
	lock.lock();
	auto value = *access(ptr);
	lock.unlock();
	return value;
}

void mlibc_atomic_store_8(volatile void *ptr, uint64_t value,
		int model [[maybe_unused]]) {
	auto &lock = get_lock(ptr);
	lock.lock();
	*access(ptr) = value;
	lock.unlock();
}

uint64_t mlibc_atomic_exchange_8(volatile void *ptr, uint64_t value,
		int model [[maybe_unused]]) {
	auto &lock = get_lock(ptr);
	lock.lock();
	uint64_t old = *access(ptr);
	*access(ptr) = value;
	lock.unlock();
	return old;
}

bool mlibc_atomic_compare_exchange_8(volatile void *ptr, void *expected,
		uint64_t desired, int success_model [[maybe_unused]],
		int failure_model [[maybe_unused]]) {
	auto &lock = get_lock(ptr);
	bool result = false;
	lock.lock();
	uint64_t old = *access(ptr);
	if(old == *access(expected)) {
		*access(ptr) = desired;
		result = true;
	} else {
		*access(expected) = old;
	}
	lock.unlock();
	return result;
}

uint64_t mlibc_atomic_fetch_add_8(volatile void *ptr, uint64_t value,
		int model [[maybe_unused]]) {
	auto &lock = get_lock(ptr);
	lock.lock();
	uint64_t old = *access(ptr);
	*access(ptr) = old + value;
	lock.unlock();
	return old;
}

}
