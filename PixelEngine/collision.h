#pragma once
#include "typedef.h"

namespace Collision {

    enum TileCollisionType : u8 {
        TileEmpty = 0,
        TileSolid = 1,
        TilePassThrough = 2,
        TileJumpThrough = 3,
        TilePassThroughFlip = 4
    };

    struct TileCollision {
        TileCollisionType type = TileEmpty;
        r32 slope = 0.0f;
        r32 slopeHeight = 0.0f;
    };

    TileCollision* GetBgCollisionPtr();
}