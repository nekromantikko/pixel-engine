#include "collision.h"
#include "rendering.h"
#include <stdio.h>
#include "system.h"

namespace Collision {

	TileCollision bgTileCollision[256]{};

	TileCollision* GetBgCollisionPtr() {
		return bgTileCollision;
	}

	void LoadBgCollision(const char* fname) {
		FILE* pFile;
		fopen_s(&pFile, fname, "rb");

		if (pFile == NULL) {
			ERROR("Failed to load tile collision file\n");
		}

		const char signature[4]{};
		fread((void*)signature, sizeof(u8), 4, pFile);
		fread((void*)bgTileCollision, sizeof(TileCollision), 256, pFile);

		fclose(pFile);
	}

	void SaveBgCollision(const char* fname) {
		FILE* pFile;
		fopen_s(&pFile, fname, "wb");

		if (pFile == NULL) {
			ERROR("Failed to write tile collision file\n");
		}

		const char signature[4] = "TIL";
		fwrite(signature, sizeof(u8), 4, pFile);
		fwrite(bgTileCollision, sizeof(TileCollision), 256, pFile);

		fclose(pFile);
	}

	// TODO: Move to some math util
	bool IsNearlyEqual(r32 a, r32 b, r32 tolerance = 0.00001f) {
		return abs(a - b) <= tolerance;
	}

	bool IsNearlyZero(r32 a, r32 tolerance = 0.00001f) {
		return IsNearlyEqual(a, 0.0f, tolerance);
	}

	r32 Sign(r32 a) {
		if (a < 0.0f) {
			return -1.0f;
		}
		else if (a > 0.0f) {
			return 1.0f;
		}
		else return 0.0f;
	}

	r32 GetTileSlope(TileCollision tile) {
		return (tile.slopeEnd.y - tile.slopeStart.y) / (tile.slopeEnd.x - tile.slopeStart.x);
	}

	r32 GetTileSurfaceX(TileCollision tile, r32 y) {
		r32 slope = GetTileSlope(tile);
		r32 intercept = tile.slopeStart.y - slope * tile.slopeStart.x;

		return (y - intercept) / slope;
	}

	r32 GetTileSurfaceY(TileCollision tile, r32 x) {
		r32 slope = GetTileSlope(tile);
		r32 intercept = tile.slopeStart.y - slope * tile.slopeStart.x;

		return slope * x + intercept;
	}

	bool PointInsideTile(TileCollision tile, Vec2 p) {
		if (p.x < 0.0f || p.x > 1.0f || p.y < 0.0f || p.y > 1.0f) {
			return false;
		}

		if (tile.type == TileSolid) {
			return true;
		}

		r32 yTileSurface = GetTileSurfaceY(tile, p.x);
		if (tile.type == TileSlopeFlip) {
			return p.y < yTileSurface;
		}

		return p.y > yTileSurface;
	}

	bool LineIntersectsTileSurface(Vec2 start, Vec2 dir, TileCollision tile, IVec2 tileCoord, Vec2& outIntersectionPoint) {
		r32 raySlope = dir.y / dir.x;
		r32 rayIntercept = start.y - raySlope * start.x;

		r32 tileSlope = GetTileSlope(tile);
		Vec2 slopeStartWorld = Vec2{ tile.slopeStart.x + tileCoord.x, tile.slopeStart.y + tileCoord.y };
		r32 tileIntercept = slopeStartWorld.y - tileSlope * slopeStartWorld.x;

		r32 xIntersection = dir.x == 0.0f ? start.x : (tileIntercept - rayIntercept) / (raySlope - tileSlope);

		// If intersection point is outside tile, there's no collision
		if (xIntersection - tileCoord.x < 0 || xIntersection - tileCoord.x > 1) {
			return false;
		}

		r32 yIntersection = tileSlope * xIntersection + tileIntercept;

		outIntersectionPoint = Vec2{ xIntersection, yIntersection };
		return true;
	}

	u8 GetTileWorldSpace(Rendering::RenderContext* pRenderContext, IVec2 tileCoord) {
		// Cast to unsigned to prevent negative coordinates
		u32 nametableIndex = ((u32)tileCoord.x / NAMETABLE_WIDTH_TILES) % NAMETABLE_COUNT;
		IVec2 nametableCoord = { (u32)tileCoord.x % NAMETABLE_WIDTH_TILES, (u32)tileCoord.y % NAMETABLE_HEIGHT_TILES };
		u32 nametableOffset = nametableCoord.y * NAMETABLE_WIDTH_TILES + nametableCoord.x;
		u8 tile;
		Rendering::ReadNametable(pRenderContext, nametableIndex, 1, nametableOffset, &tile);
		return tile;
	}

