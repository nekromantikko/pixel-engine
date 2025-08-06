#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "memory_pool.h"
#include <filesystem>

constexpr u32 MAX_ASSET_NAME_LENGTH = 56;
constexpr u32 MAX_ASSET_PATH_LENGTH = 256;
constexpr u32 MAX_ASSETS = 1024;

struct AssetFlags {
	AssetType type : 4;
	bool deleted : 1;
	bool compressed : 1; // NOTE: For the future maybe?
};

struct AssetEntry {
	u64 id;
	char name[MAX_ASSET_NAME_LENGTH];
	char relativePath[MAX_ASSET_PATH_LENGTH];
	size_t offset;
	size_t size;
	AssetFlags flags;
};

typedef Pool<AssetEntry, MAX_ASSETS> AssetIndex;

// Self-contained asset archive class without dependencies on debug or random
class AssetArchive {
public:
	AssetArchive();
	~AssetArchive();

	// Archive file operations
	bool LoadFromFile(const std::filesystem::path& path);
	bool SaveToFile(const std::filesystem::path& path);

	// Asset operations
	void* AddAsset(u64 id, AssetType type, size_t size, const char* path, const char* name, const void* data = nullptr);
	bool RemoveAsset(u64 id);
	bool ResizeAsset(u64 id, size_t newSize);
	
	// Asset retrieval
	void* GetAssetData(u64 id);
	void* GetAssetData(const AssetEntry* pEntry);
	AssetEntry* GetAssetEntry(u64 id);
	AssetEntry* GetAssetEntryByPath(const std::filesystem::path& relativePath);
	
	// Archive management
	void Repack();
	void Clear();
	
	// Statistics
	size_t GetAssetCount() const;
	const AssetIndex& GetIndex() const;

private:
	size_t m_capacity;
	size_t m_size;
	u8* m_data;
	AssetIndex m_index;

	bool ResizeStorage(size_t minCapacity);
	bool ReserveMemory(size_t size);
	// Binary search helpers for sorted asset pool
	AssetEntry* FindAssetByIdBinary(u64 id);
	const AssetEntry* FindAssetByIdBinary(u64 id) const;
};