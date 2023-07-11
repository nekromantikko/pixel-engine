#pragma once
#include "rendering.h"

namespace Game {
	void Initialize(Rendering::RenderContext* pContext);
	void Free();
	void Step(float dt, Rendering::RenderContext* pContext);
}
