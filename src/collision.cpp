#include "collision.h"
#include "rendering.h"
#include <stdio.h>
#include "system.h"
#include "level.h"
#include <gtc/epsilon.hpp>

static constexpr r32 epsilon = 0.0000001f;

static inline bool IsNearlyZero(r32 value) {
	return glm::epsilonEqual(value, 0.0f, epsilon);
}

namespace Collision {
	void SweepBoxHorizontal(const Tilemap* pTilemap, const AABB& hitbox, const glm::vec2& pos, r32 dx, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = glm::abs(dx);
		outHit.location = glm::vec2{ pos.x + dx, pos.y };

		if (pTilemap == nullptr) {
			return;
		}

		if (IsNearlyZero(dx)) {
			return;
		}

		const AABB hitboxAbs(hitbox.min + pos, hitbox.max + pos);
		r32 xSide = dx < 0.0f ? hitboxAbs.x1 : hitboxAbs.x2;

		s32 yTopTile = (s32)glm::floor(hitboxAbs.y1);
		s32 yBottomTile = (s32)glm::floor(hitboxAbs.y2);
		// Right at the seam, should look at one tile above
		if (IsNearlyZero(hitboxAbs.y2 - (r32)yBottomTile))
			yBottomTile--;
		s32 yTileDelta = yBottomTile - yTopTile;
		s32 xTile = (s32)glm::floor(xSide);

		r32 dist = 0.0f;

		while (!outHit.blockingHit && dist < glm::abs(dx)) {
			for (s32 i = 0; i <= yTileDelta; i++) {
				glm::ivec2 tileCoord = glm::ivec2{ xTile, yTopTile + i };

				const MapTile* tile = Tiles::GetMapTile(pTilemap, tileCoord);

				// Treat outside of screen as solid wall
				if (!tile || tile->type == TILE_SOLID) {
					outHit.blockingHit = true;
					outHit.startPenetrating = IsNearlyZero(dist);
					outHit.distance = dist;
					outHit.impactNormal = glm::vec2{ -glm::sign(dx), 0 };
					outHit.impactPoint = glm::vec2{ xSide, pos.y };
					outHit.location = glm::vec2{ pos.x + glm::sign(dx) * dist, pos.y };
					outHit.normal = glm::vec2{ glm::sign(dx), 0 };
					outHit.tileType = tile ? tile->type: TILE_SOLID;

					break;
				}
			}

			r32 distToNextTile = dx < 0.0f ? xSide - xTile : xTile + 1 - xSide;
			dist += distToNextTile;
			xSide += glm::sign(dx) * distToNextTile;
			xTile += (s32)glm::sign(dx);
		}
	}

	void SweepBoxVertical(const Tilemap *pTilemap, const AABB& hitbox, const glm::vec2& pos, r32 dy, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = glm::abs(dy);
		outHit.location = glm::vec2{ pos.x, pos.y + dy };

		if (pTilemap == nullptr) {
			return;
		}

		if (IsNearlyZero(dy)) {
			return;
		}

		const AABB hitboxAbs(hitbox.min + pos, hitbox.max + pos);
		r32 ySide = dy < 0.0f ? hitboxAbs.y1 : hitboxAbs.y2;

		s32 xLeftTile = (s32)glm::floor(hitboxAbs.x1);
		s32 xRightTile = (s32)glm::floor(hitboxAbs.x2);
		// Right at the seam, should look at one tile left
		if (IsNearlyZero(hitboxAbs.x2 - (r32)xRightTile))
			xRightTile--;
		s32 xTileDelta = xRightTile - xLeftTile;
		s32 yTile = (s32)glm::floor(ySide);

		r32 dist = 0.0f;

		while (!outHit.blockingHit && dist < glm::abs(dy)) {
			for (s32 i = 0; i <= xTileDelta; i++) {
				glm::ivec2 metatileCoord = glm::ivec2{ xLeftTile + i, yTile };

				const MapTile* tile = Tiles::GetMapTile(pTilemap, metatileCoord);
				
				// Treat outside of screen as solid wall
				if (!tile || tile->type == TILE_SOLID) {
					outHit.blockingHit = true;
					outHit.startPenetrating = IsNearlyZero(dist);
					outHit.distance = dist;
					outHit.impactNormal = glm::vec2{ 0, -glm::sign(dy) };
					outHit.impactPoint = glm::vec2{ pos.x, ySide };
					outHit.location = glm::vec2{ pos.x, pos.y + glm::sign(dy) * dist };
					outHit.normal = glm::vec2{ 0, glm::sign(dy) };
					outHit.tileType = tile ? tile->type : TILE_SOLID;

					break;
				}
			}

			r32 distToNextTile = dy < 0.0f ? ySide - yTile : yTile + 1 - ySide;
			dist += distToNextTile;
			ySide += glm::sign(dy) * distToNextTile;
			yTile += (s32)glm::sign(dy);
		}
	}

	bool BoxesOverlap(const AABB& a, const glm::vec2& aPos, const AABB& b, const glm::vec2& bPos) {
		const AABB aAbs = AABB(a.min + aPos, a.max + aPos);
		const AABB bAbs = AABB(b.min + bPos, b.max + bPos);

		return (aAbs.x1 < bAbs.x2 &&
			aAbs.x2 >= bAbs.x1 &&
			aAbs.y1 < bAbs.y2 &&
			aAbs.y2 >= bAbs.y1);
	}

	bool PointInsideBox(const glm::vec2& point, const AABB& hitbox, const glm::vec2& boxPos) {
		const AABB hitboxAbs = AABB(hitbox.min + boxPos, hitbox.max + boxPos);

		return (point.x >= hitboxAbs.x1 &&
			point.x < hitboxAbs.x2 &&
			point.y >= hitboxAbs.y1 &&
			point.y < hitboxAbs.y2);
	}
}