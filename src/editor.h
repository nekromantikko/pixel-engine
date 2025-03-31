#pragma once
#include "typedef.h"

struct SDL_Window;
union SDL_Event;

namespace Editor {
	void CreateContext();
	void Init(SDL_Window* pWindow);
	void Free();
	void DestroyContext();

	void ConsoleLog(const char* fmt, va_list args);
	void ClearLog();

	void ProcessEvent(const SDL_Event* event);
	void SetupFrame();
	void Render(r64 dt);
}