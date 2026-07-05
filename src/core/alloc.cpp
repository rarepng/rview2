#include <rpmalloc.h>
#include <new>
#include <cstddef>
#include <rpmalloc.h>
#include <new>
#include <cstddef>
#include <atomic>

static std::atomic<int> g_rpmalloc_state{0};
thread_local bool t_rpmalloc_thread_init = false;

inline void ensure_rpmalloc() {
	if (!t_rpmalloc_thread_init) [[unlikely]] {
		int expected = 0;

		if (g_rpmalloc_state.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
			rpmalloc_initialize();
			g_rpmalloc_state.store(2, std::memory_order_release);
		} else {
			while (g_rpmalloc_state.load(std::memory_order_acquire) != 2) {}
		}

		rpmalloc_thread_initialize();
		t_rpmalloc_thread_init = true;
	}
}

void* operator new(std::size_t size) {
	ensure_rpmalloc();
	return rpmalloc(size);
}
void* operator new[](std::size_t size) {
	ensure_rpmalloc();
	return rpmalloc(size);
}
void* operator new(std::size_t size, std::align_val_t al) {
	ensure_rpmalloc();
	return rpaligned_alloc(static_cast<std::size_t>(al), size);
}
void* operator new[](std::size_t size, std::align_val_t al) {
	ensure_rpmalloc();
	return rpaligned_alloc(static_cast<std::size_t>(al), size);
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
	ensure_rpmalloc();
	return rpmalloc(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
	ensure_rpmalloc();
	return rpmalloc(size);
}
void* operator new(std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept {
	ensure_rpmalloc();
	return rpaligned_alloc(static_cast<std::size_t>(al), size);
}
void* operator new[](std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept {
	ensure_rpmalloc();
	return rpaligned_alloc(static_cast<std::size_t>(al), size);
}

void operator delete(void* p) noexcept {
	if (p) {
		ensure_rpmalloc();
		rpfree(p);
	}
}
void operator delete[](void* p) noexcept {
	if (p) {
		ensure_rpmalloc();
		rpfree(p);
	}
}
void operator delete(void* p, std::size_t) noexcept {
	if (p) {
		ensure_rpmalloc();
		rpfree(p);
	}
}
void operator delete[](void* p, std::size_t) noexcept {
	if (p) {
		ensure_rpmalloc();
		rpfree(p);
	}
}
void operator delete(void* p, std::align_val_t) noexcept {
	if (p) {
		ensure_rpmalloc();
		rpfree(p);
	}
}
void operator delete[](void* p, std::align_val_t) noexcept {
	if (p) {
		ensure_rpmalloc();
		rpfree(p);
	}
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
	if (p) {
		ensure_rpmalloc();
		rpfree(p);
	}
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
	if (p) {
		ensure_rpmalloc();
		rpfree(p);
	}
}