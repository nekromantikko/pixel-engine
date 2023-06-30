#pragma once
#include "rendering.h"

namespace Game {
	struct Metasprite {
		const char* name;
		u32 spriteCount;
		Rendering::Sprite* sprites;
	};

	void Initialize(Rendering::RenderContext* pContext);
	void Step(float dt, Rendering::RenderContext* pContext);
}
