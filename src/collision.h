#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include "typedef.h"
#include "rendering.h"

struct Tilemap;

union AABB {
    struct {
        r32 x1, y1, x2, y2;
    };
    struct {
        glm::vec2 min;
        glm::vec2 max;
    };

    AABB() : min{}, max{} {}
    AABB(r32 x1, r32 x2, r32 y1, r32 y2) : x1(x1), x2(x2), y1(y2), y2(y2) {}
    AABB(glm::vec2 min, glm::vec2 max) : min(min), max(max) {}
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

namespace Collision {
    void SweepBoxHorizontal(const Tilemap* pTilemap, const AABB& box, const glm::vec2& pos, r32 dx, HitResult& outHit);
    void SweepBoxVertical(const Tilemap* pTilemap, const AABB& box, const glm::vec2& pos, r32 dy, HitResult& outHit);
    bool BoxesOverlap(const AABB& a, const glm::vec2& aPos, const AABB& b, const glm::vec2& bPos);
    bool PointInsideBox(const glm::vec2& point, const AABB& bounds, const glm::vec2& boxPos);
}