	void RaycastTilesWorldSpace(Rendering::RenderContext *pRenderContext, Vec2 start, Vec2 dir, r32 maxLength, HitResult& outHitResult) {
		outHitResult.blockingHit = false;

		const Vec2 deltaDist = Vec2{ abs(1 / dir.x), abs(1 / dir.y) };

		IVec2 tileCoord = { (s32)start.x, (s32)start.y };
		Vec2 sideDist = { 0,0 };

		// Check starting tile first
		const u8 startTile = GetTileWorldSpace(pRenderContext, tileCoord);
		const TileCollision startTileCollision = bgTileCollision[startTile];

		if (startTileCollision.type != TileEmpty) {
			if (startTileCollision.type != TileJumpThrough && PointInsideTile(startTileCollision, Vec2{ start.x - tileCoord.x, start.y - tileCoord.y })) {
				outHitResult.blockingHit = true;
				outHitResult.startPenetrating = true;
				outHitResult.distance = 0;
				outHitResult.impactNormal = Vec2{};
				outHitResult.impactPoint = start;
				outHitResult.location = start;
				outHitResult.normal = dir;
				outHitResult.tileType = startTileCollision.type;

				return;
			}

			Vec2 intersectionPoint;
			if (LineIntersectsTileSurface(start, dir, startTileCollision, tileCoord, intersectionPoint)) {
				outHitResult.blockingHit = true;
				outHitResult.startPenetrating = false;
				outHitResult.distance = (intersectionPoint - start).Length();
				outHitResult.impactNormal = Vec2{ GetTileSlope(startTileCollision), startTileCollision.type == TileSlopeFlip ? 1.0f : -1.0f }.Normalize();
				outHitResult.impactPoint = intersectionPoint;
				outHitResult.location = intersectionPoint;
				outHitResult.normal = dir;
				outHitResult.tileType = startTileCollision.type;

				return;
			}
		}

		IVec2 step;
		if (dir.x < 0) {
			step.x = -1;
			sideDist.x = (start.x - tileCoord.x) * deltaDist.x;
		}
		else if (dir.x > 0) {
			step.x = 1;
			sideDist.x = (tileCoord.x + 1.0f - start.x) * deltaDist.x;
		}
		else {
			step.x = 0;
			sideDist.x = INFINITY;
		}
		if (dir.y < 0) {
			step.y = -1;
			sideDist.y = (start.y - tileCoord.y) * deltaDist.y;
		}
		else if (dir.y > 0) {
			step.y = 1;
			sideDist.y = (tileCoord.y + 1.0f - start.y) * deltaDist.y;
		}
		else {
			step.y = 0;
			sideDist.y = INFINITY;
		}

		r32 dist = 0;

		// DDA (Digital Differential Analysis)
		while (!outHitResult.blockingHit) {
			// 0 = x, 1 = y
			bool side;

			// Advance in either x or y direction depending on which distance is shorter
			if (sideDist.x < sideDist.y) {
				dist = sideDist.x;
				sideDist.x += deltaDist.x;

				tileCoord.x += step.x;
				side = 0;
			}
			else {
				dist = sideDist.y;
				sideDist.y += deltaDist.y;

				tileCoord.y += step.y;
				side = 1;
			}

			if (dist > maxLength) {
				break;
			}

			const u8 tile = GetTileWorldSpace(pRenderContext, tileCoord);
			const TileCollision tileCollision = bgTileCollision[tile];

			if (tileCollision.type == TileEmpty) {
				continue;
			}

			Vec2 impactPoint = start + dir * dist;

			// Solid tiles block completely
			if (tileCollision.type == TileSolid) {
				outHitResult.blockingHit = true;
				outHitResult.startPenetrating = false;
				outHitResult.distance = dist;
				outHitResult.impactNormal = side ? Vec2{ 0, (r32)-step.y } : Vec2{ (r32)-step.x, 0 };
				outHitResult.impactPoint = impactPoint;
				outHitResult.location = impactPoint;
				outHitResult.normal = dir;
				outHitResult.tileType = TileSolid;
			}
			// For slopes need additional calculations
			else {
				Vec2 subTilePos = Vec2{ impactPoint.x - tileCoord.x, impactPoint.y - tileCoord.y };

				// Can't collide with jump thru tiles from below
				if (tileCollision.type == TileJumpThrough && step.y < 0) {
					continue;
				}

				r32 yTileSurface = GetTileSurfaceY(tileCollision, subTilePos.x);
				// Hit sides of a slope?
				if ((tileCollision.type == TileSlopeFlip && subTilePos.y < yTileSurface) || (tileCollision.type == TileSlope && subTilePos.y > yTileSurface)) {
					outHitResult.blockingHit = true;
					outHitResult.startPenetrating = false;
					outHitResult.distance = dist;
					outHitResult.impactNormal = side ? Vec2{ 0, (r32)-step.y } : Vec2{ (r32)-step.x, 0 };
					outHitResult.impactPoint = impactPoint;
					outHitResult.location = impactPoint;
					outHitResult.normal = dir;
					outHitResult.tileType = tileCollision.type;
				}
				// Even more calculations needed for top of slope
				else if (LineIntersectsTileSurface(start, dir, tileCollision, tileCoord, impactPoint) ) {
					dist = (impactPoint - start).Length();
					if (dist > maxLength)
						break;

					outHitResult.blockingHit = true;
					outHitResult.startPenetrating = false;
					outHitResult.distance = dist;
					outHitResult.impactNormal = Vec2{ GetTileSlope(tileCollision), tileCollision.type == TileSlopeFlip ? 1.0f : -1.0f }.Normalize();
					outHitResult.impactPoint = impactPoint;
					outHitResult.location = impactPoint;
					outHitResult.normal = dir;
					outHitResult.tileType = tileCollision.type;
				}
			}
		}
	}

