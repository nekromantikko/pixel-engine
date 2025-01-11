#include "collision.h"
#include "rendering.h"
#include <stdio.h>
#include "system.h"
#include "math.h"
#include "level.h"

namespace Collision {
	u32 GetMetatileIndex(Level* pLevel, IVec2 tilemapCoord) {
		u32 screenIndex = TilemapToScreenIndex(pLevel, tilemapCoord);

		if (screenIndex >= pLevel->screenCount) {
			return 0;
		}

		u32 screenTileIndex = TilemapToMetatileIndex(tilemapCoord);

		return pLevel->screens[screenIndex].tiles[screenTileIndex].metatile;
	}

	void SweepBoxHorizontal(Level* pLevel, Vec2 pos, Vec2 dimensions, r32 dx, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = abs(dx);
		outHit.location = Vec2{ pos.x + dx, pos.y };

		if (IsNearlyZero(dx)) {
			return;
		}

		r32 yTop = pos.y - dimensions.y / 2.0f;
		r32 yBottom = pos.y + dimensions.y / 2.0f;
		r32 xSide = dx < 0.0f ? pos.x - dimensions.x / 2.0f : pos.x + dimensions.x / 2.0f;

		s32 yTopMetatile = WorldToTilemap(yTop);
		s32 yBottomMetatile = WorldToTilemap(yBottom);
		// Right at the seam, should look at one tile above
		if (IsNearlyZero(yBottom - TilemapToWorld(yBottomMetatile)))
			yBottomMetatile--;
		s32 yMetatileDelta = yBottomMetatile - yTopMetatile;
		s32 xMetatile = WorldToTilemap(xSide);

		r32 dist = 0.0f;

		while (!outHit.blockingHit && dist < fabs(dx)) {
			for (s32 i = 0; i <= yMetatileDelta; i++) {
				IVec2 metatileCoord = IVec2{ xMetatile, yTopMetatile + i };

				const u32 index = GetMetatileIndex(pLevel, metatileCoord);
				const Tileset::TileType tileType = Tileset::GetTileType(index);

				if (tileType == Tileset::TileSolid) {
					outHit.blockingHit = true;
					outHit.startPenetrating = IsNearlyZero(dist);
					outHit.distance = dist;
					outHit.impactNormal = Vec2{ -Sign(dx), 0 };
					outHit.impactPoint = Vec2{ pos.x + Sign(dx) * (dist + (dimensions.x / 2.0f)), pos.y };
					outHit.location = Vec2{ pos.x + Sign(dx) * dist, pos.y };
					outHit.normal = Vec2{ Sign(dx), 0 };
					outHit.tileType = tileType;

					break;
				}
			}

			r32 distToNextTile = dx < 0.0f ? xSide - TilemapToWorld(xMetatile) : TilemapToWorld(xMetatile + 1) - xSide;
			dist += distToNextTile;
			xSide += Sign(dx) * distToNextTile;
			xMetatile += (s32)Sign(dx);
		}
	}

	void SweepBoxVertical(Level* pLevel, Vec2 pos, Vec2 dimensions, r32 dy, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = abs(dy);
		outHit.location = Vec2{ pos.x, pos.y + dy };

		if (IsNearlyZero(dy)) {
			return;
		}

		r32 xLeft = pos.x - dimensions.x / 2.0f;
		r32 xRight = pos.x + dimensions.x / 2.0f;
		r32 ySide = dy < 0.0f ? pos.y - dimensions.y / 2.0f : pos.y + dimensions.y / 2.0f;

		s32 xLeftMetatile = WorldToTilemap(xLeft);
		s32 xRightMetatile = WorldToTilemap(xRight);
		// Right at the seam, should look at one tile left
		if (IsNearlyZero(xRight - TilemapToWorld(xRightMetatile)))
			xRightMetatile--;
		s32 xMetatileDelta = xRightMetatile - xLeftMetatile;
		s32 yMetatile = WorldToTilemap(ySide);

		r32 dist = 0.0f;

		while (!outHit.blockingHit && dist < fabs(dy)) {
			for (s32 i = 0; i <= xMetatileDelta; i++) {
				IVec2 metatileCoord = IVec2{ xLeftMetatile + i, yMetatile };

				const u32 index = GetMetatileIndex(pLevel, metatileCoord);
				const Tileset::TileType tileType = Tileset::GetTileType(index);

				if (tileType == Tileset::TileSolid) {
					outHit.blockingHit = true;
					outHit.startPenetrating = IsNearlyZero(dist);
					outHit.distance = dist;
					outHit.impactNormal = Vec2{ 0, -Sign(dy) };
					outHit.impactPoint = Vec2{ pos.x, pos.y + Sign(dy) * (dist + (dimensions.y / 2.0f)) };
					outHit.location = Vec2{ pos.x, pos.y + Sign(dy) * dist };
					outHit.normal = Vec2{ 0, Sign(dy) };
					outHit.tileType = tileType;

					break;
				}
			}

			r32 distToNextTile = dy < 0.0f ? ySide - TilemapToWorld(yMetatile) : TilemapToWorld(yMetatile + 1) - ySide;
			dist += distToNextTile;
			ySide += Sign(dy) * distToNextTile;
			yMetatile += (s32)Sign(dy);
		}
	}
}