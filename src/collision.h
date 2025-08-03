#pragma once
#include "collision_types.h"

struct Tilemap;

namespace Collision {
    void SweepBoxHorizontal(const Tilemap* pTilemap, const AABB& box, const glm::vec2& pos, r32 dx, HitResult& outHit);
    void SweepBoxVertical(const Tilemap* pTilemap, const AABB& box, const glm::vec2& pos, r32 dy, HitResult& outHit);
    bool BoxesOverlap(const AABB& a, const glm::vec2& aPos, const AABB& b, const glm::vec2& bPos);
    bool PointInsideBox(const glm::vec2& point, const AABB& bounds, const glm::vec2& boxPos);
}