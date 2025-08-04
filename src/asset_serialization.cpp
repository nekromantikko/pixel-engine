#include "asset_serialization.h"
#include <cstdlib>
#include "core_types.h"
#include "data_types.h"
#include "actor_data.h"

static constexpr u64 ASSET_FILE_FORMAT_VERSION = 1;

#pragma region JSON Helpers

NLOHMANN_JSON_SERIALIZE_ENUM(SoundChannelId, {
	{ CHAN_ID_PULSE0, "pulse 1" },
	{ CHAN_ID_PULSE1, "pulse 2" },
	{ CHAN_ID_TRIANGLE, "triangle" },
	{ CHAN_ID_NOISE, "noise" }
	// { CHAN_ID_DPCM, "dpcm" },
})

NLOHMANN_JSON_SERIALIZE_ENUM(SoundType, {
	{ SOUND_TYPE_SFX, "sfx" },
	{ SOUND_TYPE_MUSIC, "music" },
})

static void from_json(const nlohmann::json& j, Sprite& sprite) {
	sprite.x = j.at("x").get<s16>();
	sprite.y = j.at("y").get<s16>();
	sprite.tileId = j.at("tile_id").get<u16>();
	sprite.palette = j.at("palette").get<u8>();
	sprite.priority = j.at("priority").get<bool>() ? 1 : 0;
	sprite.flipHorizontal = j.at("flip_horizontal").get<bool>() ? 1 : 0;
	sprite.flipVertical = j.at("flip_vertical").get<bool>() ? 1 : 0;
}

static void to_json(nlohmann::json& j, const Sprite& sprite) {
	j["x"] = sprite.x;
	j["y"] = sprite.y;
	j["tile_id"] = sprite.tileId;
	j["palette"] = sprite.palette;
	j["priority"] = sprite.priority != 0;
	j["flip_horizontal"] = sprite.flipHorizontal != 0;
	j["flip_vertical"] = sprite.flipVertical != 0;
}

NLOHMANN_JSON_SERIALIZE_ENUM(TilesetTileType, {
	{ TILE_EMPTY, "empty" },
	{ TILE_SOLID, "solid" }
})

static void from_json(const nlohmann::json& j, BgTile& tile) {
	tile.tileId = j.at("tile_id").get<u16>();
	tile.palette = j.at("palette").get<u16>();
	tile.flipHorizontal = j.at("flip_horizontal").get<bool>();
	tile.flipVertical = j.at("flip_vertical").get<bool>();
	tile.unused = 0; // Unused bit set to 0
}

static void to_json(nlohmann::json& j, const BgTile& tile) {
	j["tile_id"] = tile.tileId;
	j["palette"] = tile.palette;
	j["flip_horizontal"] = tile.flipHorizontal != 0;
	j["flip_vertical"] = tile.flipVertical != 0;
}

static void from_json(const nlohmann::json& j, Metatile& metatile) {
	for (u32 i = 0; i < METATILE_TILE_COUNT; ++i) {
		metatile.tiles[i] = j.at("tiles").at(i).get<BgTile>();
	}
}

static void to_json(nlohmann::json& j, const Metatile& metatile) {
	j["tiles"] = nlohmann::json::array();
	for (u32 i = 0; i < METATILE_TILE_COUNT; ++i) {
		j["tiles"].push_back(metatile.tiles[i]);
	}
}

static void from_json(const nlohmann::json& j, TilesetTile& tile) {
	TilesetTileType type;
	j.at("type").get_to(type);
	tile.type = (s32)type;
	j.at("metatile").get_to(tile.metatile);
}

static void to_json(nlohmann::json& j, const TilesetTile& tile) {
	j["type"] = (TilesetTileType)tile.type;
	j["metatile"] = tile.metatile;
}

static void from_json(const nlohmann::json& j, Tileset& tileset) {
	for (u32 i = 0; i < TILESET_SIZE; ++i) {
		tileset.tiles[i] = j.at("tiles").at(i).get<TilesetTile>();
	}
}

inline void to_json(nlohmann::json& j, const Tileset& tileset) {
	j["tiles"] = nlohmann::json::array();
	for (u32 i = 0; i < TILESET_SIZE; ++i) {
		j["tiles"].push_back(tileset.tiles[i]);
	}
}

static void from_json(const nlohmann::json& j, AnimationFrame& frame) {
	j.at("metasprite_id").get_to(frame.metaspriteId.id);
}

static void to_json(nlohmann::json& j, const AnimationFrame& frame) {
	j["metasprite_id"] = frame.metaspriteId.id;
}

NLOHMANN_JSON_SERIALIZE_ENUM(ActorType, {
	{ ACTOR_TYPE_PLAYER, "player" },
	{ ACTOR_TYPE_ENEMY, "enemy" },
	{ ACTOR_TYPE_BULLET, "bullet" },
	{ ACTOR_TYPE_PICKUP, "pickup" },
	{ ACTOR_TYPE_EFFECT, "effect" },
	{ ACTOR_TYPE_INTERACTABLE, "interactable" },
	{ ACTOR_TYPE_SPAWNER, "spawner" }
	})

static void to_json(nlohmann::json& j, const AABB& aabb) {
	j["min_x"] = aabb.min.x;
	j["min_y"] = aabb.min.y;
	j["max_x"] = aabb.max.x;
	j["max_y"] = aabb.max.y;
}

static void from_json(const nlohmann::json& j, AABB& aabb) {
	aabb.min.x = j.at("min_x").get<r32>();
	aabb.min.y = j.at("min_y").get<r32>();
	aabb.max.x = j.at("max_x").get<r32>();
	aabb.max.y = j.at("max_y").get<r32>();
}

