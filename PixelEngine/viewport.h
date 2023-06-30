#pragma once
#include "typedef.h"
#include "rendering.h"

struct Level;

struct Viewport {
    f32 x;
    f32 y;
    f32 w;
    f32 h;
};

void MoveViewport(Viewport *viewport, Rendering::RenderContext* pRenderContext, Level* pLevel, f32 dx, f32 dy);