#pragma once
#include "typedef.h"
#include "vector.h"
#include "rendering.h"

namespace Collision {

    enum TileCollisionType : u32 {
        TileEmpty = 0,
        TileSolid = 1,
        TileSlope = 2,
        TileJumpThrough = 3,
        TileSlopeFlip = 4
    };

    struct TileCollision {
        TileCollisionType type;
        Vec2 slopeStart;
        Vec2 slopeEnd;
    };

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
        TileCollisionType tileType;
    };

    TileCollision* GetBgCollisionPtr();

    // TODO: Move somewhere?
    void LoadBgCollision(const char* fname);
    void SaveBgCollision(const char* fname);

    r32 GetTileSlope(TileCollision tile);
    r32 GetTileSurfaceX(TileCollision tile, r32 y);
    r32 GetTileSurfaceY(TileCollision tile, r32 x);
    u8 GetTileWorldSpace(Rendering::RenderContext* pRenderContext, IVec2 tileCoord);

    bool PointInsideTile(TileCollision tile, Vec2 p);
    void RaycastTilesWorldSpace(Rendering::RenderContext* pRenderContext, Vec2 start, Vec2 dir, r32 maxLength, HitResult& outHitResult);

    void SweepBoxHorizontal(Rendering::RenderContext* pRenderContext, Vec2 pos, Vec2 dimensions, r32 dx, HitResult& outHit);
    void SweepBoxVertical(Rendering::RenderContext* pRenderContext, Vec2 pos, Vec2 dimensions, r32 dy, HitResult& outHit);
}