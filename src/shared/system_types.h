#pragma once
#include "typedef.h"
#include "asset_types.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>

// =================
// ANIMATION TYPES
// =================
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

// =================
// AUDIO TYPES
// =================
enum SoundChannelId {
    CHAN_ID_PULSE0 = 0,
    CHAN_ID_PULSE1,
    CHAN_ID_TRIANGLE,
    CHAN_ID_NOISE,
    //CHAN_ID_DPCM,

    CHAN_COUNT
};

struct SoundOperation {
    u8 opCode : 4;
    u8 address : 4;
    u8 data;
};

enum SoundType {
    SOUND_TYPE_SFX = 0,
    SOUND_TYPE_MUSIC,

    SOUND_TYPE_COUNT
};

struct Sound {
    u32 length;
    u32 loopPoint;
    u16 type;
    u16 sfxChannel;
    u32 dataOffset;

    inline SoundOperation* GetData() const {
        return (SoundOperation*)((u8*)this + dataOffset);
    }
};

// =================
// COLLISION TYPES
// =================
struct AABB {
    union {
        struct {
            r32 x1, y1;
        };
        glm::vec2 min;
    };
    union {
        struct {
            r32 x2, y2;
        };
        glm::vec2 max;
    };

    AABB() : min{}, max{} {}
    AABB(r32 x1, r32 x2, r32 y1, r32 y2) : x1(x1), x2(x2), y1(y1), y2(y2) {}
    AABB(const glm::vec2& min, const glm::vec2& max) : min(min), max(max) {}
};

// Blatant plagiarism from unreal engine
struct HitResult {
    bool32 blockingHit;
    bool32 startPenetrating;
    r32 distance;
    glm::vec2 impactNormal;
    glm::vec2 impactPoint;
    glm::vec2 location;
    glm::vec2 normal;
    u32 tileType;
};