static void from_json(const nlohmann::json& j, RoomActor& actor) {
	j.at("id").get_to(actor.id);
	j.at("prototype_id").get_to(actor.prototypeHandle.id);
	j.at("x").get_to(actor.position.x);
	j.at("y").get_to(actor.position.y);
}

static void to_json(nlohmann::json& j, const RoomActor& actor) {
	j["id"] = actor.id;
	j["prototype_id"] = actor.prototypeHandle.id;
	j["x"] = actor.position.x;
	j["y"] = actor.position.y;
}

static void from_json(const nlohmann::json& j, OverworldKeyArea& area) {
	j.at("dungeon_id").get_to(area.dungeonId.id);
	j.at("x").get_to(area.position.x);
	j.at("y").get_to(area.position.y);
	j.at("target_x").get_to(area.targetGridCell.x);
	j.at("target_y").get_to(area.targetGridCell.y);
	area.flags.flipDirection = j.value("flip_direction", 0);
	area.flags.passthrough = j.value("passthrough", 0);
}

static void to_json(nlohmann::json& j, const OverworldKeyArea& area) {
	j["dungeon_id"] = area.dungeonId.id;
	j["x"] = area.position.x;
	j["y"] = area.position.y;
	j["target_x"] = area.targetGridCell.x;
	j["target_y"] = area.targetGridCell.y;
	j["flip_direction"] = area.flags.flipDirection != 0;
	j["passthrough"] = area.flags.passthrough != 0;
}

static void from_json(const nlohmann::json& j, DungeonCell& cell) {
	cell.roomIndex = j.at("room_index").get<s8>();
	cell.screenIndex = j.at("screen_index").get<u8>();
}

static void to_json(nlohmann::json& j, const DungeonCell& cell) {
	j["room_index"] = cell.roomIndex;
	j["screen_index"] = cell.screenIndex;
}

static void from_json(const nlohmann::json& j, RoomInstance& room) {
	room.id = j.at("id").get<u32>();
	room.templateId.id = j.at("template_id").get<u64>();
}

static void to_json(nlohmann::json& j, const RoomInstance& room) {
	j["id"] = room.id;
	j["template_id"] = room.templateId.id;
}

static void from_json(const nlohmann::json& j, Dungeon& dungeon) {
	dungeon.roomCount = (u32)j.at("rooms").size();
	for (u32 i = 0; i < dungeon.roomCount; ++i) {
		dungeon.rooms[i] = j.at("rooms").at(i).get<RoomInstance>();
	}
	for (u32 i = 0; i < DUNGEON_GRID_SIZE; ++i) {
		dungeon.grid[i] = j.at("grid").at(i).get<DungeonCell>();
	}
}

static void to_json(nlohmann::json& j, const Dungeon& dungeon) {
	j["rooms"] = nlohmann::json::array();
	for (u32 i = 0; i < dungeon.roomCount; ++i) {
		j["rooms"].push_back(dungeon.rooms[i]);
	}
	j["grid"] = nlohmann::json::array();
	for (u32 i = 0; i < DUNGEON_GRID_SIZE; ++i) {
		j["grid"].push_back(dungeon.grid[i]);
	}
}

#pragma endregion

#pragma region Asset serializers

#pragma pack(push, 1)
	struct BitmapInfo {
		u32 infoSize;
		s32 width;
		s32 height;
		u16 planes;
		u16 bpp;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct BitmapHeader {
		char signature[2];
		u32 size;
		u32 reserved;
		u32 dataOffset;
		BitmapInfo info;
	};
#pragma pack(pop)

static void CreateChrSheetFromBmp(const char* pixels, ChrSheet* pOutSheet) {
	for (u32 y = 0; y < CHR_DIM_PIXELS; y++) {
		for (u32 x = 0; x < CHR_DIM_PIXELS; x++) {
			u32 coarseX = x / 8;
			u32 coarseY = y / 8;
			u32 fineX = x % 8;
			u32 fineY = y % 8;
			u32 tileIndex = (15 - coarseY) * 16 + coarseX; // Tile 0 is top left instead of bottom left
			u32 inPixelIndex = y * CHR_DIM_PIXELS + x;
			u32 outPixelIndex = (7 - fineY) * 8 + fineX; // Also pixels go from top to bottom in this program, but bottom to top in bmp, so flip

			u8 pixel = pixels[inPixelIndex];

			ChrTile& tile = pOutSheet->tiles[tileIndex];
			tile.p0 = (tile.p0 & ~(1ULL << outPixelIndex)) | ((u64)(pixel & 0b00000001) << outPixelIndex);
			tile.p1 = (tile.p1 & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000010) >> 1) << outPixelIndex);
			tile.p2 = (tile.p2 & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000100) >> 2) << outPixelIndex);
		}
	}
}

static SerializationResult LoadChrSheetFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	if (!pOutData) {
		size = sizeof(ChrSheet);
		return SERIALIZATION_SUCCESS; // Just return size if no output data is provided
	}

	BitmapHeader header{};
	fread(&header, sizeof(BitmapHeader), 1, pFile);
	if (memcmp(header.signature, "BM", 2) != 0) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid bitmap signature
	}

	if (header.info.bpp != 8) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Only 8-bit BMPs are supported
	}

	if (header.info.width != CHR_DIM_PIXELS || header.info.height != CHR_DIM_PIXELS) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Only CHR_DIM_PIXELS x CHR_DIM_PIXELS BMPs are supported
	}

	fseek(pFile, header.dataOffset, SEEK_SET);
	char pixels[CHR_DIM_PIXELS * CHR_DIM_PIXELS];
	fread(pixels, 1, CHR_DIM_PIXELS * CHR_DIM_PIXELS, pFile);

	CreateChrSheetFromBmp(pixels, (ChrSheet*)pOutData);

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SaveChrSheetToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	// TODO
	return SERIALIZATION_NOT_IMPLEMENTED;
}

