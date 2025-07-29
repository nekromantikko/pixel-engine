#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "debug.h"
#include <filesystem>
#include <unordered_map>

constexpr u32 MAX_ASSET_NAME_LENGTH = 56;

struct AssetFlags {
	AssetType type : 4;
	bool deleted : 1;
	bool compressed : 1; // NOTE: For the future maybe?
};

struct AssetEntry {
	u64 id;
	char name[MAX_ASSET_NAME_LENGTH];
	u32 offset;
	u32 size;
	AssetFlags flags;
};

#ifdef NDEBUG
typedef std::unordered_map<u64, AssetEntry> AssetIndex;
#else
typedef std::unordered_map < u64, AssetEntry, std::hash<u64>, std::equal_to<u64>, DebugAllocator<std::pair<const u64, AssetEntry>>> AssetIndex;
#endif

namespace AssetManager {
	bool LoadArchive(const std::filesystem::path& path);
	bool SaveArchive(const std::filesystem::path& path);
	bool RepackArchive();

	u64 CreateAsset(AssetType type, u32 size, const char* name);
	template <AssetType T>
	AssetHandle<T> CreateAsset(u32 size, const char* name) {
		const u64 id = CreateAsset(T, size, name);
		return AssetHandle<T>{ id };
	}

	void* AddAsset(u64 id, AssetType type, u32 size, const char* name, void* data = nullptr);
	bool RemoveAsset(u64 id);

	bool ResizeAsset(u64 id, u32 newSize);

	void* GetAsset(u64 id, AssetType type);
	template <AssetType T>
	void* GetAsset(const AssetHandle<T>& handle) {
		return GetAsset(handle.id, T);
	}

	AssetEntry* GetAssetInfo(u64 id);
	const char* GetAssetName(u64 id);

	u32 GetAssetCount();
	AssetIndex& GetIndex();

}