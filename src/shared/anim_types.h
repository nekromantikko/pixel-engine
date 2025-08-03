#pragma once
#include "typedef.h"

struct AnimationFrame {
	MetaspriteHandle metaspriteId;
};

struct Animation {
	u8 frameLength;
	s16 loopPoint;
	u16 frameCount;
	u32 framesOffset;

	inline AnimationFrame* GetFrames() const {
		return (AnimationFrame*)((u8*)this + framesOffset);
	}
};