struct NSFHeader {
	char signature[4];
	u32 unused;
	u32 size;
	u32 loopPoint;
};

static SerializationResult LoadSoundFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	NSFHeader header{};
	fread(&header, sizeof(NSFHeader), 1, pFile);
	if (strcmp(header.signature, "NSF") != 0) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid NSF signature
	}

	size = sizeof(Sound) + header.size * sizeof(SoundOperation);

	if (pOutData) {
		Sound* pSound = (Sound*)pOutData;
		pSound->length = header.size;
		pSound->loopPoint = header.loopPoint;
		pSound->dataOffset = sizeof(Sound);

		if (metadata.contains("sound_type") && metadata["sound_type"] != nullptr) {
			SoundType type;
			metadata.at("sound_type").get_to(type);
			pSound->type = (u16)type;
		}
		else {
			pSound->type = SOUND_TYPE_SFX; // Default to SFX if not specified
		}

		if (metadata.contains("sfx_channel") && metadata["sfx_channel"] != nullptr) {
			SoundChannelId channel;
			metadata.at("sfx_channel").get_to(channel);
			pSound->sfxChannel = (u16)channel;
		}
		else {
			pSound->sfxChannel = CHAN_ID_PULSE0; // Default to PULSE0 if not specified
		}

		SoundOperation* pData = pSound->GetData();
		fread(pData, sizeof(SoundOperation), header.size, pFile);
	}

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SaveSoundToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	// For sounds, we only save metadata, not the binary data
	// The binary sound file should remain as-is (NSF format)
	// Metadata like sound_type and sfx_channel should be saved to .meta file

	const Sound* pSound = (const Sound*)pData;

	metadata["sound_type"] = (SoundType)pSound->type;
	metadata["sfx_channel"] = (SoundChannelId)pSound->sfxChannel;

	return SERIALIZATION_SUCCESS;
}

static SerializationResult LoadPaletteFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	if (!pOutData) {
		size = sizeof(Palette);
		return SERIALIZATION_SUCCESS; // Just return size if no output data is provided
	}

	fread(pOutData, sizeof(Palette), 1, pFile);

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SavePaletteToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	fwrite(pData, sizeof(Palette), 1, pFile);

	return SERIALIZATION_SUCCESS;
}

static SerializationResult LoadMetaspriteFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	const nlohmann::json json = nlohmann::json::parse(pFile);
	const nlohmann::json spritesJson = json["sprites"];
	const size_t spriteCount = spritesJson != nullptr && spritesJson.is_array() ? spritesJson.size() : 0;
	size = sizeof(Metasprite) + spriteCount * sizeof(Sprite);

	if (pOutData) {
		Metasprite* pMetasprite = (Metasprite*)pOutData;
		pMetasprite->spriteCount = (u32)spriteCount;
		pMetasprite->spritesOffset = sizeof(Metasprite);

		Sprite* pSprites = pMetasprite->GetSprites();
		for (u32 i = 0; i < spriteCount; i++) {
			spritesJson[i].get_to(pSprites[i]);
		}
	}

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SaveMetaspriteToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	const Metasprite* pMetasprite = (const Metasprite*)pData;
	nlohmann::json json;

	json["sprites"] = nlohmann::json::array();
	const Sprite* pSprites = pMetasprite->GetSprites();
	for (u32 i = 0; i < pMetasprite->spriteCount; i++) {
		json["sprites"].push_back(pSprites[i]);
	}
	std::string jsonStr = json.dump(4);
	fwrite(jsonStr.c_str(), sizeof(char), jsonStr.size(), pFile);

	return SERIALIZATION_SUCCESS;
}

static SerializationResult LoadTilesetFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	if (!pOutData) {
		size = sizeof(Tileset);
		return SERIALIZATION_SUCCESS; // Just return size if no output data is provided
	}

	const nlohmann::json json = nlohmann::json::parse(pFile);
	json.get_to(*(Tileset*)pOutData);

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SaveTilesetToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	const Tileset* pTileset = (const Tileset*)pData;
	nlohmann::json json = *pTileset;

	std::string jsonStr = json.dump(4);
	fwrite(jsonStr.c_str(), sizeof(char), jsonStr.size(), pFile);

	return SERIALIZATION_SUCCESS;
}

static SerializationResult LoadAnimationFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	const nlohmann::json json = nlohmann::json::parse(pFile);
	const nlohmann::json framesJson = json["frames"];
	const size_t frameCount = framesJson != nullptr && framesJson.is_array() ? framesJson.size() : 0;
	size = sizeof(Animation) + frameCount * sizeof(AnimationFrame);

	if (pOutData) {
		Animation* pAnim = (Animation*)pOutData;
		pAnim->frameLength = json["frame_length"].get<u8>();
		pAnim->loopPoint = json["loop_point"].get<s16>();
		pAnim->frameCount = (u16)frameCount;
		pAnim->framesOffset = sizeof(Animation);

		AnimationFrame* pFrames = pAnim->GetFrames();
		for (u32 i = 0; i < frameCount; i++) {
			framesJson[i].get_to(pFrames[i]);
		}
	}

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SaveAnimationToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	const Animation* pAnim = (const Animation*)pData;
	nlohmann::json json;

	json["frame_length"] = pAnim->frameLength;
	json["loop_point"] = pAnim->loopPoint;

	json["frames"] = nlohmann::json::array();
	const AnimationFrame* pFrames = pAnim->GetFrames();
	for (u32 i = 0; i < pAnim->frameCount; i++) {
		json["frames"].push_back(pFrames[i]);
	}

	std::string jsonStr = json.dump(4);
	fwrite(jsonStr.c_str(), sizeof(char), jsonStr.size(), pFile);

	return SERIALIZATION_SUCCESS;
}

