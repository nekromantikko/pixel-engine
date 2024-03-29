#pragma once
#include "typedef.h"
#include "rendering.h"

struct Level;

struct Viewport {
    r32 x;
    r32 y;
    r32 w;
    r32 h;
};

void MoveViewport(Viewport *viewport, Rendering::RenderContext* pRenderContext, Level* pLevel, r32 dx, r32 dy);