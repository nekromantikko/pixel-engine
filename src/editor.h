#pragma once
#include "typedef.h"

struct SDL_Window;
union SDL_Event;

struct ChrSheet;

namespace Editor {
	void CreateContext();
	void Init(SDL_Window* pWindow);
	void Free();
	void DestroyContext();

	void ProcessEvent(const SDL_Event* event);
	void Render();

	void SetupChrBankRendering(u32 index, ChrSheet* pBank);
}