static bool ParseActorSubtype(const std::string& subtypeStr, const ActorEditorData& editorData, TActorSubtype& outSubtype) {
	for (u32 i = 0; i < editorData.GetSubtypeCount(); i++) {
		const char* subtypeName = editorData.GetSubtypeNames()[i];
		if (strcmp(subtypeStr.c_str(), subtypeName) == 0) {
			outSubtype = (TActorSubtype)i;
			return true;
		}
	}
	return false;
}

static bool ParseActorPropertyName(const std::string& propertyName, TActorSubtype subtype, const ActorEditorData& editorData, const ActorEditorProperty** ppOutProperty) {
	for (u32 i = 0; i < editorData.GetPropertyCount(subtype); i++) {
		const ActorEditorProperty& prop = editorData.GetProperty(subtype, i);
		if (strcmp(prop.name, propertyName.c_str()) == 0) {
			*ppOutProperty = &prop;
			return true;
		}
	}
	return false;
}

static bool ParseActorPropertyComponentValueScalar(const nlohmann::json& jsonValue, const ActorEditorProperty& property, void* pOutData) {
	switch (property.dataType) {
	case DATA_TYPE_S8: {
		jsonValue.get_to(*(s8*)pOutData);
		break;
	}
	case DATA_TYPE_U8: {
		jsonValue.get_to(*(u8*)pOutData);
		break;
	}
	case DATA_TYPE_S16: {
		jsonValue.get_to(*(s16*)pOutData);
		break;
	}
	case DATA_TYPE_U16: {
		jsonValue.get_to(*(u16*)pOutData);
		break;
	}
	case DATA_TYPE_S32: {
		jsonValue.get_to(*(s32*)pOutData);
		break;
	}
	case DATA_TYPE_U32: {
		jsonValue.get_to(*(u32*)pOutData);
		break;
	}
	case DATA_TYPE_S64: {
		jsonValue.get_to(*(s64*)pOutData);
		break;
	}
	case DATA_TYPE_U64: {
		jsonValue.get_to(*(u64*)pOutData);
		break;
	}
	case DATA_TYPE_R32: {
		jsonValue.get_to(*(r32*)pOutData);
		break;
	}
	case DATA_TYPE_R64: {
		jsonValue.get_to(*(r64*)pOutData);
		break;
	}
	case DATA_TYPE_BOOL: {
		bool value = jsonValue.get<bool>();
		*(bool*)pOutData = value;
		break;
	}
	default: {
		return false;
	}
	}
	return true;
}

static bool ParseActorPropertyValue(const nlohmann::json& jsonValue, const ActorEditorProperty& property, void* pOutData) {
	switch (property.type) {
	case ACTOR_EDITOR_PROPERTY_SCALAR: {
		const DataTypeInfo* pTypeInfo = GetDataTypeInfo(property.dataType);
		if (property.components == 1) {
			ParseActorPropertyComponentValueScalar(jsonValue, property, pOutData);
		}
		else {
			const nlohmann::json& arrayJson = jsonValue;
			for (s32 i = 0; i < property.components; i++) {
				void* pComponentData = (u8*)pOutData + i * pTypeInfo->size;
				const nlohmann::json& componentValue = arrayJson[i];
				ParseActorPropertyComponentValueScalar(componentValue, property, pComponentData);
			}
		}
		return true;
	}
	case ACTOR_EDITOR_PROPERTY_ASSET: {
		if (property.components == 1) {
			*(u64*)pOutData = jsonValue != nullptr ? jsonValue.get<u64>() : 0;
		}
		else {
			const nlohmann::json& arrayJson = jsonValue;
			for (s32 i = 0; i < property.components; i++) {
				const nlohmann::json& componentValue = arrayJson[i];
				((u64*)pOutData)[i] = componentValue != nullptr ? componentValue.get<u64>() : 0;
			}
		}
		return true;
	}
	default:
		return false;
	}
}

static nlohmann::json SerializeActorPropertyComponentValueScalar(const void* pData, const ActorEditorProperty& property) {
	switch (property.dataType) {
	case DATA_TYPE_S8:
		return *(const s8*)pData;
	case DATA_TYPE_U8:
		return *(const u8*)pData;
	case DATA_TYPE_S16:
		return *(const s16*)pData;
	case DATA_TYPE_U16:
		return *(const u16*)pData;
	case DATA_TYPE_S32:
		return *(const s32*)pData;
	case DATA_TYPE_U32:
		return *(const u32*)pData;
	case DATA_TYPE_S64:
		return *(const s64*)pData;
	case DATA_TYPE_U64:
		return *(const u64*)pData;
	case DATA_TYPE_R32:
		return *(const r32*)pData;
	case DATA_TYPE_R64:
		return *(const r64*)pData;
	case DATA_TYPE_BOOL:
		return *(const bool*)pData;
	default:
		return nullptr;
	}
}

