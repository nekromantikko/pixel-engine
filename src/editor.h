#pragma once

struct SDL_Window;
union SDL_Event;

namespace Audio {
	struct AudioContext;
}

namespace Editor {
	void CreateContext();
	void Init(SDL_Window* pWindow);
	void Free();
	void DestroyContext();

	void ProcessEvent(const SDL_Event* event);
	void Render(Audio::AudioContext* pAudioContext);
}