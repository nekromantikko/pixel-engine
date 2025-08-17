#include "editor_assets.h"
#include "debug.h"
#include "actor_data.h"
#include "asset_serialization.h"
#include "random.h"
#include "asset_manager.h"

#pragma region Size calculation
static u32 GetRoomTemplateSize(const RoomTemplate* pHeader) {
	u32 result = sizeof(RoomTemplate);
	constexpr u32 tilemapSize = ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;
	result += tilemapSize;
	result += ROOM_MAP_TILE_COUNT * sizeof(BgTile);
	if (pHeader) {
		result += pHeader->actorCount * sizeof(RoomActor);
	}

	return result;
}

static u32 GetActorPrototypeSize(const ActorPrototype* pHeader) {
	u32 result = sizeof(ActorPrototype);
	if (pHeader) {
		result += pHeader->animCount * sizeof(AnimationHandle);
	}
	return result;
}

static u32 GetAnimationSize(const Animation* pHeader) {
	u32 result = sizeof(Animation);
	if (pHeader) {
		result += pHeader->frameCount * sizeof(AnimationFrame);
	}
	return result;
}

static u32 GetMetaspriteSize(const Metasprite* pHeader) {
	u32 result = sizeof(Metasprite);
	if (pHeader) {
		result += pHeader->spriteCount * sizeof(Sprite);
	}
	return result;
}

static u32 GetSoundSize(const Sound* pHeader) {
	u32 result = sizeof(Sound);
	if (pHeader) {
		result += pHeader->length * sizeof(SoundOperation);
	}

	return result;
}


static u32 GetOverworldSize(const Overworld* pHeader) {
	u32 result = sizeof(Overworld);
	constexpr u32 tilemapSize = OVERWORLD_METATILE_COUNT;
	result += tilemapSize;
	return result;
}

#pragma endregion

#pragma region Initialization
static void InitRoomTemplate(void* data) {
	constexpr u32 mapTilesOffset = sizeof(RoomTemplate);
	constexpr u32 tilesOffset = mapTilesOffset + ROOM_MAP_TILE_COUNT * sizeof(BgTile);
	constexpr u32 actorsOffset = tilesOffset + ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;

	Tilemap tilemap{
		.width = ROOM_MAX_DIM_SCREENS * VIEWPORT_WIDTH_METATILES,
		.height = ROOM_MAX_DIM_SCREENS * VIEWPORT_HEIGHT_METATILES,
		.tilesetHandle = TilesetHandle::Null(),
		.tilesOffset = tilesOffset - offsetof(RoomTemplate, tilemap),
	};

	RoomTemplate newHeader{
		.width = 1,
		.height = 1,
		.mapTileOffset = mapTilesOffset,
		.tilemap = tilemap,
		.actorCount = 0,
		.actorOffset = actorsOffset
	};

	memcpy(data, &newHeader, sizeof(RoomTemplate));
}

static void InitActorPrototype(void* data) {
	constexpr u32 animOffset = sizeof(ActorPrototype);

	ActorPrototype newHeader{
		.type = 0,
		.subtype = 0,
		.hitbox = {-0.5f, 0.5f, -0.5f, 0.5f},
		.data = {},
		.animCount = 0,
		.animOffset = animOffset
	};

	memcpy(data, &newHeader, sizeof(ActorPrototype));
}

static void InitAnimation(void* data) {
	constexpr u32 framesOffset = sizeof(Animation);

	Animation newHeader{
		.frameLength = 4,
		.loopPoint = -1,
		.frameCount = 0,
		.framesOffset = framesOffset
	};

	memcpy(data, &newHeader, sizeof(Animation));
}

static void InitMetasprite(void* data) {
	constexpr u32 spritesOffset = sizeof(Metasprite);

	Metasprite newHeader{
		.spriteCount = 0,
		.spritesOffset = spritesOffset
	};

	memcpy(data, &newHeader, sizeof(Metasprite));
}

static void InitSound(void* data) {
	Sound newSound{};
	newSound.dataOffset = sizeof(Sound);
	memcpy(data, &newSound, sizeof(Sound));
}

static void InitTileset(void* data) {
	Tileset newTileset{};
	memcpy(data, &newTileset, sizeof(Tileset));
}

static void InitChrSheet(void* data) {
	ChrSheet newChrSheet{};
	memcpy(data, &newChrSheet, sizeof(ChrSheet));
}

static void InitDungeon(void* data) {
	Dungeon newDungeon{};
	memcpy(data, &newDungeon, sizeof(Dungeon));
}

static void InitOverworld(void* data) {
	constexpr u32 tilesOffset = sizeof(Overworld);

	Tilemap tilemapHeader {
		.width = OVERWORLD_WIDTH_METATILES,
		.height = OVERWORLD_HEIGHT_METATILES,
		.tilesetHandle = TilesetHandle::Null(),
		.tilesOffset = tilesOffset - offsetof(Overworld, tilemap),
	};

	Overworld* pOverworld = (Overworld*)data;
	pOverworld->tilemap = tilemapHeader;

	for (u32 i = 0; i < MAX_OVERWORLD_KEY_AREA_COUNT; i++) {
		pOverworld->keyAreas[i].position = { -1, -1 };
	}
}
#pragma endregion

