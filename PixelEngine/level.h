#pragma once
#include "rendering.h"
#include "tileset.h"

#define LEVEL_SCREEN_WIDTH_TILES NAMETABLE_WIDTH_TILES
#define LEVEL_SCREEN_WIDTH_METATILES (LEVEL_SCREEN_WIDTH_TILES / Tileset::metatileWorldSize)

#define LEVEL_SCREEN_HEIGHT_TILES NAMETABLE_HEIGHT_TILES
#define LEVEL_SCREEN_HEIGHT_METATILES (LEVEL_SCREEN_HEIGHT_TILES / Tileset::metatileWorldSize)

struct Screen {
    u32 metatiles[LEVEL_SCREEN_WIDTH_METATILES * LEVEL_SCREEN_HEIGHT_METATILES];
};

struct Level {
    const char* name;
    u32 screenCount;
    Screen* screens;
};

void LoadLevel(Level* pLevel, const char* fname);
void SaveLevel(Level* pLevel, const char* fname);