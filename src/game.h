#pragma once
#include "rendering.h"
#include "memory_pool.h"
#include "actors.h"

constexpr u32 MAX_DYNAMIC_ACTOR_COUNT = 512;
typedef Pool<Actor, MAX_DYNAMIC_ACTOR_COUNT> DynamicActorPool;

struct Viewport;

struct Level;

namespace Game {
	void Initialize();
	void Free();
	void LoadLevel(u32 index, s32 screenIndex = 0, u8 direction = 0, bool refresh = true);
	void UnloadLevel(bool refresh = true);
	void ReloadLevel(s32 screenIndex = 0, u8 direction = 0, bool refresh = true);
	void Update(r64 dt);

	bool IsPaused();
	void SetPaused(bool paused);

	Level* GetCurrentLevel();

	DynamicActorPool* GetActors();
}
