#include <SDL.h>
#include <cfloat>
#include "rendering.h"
#include "game.h"
#include "input.h"
#include "audio.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>

#ifdef EDITOR
#include "editor.h"
#endif

static constexpr const char* WINDOW_TITLE = "Nekro Pixel Engine";
static constexpr u32 WINDOW_TITLE_MAX_LENGTH = 128;

static constexpr u32 SUCCESSIVE_FRAME_TIME_COUNT = 64;
static r64 successiveFrameTimes[SUCCESSIVE_FRAME_TIME_COUNT]{ 0 };

static void HandleWindowEvent(const SDL_WindowEvent& event, bool& minimized) {
    switch (event.event) {
    case SDL_WINDOWEVENT_RESIZED:
        break;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        Rendering::ResizeSurface(event.data1, event.data2);
        break;
    case SDL_WINDOWEVENT_MINIMIZED:
        minimized = true;
        break;
    case SDL_WINDOWEVENT_MAXIMIZED:
        break;
    case SDL_WINDOWEVENT_RESTORED:
        minimized = false;
        break;
    default:
        break;
    }
}

static r64 GetAverageFramerate(r64 dt) {
    r64 sum = 0.0;
    // Pop front
    for (u32 i = 1; i < SUCCESSIVE_FRAME_TIME_COUNT; i++) {
        sum += successiveFrameTimes[i];
        successiveFrameTimes[i - 1] = successiveFrameTimes[i];
    }
    // Push back
    sum += dt;
    successiveFrameTimes[SUCCESSIVE_FRAME_TIME_COUNT - 1] = dt;

    return (r64)SUCCESSIVE_FRAME_TIME_COUNT / sum;
}

static void UpdateWindowTitle(SDL_Window* pWindow, r64 averageFramerate, r64 dt) {
    char titleStr[WINDOW_TITLE_MAX_LENGTH];

    snprintf(titleStr, WINDOW_TITLE_MAX_LENGTH, "%s %4d FPS (%.2f ms)", WINDOW_TITLE, (s32)glm::roundEven(averageFramerate), dt * 1000);
    SDL_SetWindowTitle(pWindow, titleStr);
}

#ifdef PLATFORM_WINDOWS
int WinMain(int argc, char** args) {
#else
int main(int argc, char** argv) {
#endif
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_HAPTIC);
    SDL_Window* pWindow = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1536, 864, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    Rendering::CreateContext();
    Rendering::CreateSurface(pWindow);
    Rendering::Init();

    Audio::CreateContext();
    Audio::Init();

#ifdef EDITOR
    Editor::CreateContext();
    Editor::Init(pWindow);
#endif

    const s64 perfFreq = SDL_GetPerformanceFrequency();
    u64 currentTime = SDL_GetPerformanceCounter();
    
    Game::Initialize();
    
    bool running = true;
    bool minimized = false;
    SDL_Event event;
    while (running) {
        s64 newTime = SDL_GetPerformanceCounter();
        s64 deltaTime = newTime - currentTime;
        r64 deltaTimeSeconds = (r64)deltaTime / perfFreq;
        currentTime = newTime;

        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT:
                HandleWindowEvent(event.window, minimized);
                break;
            default:
                break;
            }

            Input::ProcessEvent(&event);
#ifdef EDITOR
            Editor::ProcessEvent(&event);
#endif
        }
        
        Game::Update(deltaTimeSeconds);

        if (!minimized) {
            r64 averageFramerate = GetAverageFramerate(deltaTimeSeconds);
            UpdateWindowTitle(pWindow, averageFramerate, deltaTimeSeconds);

#ifdef EDITOR
			Editor::Update(deltaTimeSeconds);
#endif
            Rendering::BeginFrame();
#ifdef EDITOR
            Editor::SetupFrame();
#endif
            Rendering::BeginRenderPass();
#ifdef EDITOR
            Editor::Render();
#endif
            Rendering::EndFrame();
        }
    }

    Game::Free();
    Rendering::WaitForAllCommands();
#ifdef EDITOR
    Editor::Free();
    Editor::DestroyContext();
#endif
    Rendering::Free();
    Rendering::DestroyContext();

    Audio::Free();
    Audio::DestroyContext();

    SDL_DestroyWindow(pWindow);
    SDL_Quit();
    return 0;
}