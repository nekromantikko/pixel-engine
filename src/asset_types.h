#pragma once
#include "typedef.h"

enum AssetType : u8 {
	ASSET_TYPE_CHR_BANK,
	ASSET_TYPE_SOUND,
	ASSET_TYPE_TILESET,
	ASSET_TYPE_METASPRITE,
	ASSET_TYPE_ACTOR_PROTOTYPE,
	ASSET_TYPE_ROOM_TEMPLATE,
	ASSET_TYPE_DUNGEON,
	ASSET_TYPE_OVERWORLD,
	ASSET_TYPE_ANIMATION,
	ASSET_TYPE_PALETTE,

	ASSET_TYPE_COUNT,
};

template <AssetType T>
struct AssetHandle {
	static constexpr AssetType type = T;
	u64 id;

	constexpr AssetHandle() = default;
	constexpr AssetHandle(const AssetHandle& other) = default;
	explicit constexpr AssetHandle(u64 id) : id(id) {}

	bool operator==(const AssetHandle& other) const {
		return id == other.id;
	}
	bool operator!=(const AssetHandle& other) const {
		return id != other.id;
	}

	inline static AssetHandle Null() {
		return AssetHandle(0);
	}
};

template <AssetType T>
constexpr AssetHandle<T> MakeAssetHandle(u64 id, AssetType type) {
	return (AssetHandle<type>)(id);
}

typedef AssetHandle<ASSET_TYPE_CHR_BANK> ChrBankHandle;
typedef AssetHandle<ASSET_TYPE_SOUND> SoundHandle;
typedef AssetHandle<ASSET_TYPE_TILESET> TilesetHandle;
typedef AssetHandle<ASSET_TYPE_METASPRITE> MetaspriteHandle;
typedef AssetHandle<ASSET_TYPE_ACTOR_PROTOTYPE> ActorPrototypeHandle;
typedef AssetHandle<ASSET_TYPE_ROOM_TEMPLATE> RoomTemplateHandle;
typedef AssetHandle<ASSET_TYPE_DUNGEON> DungeonHandle;
typedef AssetHandle<ASSET_TYPE_OVERWORLD> OverworldHandle;
typedef AssetHandle<ASSET_TYPE_ANIMATION> AnimationHandle;
typedef AssetHandle<ASSET_TYPE_PALETTE> PaletteHandle;

#ifdef EDITOR
constexpr const char* ASSET_TYPE_NAMES[ASSET_TYPE_COUNT] = { "Chr bank", "Sound", "Tileset", "Metasprite", "Actor prototype", "Room template", "Dungeon", "Overworld", "Animation", "Palette" };
#endif
