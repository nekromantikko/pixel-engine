#pragma once
#include "typedef.h"
#include <algorithm>

template <typename T>
class PoolHandle {
	u64 raw;
public:
	PoolHandle() = default;
	PoolHandle(const PoolHandle& other) = default;
	PoolHandle(u64 raw) : raw(raw) {}
	PoolHandle(u32 index, u32 gen) {
		raw = ((u64)gen << 32ull) | (u64)index;
	}

	inline u32 Index() const {
		return (u32)(raw % 0x100000000);
	}

	inline u32 Generation() const {
		return (u32)(raw >> 32ull);
	}

	inline const u64 Raw() const {
		return raw;
	}

	bool operator==(const PoolHandle& other) const {
		return raw == other.raw;
	}
	bool operator!=(const PoolHandle& other) const {
		return raw != other.raw;
	}

	inline static PoolHandle Null() {
		return PoolHandle(0);
	}
};

template<typename T, u32 capacity, typename THandle = PoolHandle<T>>
class Pool
{
private:
	T objs[capacity];
	THandle handles[capacity];
	u32 erase[capacity];

	u32 count;

	bool GetArrayIndex(const THandle handle, u32& index) const {
		if (handle == THandle::Null()) {
			return false;
		}

		const u32 arrayIndex = handle.Index();
		const u32 handleIndex = erase[arrayIndex];
		if (handleIndex >= count) {
			return false;
		}

		const THandle h = handles[handleIndex];
		if (h != handle) {
			return false;
		}

		index = arrayIndex;
		return true;
	}
public:
	Pool() {
		count = 0;

		for (u32 i = 0; i < capacity; i++)
		{
			handles[i] = THandle(i, 1);
			erase[i] = i;
		}
	}
	Pool(const Pool& other) {
		count = other.count;

		std::copy(other.objs, other.objs + capacity, objs);
		std::copy(other.handles, other.handles + capacity, handles);
		std::copy(other.erase, other.erase + capacity, erase);
	}
	Pool(Pool&& other) noexcept {
		count = other.count;

		std::copy(other.objs, other.objs + capacity, objs);
		std::copy(other.handles, other.handles + capacity, handles);
		std::copy(other.erase, other.erase + capacity, erase);

		other.Clear();
	}
	T* Get(const THandle handle) {
		u32 arrayIndex;
		if (!GetArrayIndex(handle, arrayIndex)) {
			return nullptr;
		}

		return &objs[arrayIndex];
	}
	const T* Get(const THandle handle) const {
		u32 arrayIndex;
		if (!GetArrayIndex(handle, arrayIndex)) {
			return nullptr;
		}

		return &objs[arrayIndex];
	}
	T* operator[](THandle handle) {
		return Get(handle);
	}
	THandle Add() {
		if (count >= capacity) {
			return THandle::Null();
		}
		THandle handle = handles[count++];
		return handle;
	}
	THandle Add(const T& proto) {
		THandle handle = Add();
		if (handle != THandle::Null()) {
			*(*this)[handle] = proto;
		}
		return handle;
	}
    bool Remove(const THandle handle) {
		const u32 arrayIndex = handle.Index();
		const u32 handleIndex = erase[arrayIndex];
		const THandle h = handles[handleIndex];

		if (h != handle) {
			return false;
		}

		// Move the last handle to the current position
		handles[handleIndex] = handles[--count];

		// Update the generation of the moved handle
		handles[count] = THandle(arrayIndex, h.Generation() + 1);

		// Update the erase array
		const u32 swapIndex = handles[handleIndex].Index();
		erase[arrayIndex] = erase[swapIndex];
		erase[swapIndex] = handleIndex;

		return true;
    }
	bool Contains(const THandle handle) const {
		u32 index;
		return GetArrayIndex(handle, index);
	}
	u32 Count() const {
		return count;
	}
	THandle GetHandle(u32 index) const {
		if (index >= count) {
			return THandle::Null();
		}
		return handles[index];
	}
	void Clear() {
		while (count != 0) {
			THandle handle = GetHandle(0);
			Remove(handle);
		}
	}

	Pool& operator=(const Pool& other) {
		if (this != &other) {  // Prevent self-assignment
			count = other.count;

			std::copy(other.objs, other.objs + capacity, objs);
			std::copy(other.handles, other.handles + capacity, handles);
			std::copy(other.erase, other.erase + capacity, erase);
		}
		return *this;
	}

	Pool& operator=(Pool&& other) noexcept {
		if (this != &other) {
			count = other.count;

			std::copy(other.objs, other.objs + capacity, objs);
			std::copy(other.handles, other.handles + capacity, handles);
			std::copy(other.erase, other.erase + capacity, erase);

			// Clear source
			other.Clear();
		}
		return *this;
	}

	// Sort the pool contents using quicksort with a comparison function
	// The comparison function should return true if the first argument should come before the second
	// 
	// Example usage:
	//   pool.Sort([](const MyType& a, const MyType& b) { return a.value < b.value; }); // ascending
	//   pool.Sort([](const MyType& a, const MyType& b) { return a.value > b.value; }); // descending
	//   pool.Sort(myCompareFunction); // function pointer
	//
	// Example with Actors (sort by position for rendering order):
	//   actors.Sort([](const Actor& a, const Actor& b) { return a.position.y < b.position.y; });
	template<typename CompareFunc>
	void Sort(CompareFunc compare) {
		if (count <= 1) return;
		
		QuickSort(0, count - 1, compare);
	}

private:
	template<typename CompareFunc>
	void QuickSort(u32 low, u32 high, CompareFunc compare) {
		if (low < high) {
			u32 pivot = Partition(low, high, compare);
			if (pivot > low) QuickSort(low, pivot - 1, compare);
			if (pivot < high) QuickSort(pivot + 1, high, compare);
		}
	}

	template<typename CompareFunc>
	u32 Partition(u32 low, u32 high, CompareFunc compare) {
		// Use the object at the high position as pivot
		const T& pivot = objs[handles[high].Index()];
		u32 i = low;

		for (u32 j = low; j < high; j++) {
			const T& current = objs[handles[j].Index()];
			if (compare(current, pivot)) {
				SwapPoolElements(i, j);
				i++;
			}
		}
		SwapPoolElements(i, high);
		return i;
	}

	void SwapPoolElements(u32 a, u32 b) {
		if (a == b) return;

		// Swap handles
		THandle tempHandle = handles[a];
		handles[a] = handles[b];
		handles[b] = tempHandle;

		// Update erase mappings
		erase[handles[a].Index()] = a;
		erase[handles[b].Index()] = b;
	}
};