static nlohmann::json SerializeActorPropertyValue(const void* pData, const ActorEditorProperty& property) {
	switch (property.type) {
	case ACTOR_EDITOR_PROPERTY_SCALAR: {
		const DataTypeInfo* pTypeInfo = GetDataTypeInfo(property.dataType);
		if (property.components == 1) {
			return SerializeActorPropertyComponentValueScalar(pData, property);
		}
		else {
			nlohmann::json arrayJson = nlohmann::json::array();
			for (s32 i = 0; i < property.components; i++) {
				const void* pComponentData = (const u8*)pData + i * pTypeInfo->size;
				arrayJson.push_back(SerializeActorPropertyComponentValueScalar(pComponentData, property));
			}
			return arrayJson;
		}
	}
	case ACTOR_EDITOR_PROPERTY_ASSET: {
		if (property.components == 1) {
			return *(const u64*)pData;
		}
		else {
			nlohmann::json arrayJson = nlohmann::json::array();
			for (s32 i = 0; i < property.components; i++) {
				arrayJson.push_back(((const u64*)pData)[i]);
			}
			return arrayJson;
		}
	}
	default:
		return nullptr;
	}
}

static SerializationResult LoadActorPrototypeFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	const nlohmann::json json = nlohmann::json::parse(pFile);
	const nlohmann::json animationsJson = json["animation_ids"];
	const size_t animCount = animationsJson != nullptr && animationsJson.is_array() ? animationsJson.size() : 0;
	size = sizeof(ActorPrototype) + animCount * sizeof(AnimationHandle);

	if (pOutData) {
		ActorPrototype* pProto = (ActorPrototype*)pOutData;
		pProto->type = json["type"].get<ActorType>();

		const ActorEditorData& editorData = Editor::actorEditorData[pProto->type];
		std::string subtypeStr = json["subtype"].get<std::string>();
		if (!ParseActorSubtype(subtypeStr, editorData, pProto->subtype)) {
			return SERIALIZATION_INVALID_ASSET_DATA; // Invalid subtype
		}

		json["hitbox"].get_to(pProto->hitbox);

		memset(&pProto->data, 0, sizeof(pProto->data));
		const nlohmann::json& propertiesJson = json["properties"];
		if (propertiesJson.is_object()) {
			for (const auto& [propertyName, propertyValue] : propertiesJson.items()) {
				const ActorEditorProperty* pProperty = nullptr;
				if (!ParseActorPropertyName(propertyName, pProto->subtype, editorData, &pProperty)) {
					continue;
				}

				void* pPropertyData = (u8*)&pProto->data + pProperty->offset;
				ParseActorPropertyValue(propertyValue, *pProperty, pPropertyData);
			}
		}

		pProto->animCount = (u32)animCount;
		pProto->animOffset = sizeof(ActorPrototype);

		AnimationHandle* pAnims = pProto->GetAnimations();
		for (size_t i = 0; i < animCount; i++) {
			pAnims[i].id = animationsJson[i].get<u64>();
		}
	}

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SaveActorPrototypeToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	const ActorPrototype* pProto = (const ActorPrototype*)pData;
	const ActorEditorData& editorData = Editor::actorEditorData[pProto->type];

	nlohmann::json json;
	json["type"] = (ActorType)pProto->type;

	// Get subtype name
	if (pProto->subtype < editorData.GetSubtypeCount()) {
		json["subtype"] = editorData.GetSubtypeNames()[pProto->subtype];
	}
	else {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid subtype index
	}

	json["hitbox"] = pProto->hitbox;

	// Serialize properties
	json["properties"] = nlohmann::json::object();
	for (u32 i = 0; i < editorData.GetPropertyCount(pProto->subtype); i++) {
		const ActorEditorProperty& prop = editorData.GetProperty(pProto->subtype, i);
		const void* pPropertyData = (const u8*)&pProto->data + prop.offset;
		json["properties"][prop.name] = SerializeActorPropertyValue(pPropertyData, prop);
	}

	// Serialize animation IDs
	json["animation_ids"] = nlohmann::json::array();
	const AnimationHandle* pAnims = pProto->GetAnimations();
	for (u32 i = 0; i < pProto->animCount; i++) {
		json["animation_ids"].push_back(pAnims[i].id);
	}

	std::string jsonStr = json.dump(4);
	fwrite(jsonStr.c_str(), sizeof(char), jsonStr.size(), pFile);

	return SERIALIZATION_SUCCESS;
}

static bool ValidateTilemapJson(const nlohmann::json& json, u32& outWidth, u32& outHeight, u32& outTileCount) {
	if (!json.is_object()) {
		return false;
	}

	if (!json.contains("width") || !json.contains("height") || !json.contains("tiles")) {
		return false;
	}

	outWidth = json["width"].get<u32>();
	outHeight = json["height"].get<u32>();
	const nlohmann::json& tilesJson = json["tiles"];
	if (!tilesJson.is_array()) {
		return false;
	}

	outTileCount = (u32)tilesJson.size();

	if (outTileCount != outWidth * outHeight) {
		return false;
	}

	return true;
}

