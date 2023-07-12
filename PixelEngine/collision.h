#pragma once
#include "typedef.h"

namespace Collision {

    enum TileCollisionType : u32 {
        TileEmpty = 0,
        TileSolid = 1,
        TilePassThrough = 2,
        TileJumpThrough = 3,
        TilePassThroughFlip = 4
    };

    struct TileCollision {
        TileCollisionType type;
        r32 slope;
        r32 slopeHeight;
    };

    enum ColliderType : u32 {
        ColliderPoint = 0,
        ColliderBox = 1,
        ColliderCircle = 2,
        ColliderCapsule = 3,
    };

    struct Collider {
        ColliderType type;
        f32 width, height; // Width doubles as radius*2 for circle & capsule
        f32 xOffset, yOffset;
    };

    TileCollision* GetBgCollisionPtr();

    // TODO: Move somewhere?
    void LoadBgCollision(const char* fname);
    void SaveBgCollision(const char* fname);
}