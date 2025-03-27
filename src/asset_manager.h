#pragma once
#include "typedef.h"
#include "debug.h"
#include <filesystem>
#include <unordered_map>

constexpr u32 MAX_ASSET_NAME_LENGTH = 56;

enum AssetType {
	ASSET_TYPE_CHR_BANK,
	ASSET_TYPE_SOUND,
	ASSET_TYPE_TILESET,
	ASSET_TYPE_METASPRITE,
	ASSET_TYPE_ACTOR_PROTOTYPE,
	ASSET_TYPE_ROOM,
	ASSET_TYPE_DUNGEON,

	ASSET_TYPE_COUNT,
};

struct AssetFlags {
	u8 type : 4;
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

#ifdef EDITOR
constexpr const char* ASSET_TYPE_NAMES[ASSET_TYPE_COUNT] = { "Chr bank", "Sound", "Tileset", "Metasprite", "Actor prototype", "Room", "Dungeon" };
#endif

namespace AssetManager {
	bool LoadArchive(const std::filesystem::path& path);
	bool SaveArchive(const std::filesystem::path& path);
	bool RepackArchive();

	u64 CreateAsset(u8 type, u32 size, const char* name);
	void* GetAsset(u64 id);
	AssetEntry* GetAssetInfo(u64 id);

	u32 GetAssetCount();
	AssetIndex& GetIndex();

}