#include "collision.h"
#include "rendering.h"
#include <stdio.h>
#include "system.h"
#include "math.h"
#include "level.h"

namespace Collision {
	void SweepBoxHorizontal(const Tilemap* pTilemap, Vec2 pos, Vec2 dimensions, r32 dx, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = abs(dx);
		outHit.location = Vec2{ pos.x + dx, pos.y };

		if (pTilemap == nullptr) {
			return;
		}

		if (IsNearlyZero(dx)) {
			return;
		}

		r32 yTop = pos.y - dimensions.y / 2.0f;
		r32 yBottom = pos.y + dimensions.y / 2.0f;
		r32 xSide = dx < 0.0f ? pos.x - dimensions.x / 2.0f : pos.x + dimensions.x / 2.0f;

		s32 yTopTile = (s32)floorf(yTop);
		s32 yBottomTile = (s32)floorf(yBottom);
		// Right at the seam, should look at one tile above
		if (IsNearlyZero(yBottom - (r32)yBottomTile))
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
					outHit.impactPoint = Vec2{ pos.x + Sign(dx) * (dist + (dimensions.x / 2.0f)), pos.y };
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

	void SweepBoxVertical(const Tilemap *pTilemap, Vec2 pos, Vec2 dimensions, r32 dy, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = abs(dy);
		outHit.location = Vec2{ pos.x, pos.y + dy };

		if (pTilemap == nullptr) {
			return;
		}

		if (IsNearlyZero(dy)) {
			return;
		}

		r32 xLeft = pos.x - dimensions.x / 2.0f;
		r32 xRight = pos.x + dimensions.x / 2.0f;
		r32 ySide = dy < 0.0f ? pos.y - dimensions.y / 2.0f : pos.y + dimensions.y / 2.0f;

		s32 xLeftTile = (s32)floorf(xLeft);
		s32 xRightTile = (s32)floorf(xRight);
		// Right at the seam, should look at one tile left
		if (IsNearlyZero(xRight - (r32)xRightTile))
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
					outHit.impactPoint = Vec2{ pos.x, pos.y + Sign(dy) * (dist + (dimensions.y / 2.0f)) };
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
}