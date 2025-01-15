#pragma once
#include "typedef.h"
#include "rendering.h"
#include "vector.h"

namespace Level {
    struct Level;
}

struct Viewport {
    r32 x;
    r32 y;
};

void MoveViewport(Viewport *viewport, Rendering::Nametable* pNametable, const Level::Level* const pLevel, r32 dx, r32 dy, bool loadTiles = true);
void RefreshViewport(Viewport* viewport, Rendering::Nametable* pNametable, const Level::Level* const pLevel);
u8 GetMetatileAtNametablePosition(const Level::Level* const pLevel, const Viewport* pViewport, u32 nametableIndex, u32 tileX, u32 tileY);