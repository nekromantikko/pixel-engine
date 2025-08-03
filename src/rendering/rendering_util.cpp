#include "rendering_util.h"
#include "debug.h"
#include <stdio.h>
#include <gtc/constants.hpp>

#pragma region Nametable
s32 Rendering::Util::GetNametableTileIndexFromTileOffset(const glm::ivec2& tileOffset) {
	return tileOffset.x + tileOffset.y * NAMETABLE_DIM_TILES;
}

s32 Rendering::Util::GetNametableTileIndexFromMetatileOffset(const glm::ivec2& metatileOffset) {
	return (metatileOffset.x << 1) + (metatileOffset.y << 1) * NAMETABLE_DIM_TILES;
}

s32 Rendering::Util::GetNametableIndexFromTilePos(const glm::ivec2& tilePos) {
	return (tilePos.x / NAMETABLE_DIM_TILES + tilePos.y / NAMETABLE_DIM_TILES) % NAMETABLE_COUNT;
}

glm::ivec2 Rendering::Util::GetNametableOffsetFromTilePos(const glm::ivec2& tilePos) {
	return { (s32)(tilePos.x % NAMETABLE_DIM_TILES), (s32)(tilePos.y % NAMETABLE_DIM_TILES) };
}

s32 Rendering::Util::GetNametableIndexFromMetatilePos(const glm::ivec2& pos) {
	return (pos.x / NAMETABLE_DIM_METATILES + pos.y / NAMETABLE_DIM_METATILES) % NAMETABLE_COUNT;
}

glm::ivec2 Rendering::Util::GetNametableOffsetFromMetatilePos(const glm::ivec2& pos) {
	return { (s32)(pos.x % NAMETABLE_DIM_METATILES), (s32)(pos.y % NAMETABLE_DIM_METATILES) };
}

void Rendering::Util::SetNametableTile(Nametable* pNametables, const glm::ivec2& tilePos, BgTile tileIndex) {
	const u32 nametableIndex = GetNametableIndexFromTilePos(tilePos);
	const glm::ivec2 nametableOffset = GetNametableOffsetFromTilePos(tilePos);
	const u32 nametableTileIndex = GetNametableTileIndexFromTileOffset(nametableOffset);

	pNametables[nametableIndex].tiles[nametableTileIndex] = tileIndex;
}

Metatile Rendering::Util::GetNametableMetatile(const Nametable* pNametable, u32 metatileIndex) {
	const glm::ivec2 metatileOffset(metatileIndex % NAMETABLE_DIM_METATILES, metatileIndex / NAMETABLE_DIM_METATILES);

	const u32 firstTileIndex = GetNametableTileIndexFromMetatileOffset(metatileOffset);
	
	return {
		pNametable->tiles[firstTileIndex],
		pNametable->tiles[firstTileIndex + 1],
		pNametable->tiles[firstTileIndex + NAMETABLE_DIM_TILES],
		pNametable->tiles[firstTileIndex + NAMETABLE_DIM_TILES + 1],
	};
}

void Rendering::Util::SetNametableMetatile(Nametable* pNametable, const glm::ivec2& metatileOffset, const Metatile& metatile) {
	const u32 firstTileIndex = GetNametableTileIndexFromMetatileOffset(metatileOffset);

	pNametable->tiles[firstTileIndex] = metatile.tiles[0];
	pNametable->tiles[firstTileIndex + 1] = metatile.tiles[1];
	pNametable->tiles[firstTileIndex + NAMETABLE_DIM_TILES] = metatile.tiles[2];
	pNametable->tiles[firstTileIndex + NAMETABLE_DIM_TILES + 1] = metatile.tiles[3];
}

void Rendering::Util::ClearNametable(Nametable* pNametable, BgTile tile) {
	for (u32 i = 0; i < NAMETABLE_SIZE_TILES; i++) {
		pNametable->tiles[i] = tile;
	}
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
				pixelBytes[3] = 255;
			}
		}

		void SavePaletteToFile(const char* fname) {
			u32 data[COLOR_COUNT];
			GeneratePaletteColors(data);
			const u32 dataSize = COLOR_COUNT * sizeof(u32);

			FILE* pFile;
			pFile = fopen(fname, "wb");

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
	}
}