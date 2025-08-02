#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "debug.h"
#include <filesystem>
#include <unordered_map>

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
	template <IsAssetHandle HandleType>
	HandleType CreateAsset(u32 size, const char* name) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		const u64 id = CreateAsset(assetType, size, name);
		return HandleType{ id };
	}

	void* AddAsset(u64 id, AssetType type, u32 size, const char* name, void* data = nullptr);
	bool RemoveAsset(u64 id);

	bool ResizeAsset(u64 id, u32 newSize);

	void* GetAsset(u64 id, AssetType type);
	template <IsAssetHandle HandleType>
	typename AssetHandleTraits<HandleType>::data_type* GetAsset(const HandleType& handle) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		using T = typename AssetHandleTraits<HandleType>::data_type;
		return (T*)GetAsset(handle.id, assetType);
	}

	AssetEntry* GetAssetInfo(u64 id);
	const char* GetAssetName(u64 id);

	u32 GetAssetCount();
	AssetIndex& GetIndex();

}