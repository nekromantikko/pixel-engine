#pragma once
#include "typedef.h"
#include "rendering.h"
#include "vector.h"

struct Tilemap;

struct Viewport {
    r32 x;
    r32 y;
};

void MoveViewport(Viewport* pViewport, Nametable* pNametables, const Tilemap* pTilemap, r32 dx, r32 dy, bool loadTiles = true);
void RefreshViewport(Viewport* pViewport, Nametable* pNametables, const Tilemap* pTilemap);