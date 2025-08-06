#pragma once
#include "typedef.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>

namespace Random {
	u64 GenerateUUID();
	u32 GenerateUUID32();

	s32 GenerateInt(s32 min, s32 max);
	r32 GenerateReal(r32 min, r32 max);
	glm::vec2 GenerateDirection();
}