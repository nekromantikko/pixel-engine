#pragma once
#include "rendering.h"

namespace Game {
	void Initialize(Rendering::RenderContext* pContext);
	void Step(float dt, Rendering::RenderContext* pContext);
}
