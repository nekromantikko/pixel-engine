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

#ifdef EDITOR
namespace Assets {
	u32 GetAnimationSize(const Animation* pHeader = nullptr);
	void InitAnimation(u64 id, void* data);
}
#endif