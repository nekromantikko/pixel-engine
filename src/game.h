#pragma once
#include "rendering.h"
#include "memory_pool.h"
#include "actors.h"

struct Viewport;

struct Level;

namespace Game {
	void Initialize();
	void Free();
	void LoadLevel(u32 index, s32 screenIndex = -1, bool refresh = true);
	void UnloadLevel(bool refresh = true);
	void ReloadLevel(bool refresh = true);
	void Update(r64 dt);

	bool IsPaused();
	void SetPaused(bool paused);

	Viewport* GetViewport();
	Level* GetLevel();

	Pool<Actor>* GetActors();
}
