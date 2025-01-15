#pragma once
#include "typedef.h"
#include "rendering.h"

namespace Tileset {
	constexpr u32 tilesetMaxNameLength = 256;
	constexpr u32 metatileWorldSize = 2;
	constexpr u32 metatileTileCount = 4;
	constexpr u32 tilesetMetatileCount = 256;
	constexpr u32 tilesetAttributeCount = tilesetMetatileCount / 4;

	enum TileType : u8 {
		TileEmpty = 0,
		TileSolid = 1
	};

	struct Metatile {
		u8 tiles[metatileTileCount];
	};

	struct Tileset {
		char name[tilesetMaxNameLength];
		Metatile metatiles[tilesetMetatileCount];
		TileType types[tilesetMetatileCount];
		u8 attributes[tilesetAttributeCount];
	};

	Metatile& GetMetatile(u8 index);
	TileType& GetTileType(u8 index);
	u8& GetAttribute(u8 index);
	s32 GetPalette(u8 index);
	void LoadTileset(const char* fname);
	void SaveTileset(const char* fname);

	void CopyMetatileToNametable(Rendering::Nametable* pNametable, u16 x, u16 y, u8 metatileIndex);
	void FillAllNametablesWithMetatile(Rendering::Nametable* pNametables, u8 metatileIndex);
};