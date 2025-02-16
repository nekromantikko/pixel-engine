#pragma once
#include "typedef.h"
#include "system.h"

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

template<typename T, typename THandle = PoolHandle<T>>
class Pool
{
private:
	T* objs;
	THandle* handles;
	u32* erase;

	u32 size;
	u32 count;
public:
	Pool() {
		Init(0);
	}
	Pool(u32 s) {
		Init(s);
	}
	void Free() {
		delete[] objs;
		delete[] handles;
		delete[] erase;
	}
	~Pool() {
		Free();
	}
	void Init(u32 s) {
		Free();

		size = s;
		count = 0;

		objs = new T[size]{};
		handles = new THandle[size]{};
		erase = new u32[size]{};

		for (u32 i = 0; i < size; i++)
		{
			handles[i] = THandle(i, 1);
			erase[i] = i;
		}
	}
	T* Get(const THandle handle) const {
		if (handle == THandle::Null()) {
			return nullptr;
		}

		const u32 arrayIndex = handle.Index();
		const u32 handleIndex = erase[arrayIndex];
		if (handleIndex >= count) {
			return nullptr;
		}

		const THandle h = handles[handleIndex];
		if (h != handle) {
			return nullptr;
		}
		return &objs[arrayIndex];
	}
	T* operator[](THandle handle) {
		return Get(handle);
	}
	THandle Add() {
		if (count >= size) {
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
		THandle h = handles[handleIndex];
		if (h != handle) {
			return false;
		}
		handles[handleIndex] = handles[--count];
		handles[count] = THandle(arrayIndex, h.Generation() + 1);

		const u32 swapIndex = handles[handleIndex].Index();
		const u32 swap = erase[arrayIndex];
		erase[arrayIndex] = erase[swapIndex];
		erase[swapIndex] = swap;
		return true;
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
};