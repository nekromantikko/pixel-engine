#pragma once
#include "typedef.h"
#include "vector.h"
#include "rendering.h"

struct Tilemap;

struct Hitbox {
    Vec2 dimensions;
    Vec2 offset;
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
    void SweepBoxHorizontal(const Tilemap* pTilemap, Vec2 pos, Vec2 dimensions, r32 dx, HitResult& outHit);
    void SweepBoxVertical(const Tilemap* pTilemap, Vec2 pos, Vec2 dimensions, r32 dy, HitResult& outHit);
    bool BoxesOverlap(const Hitbox& a, const Hitbox& b, const Vec2& aPos, const Vec2& bPos);
}