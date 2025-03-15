#include "rendering_util.h"
#include "system.h"
#include "debug.h"
#include <stdio.h>
#include <gtc/constants.hpp>

#pragma region Nametable
static inline u32 GetNametableAttributeIndex(const glm::ivec2& metatileOffset) {
	return (metatileOffset.x >> 1) + (metatileOffset.y >> 1) * NAMETABLE_WIDTH_ATTRIBUTES;
}

static inline u32 GetNametableAttributeIndex(u32 metatileIndex) {
	const glm::ivec2 metatileOffset(metatileIndex & (NAMETABLE_WIDTH_METATILES - 1), metatileIndex >> NAMETABLE_WIDTH_METATILES_LOG2);

	return GetNametableAttributeIndex(metatileOffset);
}

static inline u8 GetNametableAttributeOffset(const glm::ivec2& metatilePos) {
	return (metatilePos.x & 1) + (metatilePos.y & 1) * 2;
}

static inline u8 GetNametableAttributeOffset(u32 metatileIndex) {
	const u32 y = metatileIndex >> NAMETABLE_WIDTH_METATILES_LOG2;

	return (metatileIndex & 1) + (y & 1) * 2;
}

s32 Rendering::Util::GetPalette(u8 attribute, u8 offset) {
	return (s32)((attribute >> (offset * 2)) & 0b11);
}

u8 Rendering::Util::SetPalette(u8 attribute, u8 offset, s32 palette) {
	// Clear bits
	attribute &= ~(0b11 << offset * 2);
	// Set bits
	attribute |= (palette & 0b11) << offset * 2;

	return attribute;
}

s32 Rendering::Util::GetNametableTileIndexFromTileOffset(const glm::ivec2& tileOffset) {
	return tileOffset.x + tileOffset.y * NAMETABLE_WIDTH_TILES;
}

s32 Rendering::Util::GetNametableTileIndexFromMetatileOffset(const glm::ivec2& metatileOffset) {
	return (metatileOffset.x << 1) + (metatileOffset.y << 1) * NAMETABLE_WIDTH_TILES;
}

s32 Rendering::Util::GetNametableIndexFromTilePos(const glm::ivec2& tilePos) {
	return (tilePos.x / NAMETABLE_WIDTH_TILES + tilePos.y / NAMETABLE_HEIGHT_TILES) % NAMETABLE_COUNT;
}

glm::ivec2 Rendering::Util::GetNametableOffsetFromTilePos(const glm::ivec2& tilePos) {
	return { (s32)(tilePos.x % NAMETABLE_WIDTH_TILES), (s32)(tilePos.y % NAMETABLE_HEIGHT_TILES) };
}

s32 Rendering::Util::GetNametableIndexFromMetatilePos(const glm::ivec2& pos) {
	return (pos.x / NAMETABLE_WIDTH_METATILES + pos.y / NAMETABLE_HEIGHT_METATILES) % NAMETABLE_COUNT;
}

glm::ivec2 Rendering::Util::GetNametableOffsetFromMetatilePos(const glm::ivec2& pos) {
	return { (s32)(pos.x % NAMETABLE_WIDTH_METATILES), (s32)(pos.y % NAMETABLE_HEIGHT_METATILES) };
}

void Rendering::Util::SetNametableTile(Nametable* pNametables, const glm::ivec2& tilePos, u8 tileIndex) {
	const u32 nametableIndex = GetNametableIndexFromTilePos(tilePos);
	const glm::ivec2 nametableOffset = GetNametableOffsetFromTilePos(tilePos);
	const u32 nametableTileIndex = GetNametableTileIndexFromTileOffset(nametableOffset);

	pNametables[nametableIndex].tiles[nametableTileIndex] = tileIndex;
}

void Rendering::Util::GetNametableMetatile(const Nametable* pNametable, u32 metatileIndex, Metatile& outMetatile, s32& outPalette) {
	const glm::ivec2 metatileOffset(metatileIndex % NAMETABLE_WIDTH_METATILES, metatileIndex / NAMETABLE_WIDTH_METATILES);

	const u32 firstTileIndex = GetNametableTileIndexFromMetatileOffset(metatileOffset);
	
	outMetatile = {
		pNametable->tiles[firstTileIndex],
		pNametable->tiles[firstTileIndex + 1],
		pNametable->tiles[firstTileIndex + NAMETABLE_WIDTH_TILES],
		pNametable->tiles[firstTileIndex + NAMETABLE_WIDTH_TILES + 1],
	};

	outPalette = GetPalette(pNametable->attributes[GetNametableAttributeIndex(metatileOffset)], GetNametableAttributeOffset(metatileOffset));
}

void Rendering::Util::SetNametableMetatile(Nametable* pNametable, const glm::ivec2& metatileOffset, const Metatile& metatile, const s32 palette) {
	const u32 firstTileIndex = GetNametableTileIndexFromMetatileOffset(metatileOffset);
	const u32 attributeIndex = GetNametableAttributeIndex(metatileOffset);
	const u32 attributeOffset = GetNametableAttributeOffset(metatileOffset);

	pNametable->tiles[firstTileIndex] = metatile.tiles[0];
	pNametable->tiles[firstTileIndex + 1] = metatile.tiles[1];
	pNametable->tiles[firstTileIndex + NAMETABLE_WIDTH_TILES] = metatile.tiles[2];
	pNametable->tiles[firstTileIndex + NAMETABLE_WIDTH_TILES + 1] = metatile.tiles[3];

	u8& attribute = pNametable->attributes[attributeIndex];
	attribute = SetPalette(attribute, attributeOffset, palette);
}

