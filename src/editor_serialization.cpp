#include "editor_serialization.h"
#include <cstdlib>
#include "rendering.h"
#include "audio.h"

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

static bool LoadChrSheetFromFile(const std::filesystem::path& path, const nlohmann::json& metadata, u32& size, void* pOutData) {
	if (!pOutData) {
		size = sizeof(ChrSheet);
		return true; // Just return size if no output data is provided
	}

	if (!std::filesystem::exists(path)) {
		DEBUG_ERROR("File (%s) does not exist\n", path.string().c_str());
		return false;
	}

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		DEBUG_ERROR("Failed to open file\n");
		return false;
	}

	BitmapHeader header{};
	fread(&header, sizeof(BitmapHeader), 1, pFile);
	if (memcmp(header.signature, "BM", 2) != 0) {
		DEBUG_ERROR("Invalid bitmap file\n");
		fclose(pFile);
		return false;
	}

	if (header.info.bpp != 8) {
		DEBUG_ERROR("Bitmap bpp %d is invalid (Should be 8)\n", header.info.bpp);
		fclose(pFile);
		return false;
	}

	if (header.info.width != CHR_DIM_PIXELS || header.info.height != CHR_DIM_PIXELS) {
		DEBUG_ERROR("Bitmap size (%d x %d) is invalid (Should be %d x %d)\n", header.info.width, header.info.height, CHR_DIM_PIXELS, CHR_DIM_PIXELS);
		fclose(pFile);
		return false;
	}

	fseek(pFile, header.dataOffset, SEEK_SET);
	char pixels[CHR_DIM_PIXELS * CHR_DIM_PIXELS];
	fread(pixels, 1, CHR_DIM_PIXELS * CHR_DIM_PIXELS, pFile);

	fclose(pFile);

	CreateChrSheetFromBmp(pixels, (ChrSheet*)pOutData);

	return true;
}

static bool SaveChrSheetToFile(const std::filesystem::path& path, const void* pData) {
	// TODO
	return false;
}

struct NSFHeader {
	char signature[4];
	u32 unused;
	u32 size;
	u32 loopPoint;
};

static void InitSound(u64 id, void* data) {
	Sound newSound{};
	newSound.dataOffset = sizeof(Sound);
	memcpy(data, &newSound, sizeof(Sound));
}

static u32 GetSoundSize(const Sound* pSound) {
	u32 result = sizeof(Sound);
	if (pSound) {
		result += pSound->length * sizeof(SoundOperation);
	}

	return result;
}

static bool LoadSoundFromFile(const std::filesystem::path& path, const nlohmann::json& metadata, u32& size, void* pOutData) {
	if (!std::filesystem::exists(path)) {
		DEBUG_ERROR("File (%s) does not exist\n", path.string().c_str());
		return false;
	}

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		DEBUG_ERROR("Failed to open file\n");
		return false;
	}

	NSFHeader header{};
	fread(&header, sizeof(NSFHeader), 1, pFile);
	if (strcmp(header.signature, "NSF") != 0) {
		DEBUG_ERROR("Invalid NSF file\n");
		fclose(pFile);
		return false;
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
		DEBUG_LOG("Loaded sound data from file\n");
	}

	fclose(pFile);
	return true;
}

static bool SaveSoundToFile(const std::filesystem::path& path, const void* pData) {
	// TODO
	return false;
}

static bool LoadPaletteFromFile(const std::filesystem::path& path, const nlohmann::json& metadata, u32& size, void* pOutData) {
	if (!pOutData) {
		size = sizeof(Palette);
		return true; // Just return size if no output data is provided
	}

	if (!std::filesystem::exists(path)) {
		DEBUG_ERROR("File (%s) does not exist\n", path.string().c_str());
		return false;
	}

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		DEBUG_ERROR("Failed to open file\n");
		return false;
	}

	fread(pOutData, sizeof(Palette), 1, pFile);

	fclose(pFile);
	return true;
}

static bool SavePaletteToFile(const std::filesystem::path& path, const void* pData) {
	// TODO
	return false;
}

static bool LoadMetaspriteFromFile(const std::filesystem::path& path, const nlohmann::json& metadata, u32& size, void* pOutData) {
	if (!std::filesystem::exists(path)) {
		DEBUG_ERROR("File (%s) does not exist\n", path.string().c_str());
		return false;
	}

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		DEBUG_ERROR("Failed to open file\n");
		return false;
	}

	const nlohmann::json json = nlohmann::json::parse(pFile);
	const nlohmann::json spritesJson = json["sprites"];
	u32 spriteCount = spritesJson != nullptr && spritesJson.is_array() ? spritesJson.size() : 0;
	size = sizeof(Metasprite) + spriteCount * sizeof(Sprite);

	if (pOutData) {
		Metasprite* pMetasprite = (Metasprite*)pOutData;
		pMetasprite->spriteCount = spriteCount;
		pMetasprite->spritesOffset = sizeof(Metasprite);

		Sprite* pSprites = pMetasprite->GetSprites();
		for (u32 i = 0; i < spriteCount; i++) {
			spritesJson[i].get_to(pSprites[i]);
		}
	}

	fclose(pFile);
	return true;

}

