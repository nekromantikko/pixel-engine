#pragma once
#include "typedef.h"
#include <cassert>

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

	void Clear() {
		current = data;
		size = 0;
	}

	size_t GetRemainingBytes() const {
		return end - current;
	}

	size_t Size() const {
		return size;
	}

	const char* Name() const {
		return name;
	}
};

enum ArenaType {
	ARENA_SCRATCH,

	ARENA_COUNT
};

namespace ArenaAllocator {
	void Init();
	void Free();

	Arena* GetArena(ArenaType type);

	void* Push(ArenaType type, size_t bytes, size_t alignment = sizeof(void*));
	template <typename T, typename... Args>
	T* Push(ArenaType type, Args&&... args) {
		void* pResult = Push(type, sizeof(T), alignof(T));
		if (pResult == nullptr) {
			return nullptr;
		}
		return new (pResult) T(args...);
	}
	void Clear(ArenaType type);
}