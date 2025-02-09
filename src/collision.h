#pragma once
#include "typedef.h"
#include "vector.h"
#include "rendering.h"

struct Tilemap;

union AABB {
    struct {
        r32 x1, y1, x2, y2;
    };
    struct {
        Vec2 min;
        Vec2 max;
    };

    AABB() : min{}, max{} {}
    AABB(r32 x1, r32 x2, r32 y1, r32 y2) : x1(x1), x2(x2), y1(y2), y2(y2) {}
    AABB(Vec2 min, Vec2 max) : min(min), max(max) {}
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

namespace Collision {
    void SweepBoxHorizontal(const Tilemap* pTilemap, const AABB& box, const Vec2& pos, r32 dx, HitResult& outHit);
    void SweepBoxVertical(const Tilemap* pTilemap, const AABB& box, const Vec2& pos, r32 dy, HitResult& outHit);
    bool BoxesOverlap(const AABB& a, const Vec2& aPos, const AABB& b, const Vec2& bPos);
    //bool PointInBounds(const Vec2& p, const Hitbox& b, const Vec2& boxPos);
}