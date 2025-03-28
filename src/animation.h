#pragma once
#include "typedef.h"

constexpr u32 ANIMATION_MAX_FRAME_COUNT = 64;

struct AnimationFrame {
	u64 metaspriteId;
};

struct AnimationNew {
	u8 frameLength;
	s16 loopPoint;
	u16 frameCount;
	AnimationFrame frames[ANIMATION_MAX_FRAME_COUNT];
};