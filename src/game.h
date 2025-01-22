#pragma once
#include "rendering.h"

struct Viewport;

namespace Level {
	struct Level;
}

namespace Game {
	void Initialize(Rendering::RenderContext* pContext);
	void Free();
	void LoadLevel(u32 index, s32 screenIndex = -1, bool refresh = true);
	void ReloadLevel(bool refresh = true);
	void Step(r64 dt);

	bool IsPaused();
	void SetPaused(bool paused);

	Viewport* GetViewport();
	Level::Level* GetLevel();
}
