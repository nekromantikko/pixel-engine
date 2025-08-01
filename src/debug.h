#pragma once

#define DEBUG_LOG(fmt, ...) Debug::Log("%s: " fmt, \
    __func__ __VA_OPT__(,) __VA_ARGS__)

#define DEBUG_ERROR(fmt, ...) Debug::Log("[error] %s: " fmt, \
	__func__ __VA_OPT__(,) __VA_ARGS__)

namespace Debug {
	void Log(const char* fmt, ...);
}

#ifndef NDEBUG
#include <cstdlib>
#include <stddef.h>
#include <type_traits>

// Allocator that prints to console when allocating / deallocating
template <typename T>
struct DebugAllocator {
	using value_type = T;
	using pointer = T*;
	using const_pointer = const T*;
	using reference = T&;
	using const_reference = const T&;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	template <typename U>
	struct rebind {
		using other = DebugAllocator<U>;
	};

	DebugAllocator() noexcept{}
	template <typename U>
	DebugAllocator(const DebugAllocator<U>&) noexcept {}

	pointer allocate(size_type count, const void* hint = nullptr) {
		const size_type byteSize = count * sizeof(value_type);
		DEBUG_LOG("(DebugAllocator) allocating %d element(s) (%d bytes)\n", count, byteSize);
		return (pointer)malloc(byteSize);
	}

	void deallocate(pointer ptr, size_type count) {
		const size_type byteSize = count * sizeof(value_type);
		DEBUG_LOG("(DebugAllocator) deallocating %d element(s) (%d bytes)\n", count, byteSize);
		free(ptr);
	}

	template <typename U>
	bool operator==(const DebugAllocator<U>&) const noexcept {
		return true;
	}

	template <typename U>
	bool operator!=(const DebugAllocator<U>& other) const noexcept {
		return !(*this == other);
	}

	using propagate_on_container_copy_assignment = std::true_type;
	using propagate_on_container_move_assignment = std::true_type;
	using propagate_on_container_swap = std::true_type;
	using is_always_equal = std::true_type;
};

#endif