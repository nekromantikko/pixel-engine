#pragma once
#include "rendering.h"
#include "tileset.h"

#define LEVEL_SCREEN_WIDTH_TILES NAMETABLE_WIDTH_TILES
#define LEVEL_SCREEN_WIDTH_METATILES (LEVEL_SCREEN_WIDTH_TILES / Tileset::metatileWorldSize)

#define LEVEL_SCREEN_HEIGHT_TILES NAMETABLE_HEIGHT_TILES
#define LEVEL_SCREEN_HEIGHT_METATILES (LEVEL_SCREEN_HEIGHT_TILES / Tileset::metatileWorldSize)

enum ActorType : u8 {
    ACTOR_NONE = 0,
    ACTOR_PLAYER_START = 1,

};

struct LevelTile {
    u8 metatile;
    ActorType actorType;
    u8 unused1;
    u8 unused2;
};

struct Screen {
    LevelTile tiles[LEVEL_SCREEN_WIDTH_METATILES * LEVEL_SCREEN_HEIGHT_METATILES];
};

struct Level {
    const char* name;
    u32 screenCount;
    Screen* screens;
};

void LoadLevel(Level* pLevel, const char* fname);
void SaveLevel(Level* pLevel, const char* fname);