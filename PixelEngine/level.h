#pragma once
#include "rendering.h"

struct Screen {
    // Size of one nametable
    u8 tiles[NAMETABLE_ATTRIBUTE_OFFSET]{};
    u8 attributes[NAMETABLE_ATTRIBUTE_SIZE]{};
};

struct Level {
    const char* name;
    u32 screenCount;
    Screen* screens;
};

void LoadLevel(Level* pLevel, const char* fname);
void SaveLevel(Level* pLevel, const char* fname);