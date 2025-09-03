#pragma once
#include "typedef.h"
#include <cassert>
#include <type_traits>
#include <cstring>

class Arena;

struct ArenaMarker {
	u8* position;
	const Arena* const pArena;

	ArenaMarker() : position(nullptr), pArena(nullptr) {}
	ArenaMarker(const Arena* arena, u8* pos) : position(pos), pArena(arena) {}
};

class Arena {
private:
	u8* data;
	u8* current;
	u8* end;
	size_t capacity;
	size_t size;
	const char* name;

public:
	Arena() : data(nullptr), current(nullptr), end(nullptr), capacity(0), size(0), name("Unnamed") {}

	void Init(void* memory, size_t totalSize, const char* arenaName) {
		data = static_cast<u8*>(memory);
		current = data;
		end = data + totalSize;
		capacity = totalSize;
		size = 0;
		name = arenaName;
	}

	void* Push(size_t bytes, size_t alignment = sizeof(void*)) {
		uintptr_t aligned = (reinterpret_cast<uintptr_t>(current) + alignment - 1) & ~(alignment - 1);
		u8* alignedPtr = reinterpret_cast<u8*>(aligned);

		if (alignedPtr + bytes > end) {
			assert(false && "Arena overflow");
			return nullptr;
		}

		void* pResult = alignedPtr;
		current = alignedPtr + bytes;
		size = current - data;

		return pResult;
	}

	template <typename T, typename... Args>
	T* Push(Args&&... args) {
		void* pResult = Push(sizeof(T), alignof(T));
		if (pResult == nullptr) {
			return nullptr;
		}
		return new (pResult) T(args...);
	}

	// NOTE: For non-trivial types, ensure to call the destructor manually if needed.
	void Pop(size_t bytes) {
		if (bytes > size) {
			assert(false && "Arena underflow");
			return;
		}
		current -= bytes;
		size = current - data;
	}

	ArenaMarker GetMarker() const {
		return ArenaMarker(this, current);
	}

	ArenaMarker GetBaseMarker() const {
		return ArenaMarker(this, data);
	}

	size_t PopToMarker(const ArenaMarker& marker) {
		assert(marker.pArena == this && "Invalid arena marker");
		assert(marker.position >= data && marker.position <= end && "Marker position out of bounds");

		size_t bytesToPop = current - marker.position;
		current = marker.position;
		size = current - data;
		return bytesToPop;
	}

	void Clear() {
		current = data;
		size = 0;
	}

	size_t GetRemainingBytes() const {
		return end - current;
	}

	size_t GetRemainingBytes(const ArenaMarker& marker) const {
		assert(marker.pArena == this && "Invalid arena marker");
		assert(marker.position >= data && marker.position <= end && "Marker position out of bounds");

		return end - marker.position;
	}

	size_t Size() const {
		return size;
	}

	const char* Name() const {
		return name;
	}
};

enum ArenaType {
	ARENA_PERMANENT,
	ARENA_ASSETS,
	ARENA_SCRATCH,
	// ARENA_PER_FRAME, // Uncomment if needed for per-frame allocations

	ARENA_COUNT
};

namespace ArenaAllocator {
	void Init();
	void Free();

	Arena* GetArena(ArenaType type);

	void* Push(ArenaType type, size_t bytes, size_t alignment = sizeof(void*));
	template <typename T, typename... Args>
	T* Push(ArenaType type, Args&&... args) {
		static_assert(std::is_trivially_destructible<T>::value, "Type must be trivially destructible");
		void* pResult = Push(type, sizeof(T), alignof(T));
		if (pResult == nullptr) {
			return nullptr;
		}
		return new (pResult) T(args...);
	}
	template<typename T>
	T* PushArray(ArenaType type, size_t count) {
		static_assert(std::is_trivially_destructible<T>::value, "Type must be trivially destructible");
		void* pResult = Push(type, sizeof(T) * count, alignof(T));
		if (pResult == nullptr) {
			return nullptr;
		}
		T* arr = static_cast<T*>(pResult);
		if constexpr (std::is_trivial<T>::value) {
			memset(arr, 0, sizeof(T) * count); // Zero-initialize for trivial types
		}
		else {
			for (size_t i = 0; i < count; ++i) {
				new (&arr[i]) T(); // Placement new for default initialization
			}
		}
		return arr;
	}

	ArenaMarker GetMarker(ArenaType type);
	ArenaMarker GetBaseMarker(ArenaType type);
	void Pop(ArenaType type, size_t bytes);
	void PopToMarker(ArenaType type, const ArenaMarker& marker);
	void Clear(ArenaType type);
	bool Copy(const ArenaMarker& dstMarker, const ArenaMarker& srcMarker, size_t bytes);
}