static bool SaveMetaspriteToFile(const std::filesystem::path& path, const void* pData) {
	// TODO
	return false;
}

#pragma endregion

std::filesystem::path Editor::Assets::GetAssetMetadataPath(const std::filesystem::path& path) {
	// Append ".meta" to the original path to get the metadata file path
	return path.string() + ".meta";
}

bool Editor::Assets::LoadAssetMetadataFromFile(const std::filesystem::path& origPath, nlohmann::json& outJson) {
	std::filesystem::path path = GetAssetMetadataPath(origPath);

	if (!std::filesystem::exists(path)) {
		DEBUG_ERROR("Metadata file (%s) does not exist\n", path.string().c_str());
		return false;
	}

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		DEBUG_ERROR("Failed to open metadata file\n");
		return false;
	}

	try {
		outJson = nlohmann::json::parse(pFile);
	}
	catch (const nlohmann::json::parse_error& e) {
		fclose(pFile);
		DEBUG_ERROR("Failed to parse metadata JSON: %s\n", e.what());
		return false;
	}
	fclose(pFile);

	return true;
}

bool Editor::Assets::SaveAssetMetadataToFile(const std::filesystem::path& origPath, const nlohmann::json& json) {
	std::filesystem::path path = origPath;
	path += ".meta";

	FILE* pFile = fopen(path.string().c_str(), "wb");
	if (!pFile) {
		DEBUG_ERROR("Failed to open file for writing\n");
		return false;
	}

	fwrite(json.dump(4).c_str(), sizeof(char), json.dump(4).size(), pFile);
	fclose(pFile);

	return true;
}

void Editor::Assets::InitializeMetadataJson(nlohmann::json& json, u64 id) {
	json["file_format_version"] = ASSET_FILE_FORMAT_VERSION;
	json["guid"] = id;
}

bool Editor::Assets::LoadSerializedAssetFromFile(const std::filesystem::path& path, nlohmann::json& outJson) {
	if (!std::filesystem::exists(path)) {
		DEBUG_ERROR("File (%s) does not exist\n", path.string().c_str());
		return false;
	}

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		DEBUG_ERROR("Failed to open file\n");
		return false;
	}

	outJson = nlohmann::json::parse(pFile);
	fclose(pFile);

	return true;
}

bool Editor::Assets::SaveSerializedAssetToFile(const std::filesystem::path& path, const nlohmann::json& json, const u64 id) {
	if (!SaveAssetMetadataToFile(path, { ASSET_FILE_FORMAT_VERSION, id })) {
		DEBUG_ERROR("Failed to save metadata for asset\n");
		return false;
	}

	FILE* pFile = fopen(path.string().c_str(), "wb");
	if (!pFile) {
		DEBUG_ERROR("Failed to open file for writing\n");
		return false;
	}

	fwrite(json.dump(4).c_str(), sizeof(char), json.dump(4).size(), pFile);
	fclose(pFile);

	return true;
}

bool Editor::Assets::LoadAssetFromFile(const std::filesystem::path& path, AssetType type, const nlohmann::json& metadata, u32& size, void* pOutData) {
	switch (type) {
	case (ASSET_TYPE_CHR_BANK): {
		return LoadChrSheetFromFile(path, metadata, size, pOutData);
	}
	case (ASSET_TYPE_SOUND): {
		return LoadSoundFromFile(path, metadata, size, pOutData);
	}
	case (ASSET_TYPE_PALETTE): {
		return LoadPaletteFromFile(path, metadata, size, pOutData);
	}
	case (ASSET_TYPE_METASPRITE): {
		return LoadMetaspriteFromFile(path, metadata, size, pOutData);
	}
	default:
		DEBUG_ERROR("Unsupported asset type for loading: %s\n", ASSET_TYPE_NAMES[type]);
		return false;
	}
}
bool Editor::Assets::SaveAssetToFile(const std::filesystem::path& path, AssetType type, const void* pData) {
	switch (type) {
	case (ASSET_TYPE_CHR_BANK): {
		return SaveChrSheetToFile(path, pData);
	}
	case (ASSET_TYPE_SOUND): {
		return SaveSoundToFile(path, pData);
	}
	case (ASSET_TYPE_PALETTE): {
		return SavePaletteToFile(path, pData);
	}
	case (ASSET_TYPE_METASPRITE): {
		return SaveMetaspriteToFile(path, pData);
	}
	default:
		DEBUG_ERROR("Unsupported asset type for loading: %s\n", ASSET_TYPE_NAMES[type]);
		return false;
	}
}