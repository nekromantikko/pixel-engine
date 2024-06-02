#pragma once
#include "rendering.h"

struct Viewport;
struct Level;

namespace Game {
	void Initialize(Rendering::RenderContext* pContext);
	void Free();
	void Step(r64 dt, Rendering::RenderContext* pContext);

	bool IsPaused();
	void SetPaused(bool paused);

	Viewport* GetViewport();
	Level* GetLevel();
}