	void SweepBoxHorizontal(Rendering::RenderContext* pRenderContext, Vec2 pos, Vec2 dimensions, r32 dx, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = abs(dx);
		outHit.location = Vec2{ pos.x + dx, pos.y };

		if (IsNearlyZero(dx)) {
			return;
		}

		r32 yTop = pos.y - dimensions.y / 2.0f;
		r32 yBottom = pos.y + dimensions.y / 2.0f;
		r32 xSide = dx < 0.0f ? pos.x - dimensions.x / 2.0f : pos.x + dimensions.x / 2.0f;

		s32 yTopTile = s32(yTop);
		s32 yBottomTile = s32(yBottom);
		// Right at the seam, should look at one tile above
		if (IsNearlyZero(yBottom - yBottomTile))
			yBottomTile--;
		s32 yTileDelta = yBottomTile - yTopTile;
		s32 xTile = s32(xSide);

		r32 dist = 0.0f;

		while (!outHit.blockingHit && dist < abs(dx)) {
			for (s32 i = 0; i <= yTileDelta; i++) {
				IVec2 tileCoord = IVec2{ xTile, yTopTile + i };

				const u8 tile = GetTileWorldSpace(pRenderContext, tileCoord);
				const TileCollision tileCollision = bgTileCollision[tile];

				if (tileCollision.type == TileSolid) {
					outHit.blockingHit = true;
					outHit.startPenetrating = IsNearlyZero(dist);
					outHit.distance = dist;
					outHit.impactNormal = Vec2{ -Sign(dx), 0 };
					outHit.impactPoint = Vec2{ pos.x + Sign(dx) * (dist + (dimensions.x / 2.0f)), pos.y };
					outHit.location = Vec2{ pos.x + Sign(dx) * dist, pos.y };
					outHit.normal = Vec2{ Sign(dx), 0 };
					outHit.tileType = TileSolid;

					break;
				}
			}

			r32 distToNextTile = dx < 0.0f ? xSide - xTile : xTile + 1.0f - xSide;
			dist += distToNextTile;
			xSide += Sign(dx) * distToNextTile;
			xTile += (s32)Sign(dx);
		}
	}

	void SweepBoxVertical(Rendering::RenderContext* pRenderContext, Vec2 pos, Vec2 dimensions, r32 dy, HitResult& outHit) {
		outHit.blockingHit = false;
		outHit.distance = abs(dy);
		outHit.location = Vec2{ pos.x, pos.y + dy };

		if (IsNearlyZero(dy)) {
			return;
		}

		r32 xLeft = pos.x - dimensions.x / 2.0f;
		r32 xRight = pos.x + dimensions.x / 2.0f;
		r32 ySide = dy < 0.0f ? pos.y - dimensions.y / 2.0f : pos.y + dimensions.y / 2.0f;

		s32 xLeftTile = s32(xLeft);
		s32 xRightTile = s32(xRight);
		// Right at the seam, should look at one tile left
		if (IsNearlyZero(xRight - xRightTile))
			xRightTile--;
		s32 xTileDelta = xRightTile - xLeftTile;
		s32 yTile = s32(ySide);

		r32 dist = 0.0f;

		while (!outHit.blockingHit && dist < abs(dy)) {
			for (s32 i = 0; i <= xTileDelta; i++) {
				IVec2 tileCoord = IVec2{ xLeftTile + i, yTile };

				const u8 tile = GetTileWorldSpace(pRenderContext, tileCoord);
				const TileCollision tileCollision = bgTileCollision[tile];

				if (tileCollision.type == TileSolid) {
					outHit.blockingHit = true;
					outHit.startPenetrating = IsNearlyZero(dist);
					outHit.distance = dist;
					outHit.impactNormal = Vec2{ 0, -Sign(dy) };
					outHit.impactPoint = Vec2{ pos.x, pos.y + Sign(dy) * (dist + (dimensions.y / 2.0f)) };
					outHit.location = Vec2{ pos.x, pos.y + Sign(dy) * dist };
					outHit.normal = Vec2{ 0, Sign(dy) };
					outHit.tileType = TileSolid;

					break;
				}
			}

			r32 distToNextTile = dy < 0.0f ? ySide - yTile : yTile + 1.0f - ySide;
			dist += distToNextTile;
			ySide += Sign(dy) * distToNextTile;
			yTile += (s32)Sign(dy);
		}
	}
}