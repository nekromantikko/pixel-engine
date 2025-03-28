#pragma once

enum AssetType : u8 {
	ASSET_TYPE_CHR_BANK,
	ASSET_TYPE_SOUND,
	ASSET_TYPE_TILESET,
	ASSET_TYPE_METASPRITE,
	ASSET_TYPE_ACTOR_PROTOTYPE,
	ASSET_TYPE_ROOM,
	ASSET_TYPE_DUNGEON,
	ASSET_TYPE_OVERWORLD,
	ASSET_TYPE_ANIMATION,

	ASSET_TYPE_COUNT,
};

template <AssetType T>
struct AssetHandle {
	static constexpr AssetType type = T;
	u64 id;

	AssetHandle() = default;
	AssetHandle(const AssetHandle& other) = default;
	AssetHandle(u64 id) : id(id) {}

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

typedef AssetHandle<ASSET_TYPE_CHR_BANK> ChrBankHandle;
typedef AssetHandle<ASSET_TYPE_SOUND> SoundHandle;
typedef AssetHandle<ASSET_TYPE_TILESET> TilesetHandle;
typedef AssetHandle<ASSET_TYPE_METASPRITE> MetaspriteHandle;
typedef AssetHandle<ASSET_TYPE_ACTOR_PROTOTYPE> ActorPrototypeHandle;
typedef AssetHandle<ASSET_TYPE_ROOM> RoomHandle;
typedef AssetHandle<ASSET_TYPE_DUNGEON> DungeonHandle;
typedef AssetHandle<ASSET_TYPE_OVERWORLD> OverworldHandle;
typedef AssetHandle<ASSET_TYPE_ANIMATION> AnimationHandle;