void Editor::Assets::InitializeAsset(AssetType type, void* pData) {
	switch (type) {
	case ASSET_TYPE_ROOM_TEMPLATE:
		InitRoomTemplate(pData);
		break;
	case ASSET_TYPE_ACTOR_PROTOTYPE:
		InitActorPrototype(pData);
		break;
	case ASSET_TYPE_ANIMATION:
		InitAnimation(pData);
		break;
	case ASSET_TYPE_METASPRITE:
		InitMetasprite(pData);
		break;
	case ASSET_TYPE_SOUND:
		InitSound(pData);
		break;
	case ASSET_TYPE_TILESET:
		InitTileset(pData);
		break;
	case ASSET_TYPE_CHR_BANK:
		InitChrSheet(pData);
		break;
	case ASSET_TYPE_DUNGEON:
		InitDungeon(pData);
		break;
	case ASSET_TYPE_OVERWORLD:
		InitOverworld(pData);
		break;
		// Add other asset types initialization as needed
	default:
		DEBUG_ERROR("Unsupported asset type for initialization: %s\n", ASSET_TYPE_NAMES[type]);
		break;
	}

}
u32 Editor::Assets::GetAssetSize(AssetType type, const void* pData) {
	switch (type) {
	case ASSET_TYPE_ROOM_TEMPLATE:
		return GetRoomTemplateSize((RoomTemplate*)pData);
	case ASSET_TYPE_ACTOR_PROTOTYPE:
		return GetActorPrototypeSize((ActorPrototype*)pData);
	case ASSET_TYPE_ANIMATION:
		return GetAnimationSize((Animation*)pData);
	case ASSET_TYPE_METASPRITE:
		return GetMetaspriteSize((Metasprite*)pData);
	case ASSET_TYPE_SOUND:
		return GetSoundSize((Sound*)pData);
	case ASSET_TYPE_TILESET:
		return sizeof(Tileset);
	case ASSET_TYPE_CHR_BANK:
		return sizeof(ChrSheet);
	case ASSET_TYPE_DUNGEON:
		return sizeof(Dungeon);
	case ASSET_TYPE_OVERWORLD:
		return GetOverworldSize((Overworld*)pData);
	case ASSET_TYPE_PALETTE:
		return sizeof(Palette);
		// Add other asset types size calculation as needed
	default:
		DEBUG_ERROR("Unsupported asset type for size calculation: %s\n", ASSET_TYPE_NAMES[type]);
		return 0;
	}
}

bool Editor::Assets::LoadAssetsFromDirectory(const std::filesystem::path& directory) {
	if (!std::filesystem::exists(directory)) {
		DEBUG_ERROR("Directory (%s) does not exist\n", directory.string().c_str());
		return false;
	}

	if (!std::filesystem::is_directory(directory)) {
		DEBUG_ERROR("Path (%s) is not a directory\n", directory.string().c_str());
		return false;
	}

	DEBUG_LOG("Listing assets in directory: %s\n", directory.string().c_str());

	std::vector<u8> data;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
		AssetType assetType;
		if (entry.is_regular_file() && AssetSerialization::TryGetAssetTypeFromPath(entry.path(), assetType) == SERIALIZATION_SUCCESS) {
			const std::string pathStr = entry.path().string();
			const char* pathCStr = pathStr.c_str();
			DEBUG_LOG("Found %s: %s\n", ASSET_TYPE_NAMES[assetType], pathCStr);

			nlohmann::json metadata;
			if (!AssetSerialization::HasMetadata(entry.path())) {
				DEBUG_LOG("No metadata found for asset %s, creating new metadata file\n", pathCStr);
				u64 guid = Random::GenerateUUID();
				if (AssetSerialization::CreateAssetMetadataFile(entry.path(), guid, metadata) != SERIALIZATION_SUCCESS) {
					DEBUG_ERROR("Failed to create metadata for asset %s\n", pathCStr);
					continue;
				}
			}

			if (AssetSerialization::LoadAssetMetadataFromFile(entry.path(), metadata) != SERIALIZATION_SUCCESS) {
				DEBUG_ERROR("Failed to load metadata for asset %s\n", pathCStr);
				continue;
			}

			const u64 guid = metadata["guid"];

			data.clear();
			if (AssetSerialization::LoadAssetFromFile(entry.path(), assetType, metadata, data) != SERIALIZATION_SUCCESS) {
				DEBUG_ERROR("Failed to load asset %s\n", pathCStr);
				continue;
			}

			const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), ASSETS_SRC_DIR);
			if (!AssetManager::AddAsset(guid, assetType, data.size(), relativePath.string().c_str(), data.data())) {
				DEBUG_ERROR("Failed to add asset %s to manager\n", pathCStr);
				continue;
			}
			DEBUG_LOG("Asset %s loaded successfully with GUID: %llu\n", pathCStr, guid);
		}
	}

	return true;
}