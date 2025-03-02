#pragma once
#include "typedef.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>

struct Nametable;
struct Tilemap;

namespace Game {
	glm::vec2 GetViewportPos();
	glm::vec2 SetViewportPos(const glm::vec2& pos, bool loadTiles = true);

	void RefreshViewport();

	bool PositionInViewportBounds(const glm::vec2& pos);
	glm::i16vec2 WorldPosToScreenPixels(const glm::vec2& pos);
}