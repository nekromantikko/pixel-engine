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

void MoveViewport(Viewport* pViewport, Rendering::Nametable* pNametables, const Level::Level* pLevel, r32 dx, r32 dy, bool loadTiles = true);
void RefreshViewport(Viewport* pViewport, Rendering::Nametable* pNametables, const Level::Level* pLevel);