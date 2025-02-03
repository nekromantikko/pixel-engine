#pragma once
#include "typedef.h"
#include "vector.h"
#include "rendering.h"

#ifdef EDITOR
constexpr u32 COLLIDER_TYPE_COUNT = 4;
constexpr const char* COLLIDER_TYPE_NAMES[COLLIDER_TYPE_COUNT] = { "Point", "Box", "Circle", "Capsule" };
#endif

struct Tilemap;

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
        u32 tileType;
    };

    u32 GetMetatileIndex(const Tilemap *pTilemap, IVec2 metatileCoord);

    void SweepBoxHorizontal(const Tilemap* pTilemap, Vec2 pos, Vec2 dimensions, r32 dx, HitResult& outHit);
    void SweepBoxVertical(const Tilemap* pTilemap, Vec2 pos, Vec2 dimensions, r32 dy, HitResult& outHit);
}