static SerializationResult LoadRoomTemplateFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	const nlohmann::json json = nlohmann::json::parse(pFile);
	const u8 width = json["width"] != nullptr ? json["width"].get<u8>() : 0;
	const u8 height = json["height"] != nullptr ? json["height"].get<u8>() : 0;

	const nlohmann::json mapTilesJson = json["map_tiles"];
	const size_t mapTileCount = mapTilesJson != nullptr && mapTilesJson.is_array() ? mapTilesJson.size() : 0;
	const u32 expectedMapTileCount = ROOM_MAP_TILE_COUNT;
	if (mapTileCount != expectedMapTileCount) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid map tile count
	}

	const nlohmann::json actorsJson = json["actors"];
	const size_t actorCount = actorsJson != nullptr && actorsJson.is_array() ? actorsJson.size() : 0;

	const nlohmann::json tilemapJson = json["tilemap"];
	u32 tilemapWidth, tilemapHeight, tileCount;
	if (!ValidateTilemapJson(tilemapJson, tilemapWidth, tilemapHeight, tileCount)) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid tilemap data
	}

	const u32 expectedTilemapWidth = ROOM_MAX_DIM_SCREENS * VIEWPORT_WIDTH_METATILES;
	if (tilemapWidth != expectedTilemapWidth) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid tilemap width
	}

	const u32 expectedTilemapHeight = ROOM_MAX_DIM_SCREENS * VIEWPORT_HEIGHT_METATILES;
	if (tilemapHeight != expectedTilemapHeight) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid tilemap height
	}

	const u32 expectedTileCount = ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;
	if (tileCount != expectedTileCount) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid tile count
	}

	size = sizeof(RoomTemplate) +
		ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT +
		mapTileCount * sizeof(BgTile) +
		actorCount * sizeof(RoomActor);

	if (pOutData) {
		const u32 mapTileOffset = sizeof(RoomTemplate);
		const u32 tilesOffset = mapTileOffset + ROOM_MAP_TILE_COUNT * sizeof(BgTile);
		const u32 actorOffset = tilesOffset + tileCount;

		RoomTemplate* pRoom = (RoomTemplate*)pOutData;
		pRoom->width = width;
		pRoom->height = height;
		pRoom->mapTileOffset = mapTileOffset;

		Tilemap& tilemap = pRoom->tilemap;
		tilemap.width = tilemapWidth;
		tilemap.height = tilemapHeight;
		tilemap.tilesetHandle.id = tilemapJson["tileset_id"].get<u64>();
		tilemap.tilesOffset = tilesOffset - offsetof(RoomTemplate, tilemap);

		pRoom->actorCount = (u32)actorCount;
		pRoom->actorOffset = actorOffset;

		// Load map tiles
		BgTile* pMapTiles = (BgTile*)((u8*)pRoom + mapTileOffset);
		for (u32 i = 0; i < mapTileCount && i < mapTilesJson.size(); i++) {
			mapTilesJson[i].get_to(pMapTiles[i]);
		}

		// Load room tiles
		u8* pTiles = (u8*)pRoom + tilesOffset;
		const nlohmann::json& tilesJson = tilemapJson["tiles"];
		for (u32 i = 0; i < tileCount && i < tilesJson.size(); i++) {
			tilesJson[i].get_to(pTiles[i]);
		}

		// Load actors
		RoomActor* pActors = (RoomActor*)((u8*)pRoom + actorOffset);
		for (u32 i = 0; i < actorCount; i++) {
			actorsJson[i].get_to(pActors[i]);
		}
	}

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SaveRoomTemplateToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	const RoomTemplate* pRoom = (const RoomTemplate*)pData;
	nlohmann::json json;

	json["width"] = pRoom->width;
	json["height"] = pRoom->height;

	// Serialize map tiles
	json["map_tiles"] = nlohmann::json::array();
	const BgTile* pMapTiles = (const BgTile*)((const u8*)pRoom + pRoom->mapTileOffset);
	for (u32 i = 0; i < ROOM_MAP_TILE_COUNT; i++) {
		json["map_tiles"].push_back(pMapTiles[i]);
	}

	// Serialize tilemap
	json["tilemap"] = nlohmann::json::object();
	json["tilemap"]["width"] = pRoom->tilemap.width;
	json["tilemap"]["height"] = pRoom->tilemap.height;
	json["tilemap"]["tileset_id"] = pRoom->tilemap.tilesetHandle.id;

	json["tilemap"]["tiles"] = nlohmann::json::array();
	const u8* pTiles = (const u8*)&pRoom->tilemap + pRoom->tilemap.tilesOffset;
	const u32 tileCount = pRoom->tilemap.width * pRoom->tilemap.height;
	for (u32 i = 0; i < tileCount; i++) {
		json["tilemap"]["tiles"].push_back(pTiles[i]);
	}

	// Serialize actors
	json["actors"] = nlohmann::json::array();
	const RoomActor* pActors = (const RoomActor*)((const u8*)pRoom + pRoom->actorOffset);
	for (u32 i = 0; i < pRoom->actorCount; i++) {
		json["actors"].push_back(pActors[i]);
	}

	std::string jsonStr = json.dump(4);
	fwrite(jsonStr.c_str(), sizeof(char), jsonStr.size(), pFile);

	return SERIALIZATION_SUCCESS;
}

