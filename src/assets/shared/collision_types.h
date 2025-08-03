#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include "typedef.h"

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