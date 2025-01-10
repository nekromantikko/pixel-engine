#include "tileset.h"
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif
#include "system.h"
#include <stdio.h>

namespace Tileset {
	Tileset tileset;

	Metatile& GetMetatile(u8 index) {
		return tileset.metatiles[index % tilesetMetatileCount];
	}

	TileType& GetTileType(u8 index) {
		return tileset.types[index % tilesetMetatileCount];
	}

	u8& GetAttribute(u8 index) {
		return tileset.attributes[(index % tilesetMetatileCount) / 4];
	}

	s32 GetPalette(u8 index) {
		u8& attribute = GetAttribute(index);
		u8 attribSubIndex = index % 4;
		return (attribute >> attribSubIndex * 2) & 0b11;
	}

	void LoadTileset(const char* fname) {
		FILE* pFile;
		fopen_s(&pFile, fname, "rb");

		if (pFile == NULL) {
			DEBUG_ERROR("Failed to load tile collision file\n");
		}

		const char signature[4]{};
		fread((void*)signature, sizeof(u8), 4, pFile);
		fread((void*)tileset.name, sizeof(char), tilesetMaxNameLength, pFile);
		fread((void*)&tileset.metatiles, sizeof(Metatile), tilesetMetatileCount, pFile);
		fread((void*)&tileset.types, sizeof(TileType), tilesetMetatileCount, pFile);
		fread((void*)&tileset.attributes, sizeof(u8), tilesetAttributeCount, pFile);

		fclose(pFile);
	}

	void SaveTileset(const char* fname) {
		FILE* pFile;
		fopen_s(&pFile, fname, "wb");

		if (pFile == NULL) {
			DEBUG_ERROR("Failed to write tileset file\n");
		}

		const char signature[4] = "TIL";
		fwrite(signature, sizeof(u8), 4, pFile);
		fwrite(tileset.name, sizeof(char), tilesetMaxNameLength, pFile);
		fwrite(&tileset.metatiles, sizeof(Metatile), tilesetMetatileCount, pFile);
		fwrite(&tileset.types, sizeof(TileType), tilesetMetatileCount, pFile);
		fwrite(&tileset.attributes, sizeof(u8), tilesetAttributeCount, pFile);

		fclose(pFile);
	}

	void CopyMetatileToNametable(Rendering::Nametable* pNametable, u16 x, u16 y, u8 metatileIndex) {
		const u16 nametableOffset = NAMETABLE_WIDTH_TILES * y + x;

		Metatile& metatile = GetMetatile(metatileIndex);

		// Update nametable tiles
		memcpy(pNametable->tiles + nametableOffset, metatile.tiles, 2);
		memcpy(pNametable->tiles + nametableOffset + NAMETABLE_WIDTH_TILES, metatile.tiles + 2, 2);

		// Update nametable attributes
		u16 xBlock = x / 4;
		u16 yBlock = y / 4;
		u8 smallBlockOffset = (x % 4 / 2) + (y % 4 / 2) * 2;
		u16 blockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
		u8& blockAttribute = pNametable->attributes[blockIndex];
		blockAttribute &= ~(0b11 << (smallBlockOffset * 2));
		blockAttribute |= (GetPalette(metatileIndex) << (smallBlockOffset * 2));
	}
}