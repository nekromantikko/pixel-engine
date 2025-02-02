#pragma once
#include "typedef.h"
#include "vector.h"
#include "rendering.h"
#include "tileset.h"

#ifdef EDITOR
constexpr u32 COLLIDER_TYPE_COUNT = 4;
constexpr const char* COLLIDER_TYPE_NAMES[COLLIDER_TYPE_COUNT] = { "Point", "Box", "Circle", "Capsule" };
#endif

namespace Level {
    struct Level;
}

namespace Collision {

    enum ColliderType : u32 {
        ColliderPoint = 0,
        ColliderBox = 1,
        ColliderCircle = 2,
        ColliderCapsule = 3,
    };

    struct Collider {
        ColliderType type;
        r32 width, height; // Width doubles as radius*2 for circle & capsule
        r32 xOffset, yOffset;
    };

    // Blatant plagiarism from unreal engine
    struct HitResult {
        bool32 blockingHit;
        bool32 startPenetrating;
        r32 distance;
        Vec2 impactNormal;
        Vec2 impactPoint;
        Vec2 location;
        Vec2 normal;
        Tileset::TileType tileType;
    };

    u32 GetMetatileIndex(Level::Level* pLevel, IVec2 metatileCoord);

    void SweepBoxHorizontal(Level::Level* pLevel, Vec2 pos, Vec2 dimensions, r32 dx, HitResult& outHit);
    void SweepBoxVertical(Level::Level* pLevel, Vec2 pos, Vec2 dimensions, r32 dy, HitResult& outHit);
}