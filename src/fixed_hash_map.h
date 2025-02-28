#pragma once
#include "typedef.h"
#include "random.h"

static constexpr u32 HASH_MAP_SIZE = 65536;
static constexpr u32 HASH_MAP_SIZE_MASK = HASH_MAP_SIZE - 1;
static constexpr u32 HASH_MAP_SIZE_BITS = 16;
static constexpr u32 HASH_MAP_SHIFT_BITS = 64 - HASH_MAP_SIZE_BITS;

template <typename T>
class FixedHashMapBucket {
public:
	u64 key = UUID_NULL;
	T value;
};

template <typename T, typename TBucket = FixedHashMapBucket<T>>
class FixedHashMap {
private:
	TBucket data[HASH_MAP_SIZE];

	u32 Hash(u64 key) const {
		// Thank you Donald Knuth
		key ^= key >> HASH_MAP_SHIFT_BITS;
		return (key * 11400714819323198485ULL) >> HASH_MAP_SHIFT_BITS;
	}
	u32 Rehash(u32 hash) const {
		return (hash + 1) & HASH_MAP_SIZE_MASK;
	}
	bool Probe(u64 key, u32& hash) const {
		u32 count = 0;
		hash = Hash(key);

		while (count < HASH_MAP_SIZE) {
			if (data[hash].key == UUID_NULL) {
				return false;  // Empty slot found
			}
			if (data[hash].key == key) {
				return true;  // Key found
			}
			hash = Rehash(hash);
			count++;
		}

		return false;  // Full map, key not found
	}
	bool GetHash(u64 key, u32& hash) const {
		if (key == UUID_NULL) {
			return false;
		}

		hash = Hash(key);

		if (Probe(key, hash)) {
			return true;
		}

		return false;
	}
public:
	bool Add(u64 key, const T& value) {
		if (key == UUID_NULL) {
			return false;
		}

		u32 hash = Hash(key);

		if (Probe(key, hash)) {
			return false;
		}

		// Whole map is full
		if (data[hash].key != UUID_NULL) {
			return false;
		}

		data[hash] = {
			.key = key,
			.value = value
		};

		return true;
	}
	T* Get(u64 key) {
		u32 hash;
		if (GetHash(key, hash)) {
			return &data[hash].value;
		}

		return nullptr;
	}
	const T* Get(u64 key) const {
		u32 hash;
		if (GetHash(key, hash)) {
			return &data[hash].value;
		}

		return nullptr;
	}
	bool Remove(u64 key) {
		if (key == UUID_NULL) {
			return false;
		}

		u32 hash = Hash(key);

		if (Probe(key, hash)) {
			data[hash] = {
			.key = UUID_NULL,
			.value = T{}
			};
			return true;
		}

		return false;
	}
	void Clear() {
		for (u32 i = 0; i < HASH_MAP_SIZE; i++) {
			data[i].key = UUID_NULL;
		}
	}
	void ForEach(void (*callback) (u64, T&)) {
		if (callback == nullptr) {
			return;
		}

		for (u32 i = 0; i < HASH_MAP_SIZE; i++) {
			TBucket& bucket = data[i];
			if (bucket.key != UUID_NULL) {
				callback(bucket.key, bucket.value);
			}
		}
	}

	FixedHashMap() {
		Clear();
	}
};