static SerializationResult LoadOverworldFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	if (!pOutData) {
		size = sizeof(Overworld)
			+ OVERWORLD_METATILE_COUNT
			+ MAX_OVERWORLD_KEY_AREA_COUNT * sizeof(OverworldKeyArea);
		return SERIALIZATION_SUCCESS; // Just return size if no output data is provided
	}
	
	const nlohmann::json json = nlohmann::json::parse(pFile);
	const nlohmann::json tilemapJson = json["tilemap"];
	u32 tilemapWidth, tilemapHeight, tileCount;
	if (!ValidateTilemapJson(tilemapJson, tilemapWidth, tilemapHeight, tileCount)) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid tilemap data
	}

	const u32 expectedTilemapWidth = OVERWORLD_WIDTH_METATILES;
	if (tilemapWidth != expectedTilemapWidth) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid tilemap width
	}

	const u32 expectedTilemapHeight = OVERWORLD_HEIGHT_METATILES;
	if (tilemapHeight != expectedTilemapHeight) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid tilemap height
	}

	const u32 expectedTileCount = OVERWORLD_METATILE_COUNT;
	if (tileCount != expectedTileCount) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Invalid tile count
	}

	Overworld* pOverworld = (Overworld*)pOutData;
	constexpr u32 tilesOffset = sizeof(Overworld);

	// Load the tilemap
	pOverworld->tilemap.width = tilemapWidth;
	pOverworld->tilemap.height = tilemapHeight;
	pOverworld->tilemap.tilesetHandle.id = tilemapJson["tileset_id"].get<u64>();
	pOverworld->tilemap.tilesOffset = tilesOffset - offsetof(Overworld, tilemap);

	// Load the tiles
	u8* pTiles = (u8*)pOverworld + tilesOffset;
	const nlohmann::json& tilesJson = tilemapJson["tiles"];
	for (u32 i = 0; i < tileCount && i < tilesJson.size(); i++) {
		tilesJson[i].get_to(pTiles[i]);
	}

	// Load key areas
	const nlohmann::json& keyAreasJson = json["key_areas"];
	size_t keyAreaCount = keyAreasJson != nullptr && keyAreasJson.is_array() ? keyAreasJson.size() : 0;
	if (keyAreaCount > MAX_OVERWORLD_KEY_AREA_COUNT) {
		return SERIALIZATION_INVALID_ASSET_DATA; // Too many key areas
	}

	for (size_t i = 0; i < keyAreaCount; i++) {
		if (i < keyAreasJson.size()) {
			keyAreasJson[i].get_to(pOverworld->keyAreas[i]);
		}
		else {
			pOverworld->keyAreas[i] = OverworldKeyArea{}; // Initialize remaining areas to default
		}
	}

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SaveOverworldToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	const Overworld* pOverworld = (const Overworld*)pData;
	nlohmann::json json;

	// Serialize tilemap
	json["tilemap"] = nlohmann::json::object();
	json["tilemap"]["width"] = pOverworld->tilemap.width;
	json["tilemap"]["height"] = pOverworld->tilemap.height;
	json["tilemap"]["tileset_id"] = pOverworld->tilemap.tilesetHandle.id;

	json["tilemap"]["tiles"] = nlohmann::json::array();
	const u8* pTiles = (const u8*)&pOverworld->tilemap + pOverworld->tilemap.tilesOffset;
	const u32 tileCount = pOverworld->tilemap.width * pOverworld->tilemap.height;
	for (u32 i = 0; i < tileCount; i++) {
		json["tilemap"]["tiles"].push_back(pTiles[i]);
	}

	// Serialize key areas
	json["key_areas"] = nlohmann::json::array();
	for (u32 i = 0; i < MAX_OVERWORLD_KEY_AREA_COUNT; i++) {
		if (pOverworld->keyAreas[i].position.x != -1 || pOverworld->keyAreas[i].position.y != -1) {
			json["key_areas"].push_back(pOverworld->keyAreas[i]);
		}
	}

	std::string jsonStr = json.dump(4);
	fwrite(jsonStr.c_str(), sizeof(char), jsonStr.size(), pFile);

	return SERIALIZATION_SUCCESS;
}

static SerializationResult LoadDungeonFromFile(FILE* pFile, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	if (!pOutData) {
		size = sizeof(Dungeon);
		return SERIALIZATION_SUCCESS; // Just return size if no output data is provided
	}

	const nlohmann::json json = nlohmann::json::parse(pFile);
	json.get_to<Dungeon>(*(Dungeon*)pOutData);

	return SERIALIZATION_SUCCESS;
}

static SerializationResult SaveDungeonToFile(FILE* pFile, nlohmann::json& metadata, const void* pData) {
	const Dungeon* pDungeon = (const Dungeon*)pData;
	nlohmann::json json = *pDungeon;

	std::string jsonStr = json.dump(4);
	fwrite(jsonStr.c_str(), sizeof(char), jsonStr.size(), pFile);

	return SERIALIZATION_SUCCESS;
}

#pragma endregion

SerializationResult AssetSerialization::TryGetAssetTypeFromPath(const std::filesystem::path& path, AssetType& outType) {
	for (u32 i = 0; i < ASSET_TYPE_COUNT; i++) {
		if (path.extension() == ASSET_TYPE_FILE_EXTENSIONS[i]) {
			outType = (AssetType)i;
			return SERIALIZATION_SUCCESS;
		}
	}
	return SERIALIZATION_INVALID_ASSET_TYPE;
}

bool AssetSerialization::HasMetadata(const std::filesystem::path& path) {
	// Check if the file has a metadata file (e.g., .meta)
	return std::filesystem::exists(GetAssetMetadataPath(path));
}

std::filesystem::path AssetSerialization::GetAssetMetadataPath(const std::filesystem::path& path) {
	// Append ".meta" to the original path to get the metadata file path
	return path.string() + ".meta";
}

std::filesystem::path AssetSerialization::GetAssetFullPath(const std::filesystem::path& relativePath) {
	const std::filesystem::path sourceDirPath = ASSETS_SRC_DIR;
	return sourceDirPath / relativePath;
}

SerializationResult AssetSerialization::LoadAssetMetadataFromFile(const std::filesystem::path& origPath, nlohmann::json& outJson) {
	std::filesystem::path path = GetAssetMetadataPath(origPath);

	if (!std::filesystem::exists(path)) {
		return SERIALIZATION_FILE_NOT_FOUND;
	}

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		return SERIALIZATION_FAILED_TO_OPEN_FILE;
	}

	try {
		outJson = nlohmann::json::parse(pFile);
	}
	catch (const nlohmann::json::parse_error& e) {
		fclose(pFile);
		return SERIALIZATION_INVALID_ASSET_DATA;
	}
	fclose(pFile);

	return SERIALIZATION_SUCCESS;
}

