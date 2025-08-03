#pragma once
#include "typedef.h"
#include "asset_types.h"
#include <filesystem>

constexpr u32 MAX_ASSET_NAME_LENGTH = 56;
constexpr u32 MAX_ASSET_PATH_LENGTH = 256;

struct AssetFlags {
	AssetType type : 4;
	bool deleted : 1;
	bool compressed : 1; // NOTE: For the future maybe?
};

struct AssetEntry {
	u64 id;
	char name[MAX_ASSET_NAME_LENGTH];
	char relativePath[MAX_ASSET_PATH_LENGTH];
	u32 offset;
	u32 size;
	AssetFlags flags;
};

// Simple sorted array for asset directory (Doom WAD style)
class AssetIndex {
private:
	AssetEntry* m_entries;
	u32 m_count;
	u32 m_capacity;

	s32 FindIndex(u64 id) const;
	bool ResizeIfNeeded();

public:
	AssetIndex();
	~AssetIndex();
	AssetIndex(const AssetIndex&) = delete;
	AssetIndex& operator=(const AssetIndex&) = delete;

	// Hash map compatible interface
	AssetEntry* find(u64 id);
	const AssetEntry* find(u64 id) const;
	bool emplace(u64 id, const AssetEntry& entry);
	bool erase(u64 id);
	void clear();
	void reserve(u32 capacity);
	u32 size() const { return m_count; }

	// Iterator support for range-based loops
	class iterator {
	public:
		AssetEntry* ptr;
		iterator(AssetEntry* p) : ptr(p) {}
		iterator& operator++() { ++ptr; return *this; }
		bool operator!=(const iterator& other) const { return ptr != other.ptr; }
		bool operator==(const iterator& other) const { return ptr == other.ptr; }
		
		// Provide std::pair-like interface for compatibility with existing code
		struct KeyValuePair {
			u64 first;
			AssetEntry& second;
			KeyValuePair(AssetEntry& entry) : first(entry.id), second(entry) {}
		};
		KeyValuePair operator*() const { return KeyValuePair(*ptr); }
		AssetEntry* operator->() { return ptr; }
	};

	iterator begin() { return iterator(m_entries); }
	iterator end() { return iterator(m_entries + m_count); }

	class const_iterator {
	public:
		const AssetEntry* ptr;
		const_iterator(const AssetEntry* p) : ptr(p) {}
		const_iterator& operator++() { ++ptr; return *this; }
		bool operator!=(const const_iterator& other) const { return ptr != other.ptr; }
		
		struct KeyValuePair {
			u64 first;
			const AssetEntry& second;
			KeyValuePair(const AssetEntry& entry) : first(entry.id), second(entry) {}
		};
		KeyValuePair operator*() const { return KeyValuePair(*ptr); }
		const AssetEntry* operator->() const { return ptr; }
	};

	const_iterator begin() const { return const_iterator(m_entries); }
	const_iterator end() const { return const_iterator(m_entries + m_count); }
};

// Self-contained asset archive class without dependencies on debug or random
class AssetArchive {
public:
	AssetArchive();
	~AssetArchive();

	// Archive file operations
	bool LoadFromFile(const std::filesystem::path& path);
	bool SaveToFile(const std::filesystem::path& path);
	bool CreateEmpty();

	// Asset operations
	void* AddAsset(u64 id, AssetType type, u32 size, const char* path, const char* name, const void* data = nullptr);
	bool RemoveAsset(u64 id);
	bool ResizeAsset(u64 id, u32 newSize);
	
	// Asset retrieval
	void* GetAssetData(u64 id, AssetType type);
	AssetEntry* GetAssetEntry(u64 id);
	
	// Archive management
	void Repack();
	void Clear();
	
	// Statistics
	u32 GetAssetCount() const;
	const AssetIndex& GetIndex() const;

private:
	struct ArchiveHeader {
		char signature[4];
		u32 assetCount;
		u32 directoryOffset;
	};

	u32 m_capacity;
	u32 m_size;
	u8* m_data;
	AssetIndex m_index;

	bool ResizeStorage(u32 minCapacity);
	bool ReserveMemory(u32 size);
	static constexpr u32 GetNextPOT(u32 n);
};