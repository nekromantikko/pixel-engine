#include "collision.h"
#include "rendering.h"
#include <stdio.h>
#include "system.h"
#include "math.h"
#include "level.h"

namespace Collision {
	void SweepBoxHorizontal(const Tilemap* pTilemap, const AABB& hitbox, const Vec2& pos, r32 dx, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = abs(dx);
		outHit.location = Vec2{ pos.x + dx, pos.y };

		if (pTilemap == nullptr) {
			return;
		}

		if (IsNearlyZero(dx)) {
			return;
		}

		const AABB hitboxAbs(hitbox.min + pos, hitbox.max + pos);
		r32 xSide = dx < 0.0f ? hitboxAbs.x1 : hitboxAbs.x2;

		s32 yTopTile = (s32)floorf(hitboxAbs.y1);
		s32 yBottomTile = (s32)floorf(hitboxAbs.y2);
		// Right at the seam, should look at one tile above
		if (IsNearlyZero(hitboxAbs.y2 - (r32)yBottomTile))
			yBottomTile--;
		s32 yTileDelta = yBottomTile - yTopTile;
		s32 xTile = (s32)floorf(xSide);

		r32 dist = 0.0f;

		while (!outHit.blockingHit && dist < fabs(dx)) {
			for (s32 i = 0; i <= yTileDelta; i++) {
				IVec2 tileCoord = IVec2{ xTile, yTopTile + i };

				const MapTile* tile = Tiles::GetMapTile(pTilemap, tileCoord);

				// Treat outside of screen as solid wall
				if (!tile || tile->type == TILE_SOLID) {
					outHit.blockingHit = true;
					outHit.startPenetrating = IsNearlyZero(dist);
					outHit.distance = dist;
					outHit.impactNormal = Vec2{ -Sign(dx), 0 };
					outHit.impactPoint = Vec2{ xSide + Sign(dx) * dist, pos.y };
					outHit.location = Vec2{ pos.x + Sign(dx) * dist, pos.y };
					outHit.normal = Vec2{ Sign(dx), 0 };
					outHit.tileType = tile ? tile->type: TILE_SOLID;

					break;
				}
			}

			r32 distToNextTile = dx < 0.0f ? xSide - xTile : xTile + 1 - xSide;
			dist += distToNextTile;
			xSide += Sign(dx) * distToNextTile;
			xTile += (s32)Sign(dx);
		}
	}

	void SweepBoxVertical(const Tilemap *pTilemap, const AABB& hitbox, const Vec2& pos, r32 dy, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = abs(dy);
		outHit.location = Vec2{ pos.x, pos.y + dy };

		if (pTilemap == nullptr) {
			return;
		}

		if (IsNearlyZero(dy)) {
			return;
		}

		const AABB hitboxAbs(hitbox.min + pos, hitbox.max + pos);
		r32 ySide = dy < 0.0f ? hitboxAbs.y1 : hitboxAbs.y2;

		s32 xLeftTile = (s32)floorf(hitboxAbs.x1);
		s32 xRightTile = (s32)floorf(hitboxAbs.x2);
		// Right at the seam, should look at one tile left
		if (IsNearlyZero(hitboxAbs.x2 - (r32)xRightTile))
			xRightTile--;
		s32 xTileDelta = xRightTile - xLeftTile;
		s32 yTile = (s32)floorf(ySide);

		r32 dist = 0.0f;

		while (!outHit.blockingHit && dist < fabs(dy)) {
			for (s32 i = 0; i <= xTileDelta; i++) {
				IVec2 metatileCoord = IVec2{ xLeftTile + i, yTile };

				const MapTile* tile = Tiles::GetMapTile(pTilemap, metatileCoord);
				
				// Treat outside of screen as solid wall
				if (!tile || tile->type == TILE_SOLID) {
					outHit.blockingHit = true;
					outHit.startPenetrating = IsNearlyZero(dist);
					outHit.distance = dist;
					outHit.impactNormal = Vec2{ 0, -Sign(dy) };
					outHit.impactPoint = Vec2{ pos.x, yTile + Sign(dy) * dist };
					outHit.location = Vec2{ pos.x, pos.y + Sign(dy) * dist };
					outHit.normal = Vec2{ 0, Sign(dy) };
					outHit.tileType = tile ? tile->type : TILE_SOLID;

					break;
				}
			}

			r32 distToNextTile = dy < 0.0f ? ySide - yTile : yTile + 1 - ySide;
			dist += distToNextTile;
			ySide += Sign(dy) * distToNextTile;
			yTile += (s32)Sign(dy);
		}
	}

	bool BoxesOverlap(const AABB& a, const Vec2& aPos, const AABB& b, const Vec2& bPos) {
		const AABB aAbs = AABB(a.min + aPos, a.max + aPos);
		const AABB bAbs = AABB(b.min + bPos, b.max + bPos);

		return (aAbs.x1 < bAbs.x2 &&
			aAbs.x2 >= bAbs.x1 &&
			aAbs.y1 < bAbs.y2 &&
			aAbs.y2 >= bAbs.y1);
	}

	bool PointInsideBox(const Vec2& point, const AABB& hitbox, const Vec2& boxPos) {
		const AABB hitboxAbs = AABB(hitbox.min + boxPos, hitbox.max + boxPos);

		return (point.x >= hitboxAbs.x1 &&
			point.x < hitboxAbs.x2 &&
			point.y >= hitboxAbs.y1 &&
			point.y < hitboxAbs.y2);
	}
}