#include "tileset.h"
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif
#include "system.h"
#include <stdio.h>

namespace Tileset {
	Tileset tileset;

	Metatile& GetMetatile(u32 index) {
		return tileset.metatiles[index];
	}

	TileType& GetTileType(u32 index) {
		return tileset.types[index];
	}

	u8& GetAttribute(u32 index) {
		return tileset.attributes[index / 4];
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
}