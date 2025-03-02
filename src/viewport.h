#pragma once
#include "typedef.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>

struct Nametable;
struct Tilemap;

namespace Game {
	glm::vec2 MoveViewport(const glm::vec2& viewportPos, Nametable* pNametables, const Tilemap* pTilemap, const glm::vec2& delta, bool loadTiles = true);
	void RefreshViewport(const glm::vec2& viewportPos, Nametable* pNametables, const Tilemap* pTilemap);
}