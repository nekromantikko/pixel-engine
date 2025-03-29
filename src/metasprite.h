#pragma once
#include "rendering.h"

constexpr u32 METASPRITE_MAX_SPRITE_COUNT = 64;

struct Metasprite {
	u32 spriteCount;
	Sprite spritesRelativePos[METASPRITE_MAX_SPRITE_COUNT];
};