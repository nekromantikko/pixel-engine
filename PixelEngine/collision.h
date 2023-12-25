#pragma once
#include "typedef.h"
#include "vector.h"
#include "rendering.h"
#include "tileset.h"

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

    u8 GetTileWorldSpace(Rendering::RenderContext* pRenderContext, IVec2 tileCoord);

    void RaycastTilesWorldSpace(Rendering::RenderContext* pRenderContext, Vec2 start, Vec2 dir, r32 maxLength, HitResult& outHitResult);

    void SweepBoxHorizontal(Rendering::RenderContext* pRenderContext, Vec2 pos, Vec2 dimensions, r32 dx, HitResult& outHit);
    void SweepBoxVertical(Rendering::RenderContext* pRenderContext, Vec2 pos, Vec2 dimensions, r32 dy, HitResult& outHit);
}