void Rendering::Util::ClearNametable(Nametable* pNametable, u8 tile, s32 palette) {
	const u8 attribute = (palette & 0b11) | ((palette & 0b11) << 2) | ((palette & 0b11) << 4) | ((palette & 0b11) << 6);
	memset(pNametable->tiles, tile, NAMETABLE_SIZE_TILES);
	memset(pNametable->attributes, attribute, NAMETABLE_ATTRIBUTE_COUNT);
}
#pragma endregion

namespace Rendering
{
	namespace Util
	{
		struct RIFFHeader {
			char signature[4]; // Should be 'RIFF'
			u32 size;
			char type[4];
		};
		struct PaletteChunkHeader {
			char signature[4];
			u32 size;
			u16 version;
			u16 colorCount;
		};

		void CreateChrSheet(const char* fname, ChrSheet* outSheet) {
			u32 imgWidth, imgHeight;
			u16 bpp;
			char* imgData = LoadBitmapBytes(fname, imgWidth, imgHeight, bpp);

			if (imgWidth != CHR_DIM_PIXELS || imgHeight != CHR_DIM_PIXELS) {
				DEBUG_ERROR("Invalid chr image dimensions!\n");
			}

			if (bpp != 8) {
				DEBUG_ERROR("Invalid chr image format!\n");
			}

			for (u32 y = 0; y < imgHeight; y++) {
				for (u32 x = 0; x < imgWidth; x++) {
					u32 coarseX = x / 8;
					u32 coarseY = y / 8;
					u32 fineX = x % 8;
					u32 fineY = y % 8;
					u32 tileIndex = (15 - coarseY) * 16 + coarseX; // Tile 0 is top left instead of bottom left
					u32 inPixelIndex = y * imgWidth + x;
					u32 outPixelIndex = (7 - fineY) * 8 + fineX; // Also pixels go from top to bottom in this program, but bottom to top in bmp, so flip

					u8 pixel = imgData[inPixelIndex];

					ChrTile& tile = outSheet->tiles[tileIndex];
					tile.p0 = (tile.p0 & ~(1ULL << outPixelIndex)) | ((u64)(pixel & 0b00000001) << outPixelIndex);
					tile.p1 = (tile.p1 & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000010) >> 1) << outPixelIndex);
					tile.p2 = (tile.p2 & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000100) >> 2) << outPixelIndex);
				}
			}

			free(imgData);
		}

		void LoadPaletteColorsFromFile(const char* fname, u8* outColors) {
			u32 palFileSize;
			char* palData = AllocFileBytes(fname, palFileSize);

			if (palFileSize < PALETTE_MEMORY_SIZE) {
				DEBUG_ERROR("Invalid palette table file!\n");
			}

			memcpy(outColors, palData, PALETTE_MEMORY_SIZE);
			free(palData);
		}

		void GeneratePaletteColors(u32* data) {
			for (s32 i = 0; i < COLOR_COUNT; i++) {
				s32 hue = i & 0b1111;
				s32 brightness = (i & 0b1110000) >> 4;

				r32 y = (r32)brightness / 7;
				r32 u = 0.0f; 
				r32 v = 0.0f;

				if (hue != 0) {
					// No need to have multiple pure blacks and whites
					y = (r32)(brightness + 1) / 9;

					r32 angle = 2 * glm::pi<r32>() * (hue - 1) / 15;
					r32 radius = 0.5f * (1.0f - glm::abs(y - 0.5f) * 2);

					u = radius * glm::cos(angle);
					v = radius * glm::sin(angle);
				}

				// Convert YUV to RGB
				float r = y + v * 1.139883;
				float g = y - 0.394642 * u - 0.580622 * v;
				float b = y + u * 2.032062;

				r = glm::clamp(r, 0.0f, 1.0f);
				g = glm::clamp(g, 0.0f, 1.0f);
				b = glm::clamp(b, 0.0f, 1.0f);

				u32* pixel = data + i;
				u8* pixelBytes = (u8*)pixel;

				pixelBytes[0] = (u8)(r * 255);
				pixelBytes[1] = (u8)(g * 255);
				pixelBytes[2] = (u8)(b * 255);
				pixelBytes[3] = 0;
			}
		}

		void SavePaletteToFile(const char* fname) {
			u32 data[COLOR_COUNT];
			GeneratePaletteColors(data);
			const u32 dataSize = COLOR_COUNT * sizeof(u32);

			FILE* pFile;
			fopen_s(&pFile, fname, "wb");

			if (pFile == NULL) {
				DEBUG_ERROR("Failed to write palette file\n");
			}

			RIFFHeader header{};
			header.signature[0] = 'R';
			header.signature[1] = 'I';
			header.signature[2] = 'F';
			header.signature[3] = 'F';

			header.type[0] = 'P';
			header.type[1] = 'A';
			header.type[2] = 'L';
			header.type[3] = ' ';

			header.size = dataSize + sizeof(PaletteChunkHeader);

			fwrite(&header, sizeof(RIFFHeader), 1, pFile);

			PaletteChunkHeader chunk{};
			chunk.signature[0] = 'd';
			chunk.signature[1] = 'a';
			chunk.signature[2] = 't';
			chunk.signature[3] = 'a';
			
			chunk.size = dataSize;
			chunk.version = 0x0300;
			chunk.colorCount = COLOR_COUNT;

			fwrite(&chunk, sizeof(PaletteChunkHeader), 1, pFile);
			fwrite(data, dataSize, 1, pFile);

			fclose(pFile);
		}

		// Sign-extend 9-bit unsigned sprite position
		s16 SignExtendSpritePos(u16 spritePos) {
			return (spritePos ^ 0x100) - 0x100;
		}
	}
}