SerializationResult AssetSerialization::SaveAssetMetadataToFile(const std::filesystem::path& origPath, const nlohmann::json& json) {
	std::filesystem::path path = origPath;
	path += ".meta";

	FILE* pFile = fopen(path.string().c_str(), "wb");
	if (!pFile) {
		return SERIALIZATION_FAILED_TO_OPEN_FILE;
	}

	fwrite(json.dump(4).c_str(), sizeof(char), json.dump(4).size(), pFile);
	fclose(pFile);

	return SERIALIZATION_SUCCESS;
}

void AssetSerialization::InitializeMetadataJson(nlohmann::json& json, u64 id) {
	json["file_format_version"] = ASSET_FILE_FORMAT_VERSION;
	json["guid"] = id;
}

SerializationResult AssetSerialization::CreateAssetMetadataFile(const std::filesystem::path& path, u64 guid, nlohmann::json& outMetadata) {
	InitializeMetadataJson(outMetadata, guid);
	return SaveAssetMetadataToFile(path, outMetadata);
}

SerializationResult AssetSerialization::LoadAssetFromFile(const std::filesystem::path& path, AssetType type, const nlohmann::json& metadata, size_t& size, void* pOutData) {
	if (!std::filesystem::exists(path)) {
		return SERIALIZATION_FILE_NOT_FOUND;
	}

	// TODO: Reuse file handle
	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		return SERIALIZATION_FAILED_TO_OPEN_FILE;
	}
	
	SerializationResult result = SERIALIZATION_UNKNOWN_ERROR;
	switch (type) {
	case (ASSET_TYPE_CHR_BANK): {
		result = LoadChrSheetFromFile(pFile, metadata, size, pOutData);
		break;
	}
	case (ASSET_TYPE_SOUND): {
		result = LoadSoundFromFile(pFile, metadata, size, pOutData);
		break;
	}
	case (ASSET_TYPE_PALETTE): {
		result = LoadPaletteFromFile(pFile, metadata, size, pOutData);
		break;
	}
	case (ASSET_TYPE_METASPRITE): {
		result = LoadMetaspriteFromFile(pFile, metadata, size, pOutData);
		break;
	}
	case (ASSET_TYPE_TILESET): {
		result = LoadTilesetFromFile(pFile, metadata, size, pOutData);
		break;
	}
	case (ASSET_TYPE_ANIMATION): {
		result = LoadAnimationFromFile(pFile, metadata, size, pOutData);
		break;
	}
	case (ASSET_TYPE_ACTOR_PROTOTYPE): {
		result = LoadActorPrototypeFromFile(pFile, metadata, size, pOutData);
		break;
	}
	case (ASSET_TYPE_ROOM_TEMPLATE): {
		result = LoadRoomTemplateFromFile(pFile, metadata, size, pOutData);
		break;
	}
	case (ASSET_TYPE_OVERWORLD): {
		result = LoadOverworldFromFile(pFile, metadata, size, pOutData);
		break;
	}
	case (ASSET_TYPE_DUNGEON): {
		result = LoadDungeonFromFile(pFile, metadata, size, pOutData);
		break;
	}
	default:
		result = SERIALIZATION_INVALID_ASSET_TYPE;
		break;
	}

	fclose(pFile);
	return result;
}

SerializationResult AssetSerialization::SaveAssetToFile(const std::filesystem::path& path, const char* name, AssetType type, nlohmann::json& metadata, const void* pData) {
	if (!pData) {
		return SERIALIZATION_NULL_POINTER;
	}
	
	// TODO: Reuse file handle
	FILE* pFile = fopen(path.string().c_str(), "wb");
	if (!pFile) {
		return SERIALIZATION_FAILED_TO_OPEN_FILE;
	}
	
	metadata["name"] = name;

	SerializationResult saveResult = SERIALIZATION_UNKNOWN_ERROR;
	switch (type) {
	case (ASSET_TYPE_CHR_BANK): {
		saveResult = SaveChrSheetToFile(pFile, metadata, pData);
		break;
	}
	case (ASSET_TYPE_SOUND): {
		saveResult = SaveSoundToFile(pFile, metadata, pData);
		break;
	}
	case (ASSET_TYPE_PALETTE): {
		saveResult = SavePaletteToFile(pFile, metadata, pData);
		break;
	}
	case (ASSET_TYPE_METASPRITE): {
		saveResult = SaveMetaspriteToFile(pFile, metadata, pData);
		break;
	}
	case (ASSET_TYPE_TILESET): {
		saveResult = SaveTilesetToFile(pFile, metadata, pData);
		break;
	}
	case (ASSET_TYPE_ANIMATION): {
		saveResult = SaveAnimationToFile(pFile, metadata, pData);
		break;
	}
	case (ASSET_TYPE_ACTOR_PROTOTYPE): {
		saveResult = SaveActorPrototypeToFile(pFile, metadata, pData);
		break;
	}
	case (ASSET_TYPE_ROOM_TEMPLATE): {
		saveResult = SaveRoomTemplateToFile(pFile, metadata, pData);
		break;
	}
	case (ASSET_TYPE_OVERWORLD): {
		saveResult = SaveOverworldToFile(pFile, metadata, pData);
		break;
	}
	case (ASSET_TYPE_DUNGEON): {
		saveResult = SaveDungeonToFile(pFile, metadata, pData);
		break;
	}
	default:
		saveResult = SERIALIZATION_INVALID_ASSET_TYPE;
		break;
	}

	fclose(pFile);
	return saveResult;
}