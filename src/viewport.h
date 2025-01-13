#pragma once
#include "typedef.h"
#include "rendering.h"

namespace Level {
    struct Level;
}

struct Viewport {
    r32 x;
    r32 y;
    r32 w;
    r32 h;
};

void MoveViewport(Viewport *viewport, Rendering::Nametable* pNametable, const Level::Level* const pLevel, r32 dx, r32 dy);
void RefreshViewport(Viewport* viewport, Rendering::Nametable* pNametable, const Level::Level* const pLevel);