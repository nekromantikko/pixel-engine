#pragma once
#include "typedef.h"
#include "asset_types.h"

constexpr u32 ANIMATION_MAX_FRAME_COUNT = 64;

struct AnimationFrame {
	MetaspriteHandle metaspriteId;
};

struct Animation {
	u8 frameLength;
	s16 loopPoint;
	u16 frameCount;
	AnimationFrame frames[ANIMATION_MAX_FRAME_COUNT];
};