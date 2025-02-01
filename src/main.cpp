#include <SDL.h>
#include <cfloat>
#include "rendering.h"
#include "system.h"
#include "game.h"
#include "input.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"

#include "editor_core.h"
#include "editor_debug.h"
#include "editor_sprites.h"
#include "editor_chr.h"
#include "editor_tiles.h"
#include "editor_level.h"

static void HandleWindowEvent(const SDL_WindowEvent& event, Rendering::RenderContext* pRenderContext) {
    switch (event.event) {
    case SDL_WINDOWEVENT_RESIZED:
        break;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        Rendering::ResizeSurface(pRenderContext, event.data1, event.data2);
        break;
    case SDL_WINDOWEVENT_MINIMIZED:
        break;
    case SDL_WINDOWEVENT_MAXIMIZED:
        break;
    case SDL_WINDOWEVENT_RESTORED:
        break;
    default:
        break;
    }
}

int WinMain(int argc, char** args) {
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_HAPTIC);
    SDL_Window* sdlWindow = SDL_CreateWindow("Nekro Pixel Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1536, 864, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    Rendering::RenderContext* pRenderContext = Rendering::CreateRenderContext(sdlWindow);
    DEBUG_LOG("Render context = %x\n", pRenderContext);

    ImGui::CreateContext();
    Rendering::InitImGui(pRenderContext, sdlWindow);

    Editor::EditorContext* pEditorContext = Editor::CreateEditorContext(pRenderContext);

    const s64 perfFreq = SDL_GetPerformanceFrequency();
    u64 currentTime = SDL_GetPerformanceCounter();
    
    Game::Initialize(pRenderContext);
    
    bool running = true;
    SDL_Event event;
    while (running) {
        s64 newTime = SDL_GetPerformanceCounter();
        s64 deltaTime = newTime - currentTime;
        r64 deltaTimeSeconds = (r64)deltaTime / perfFreq;
        currentTime = newTime;

        Input::Update();

        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT:
                HandleWindowEvent(event.window, pRenderContext);
                break;
            default:
                break;
            }

            Input::ProcessEvent(event);
            ImGui_ImplSDL2_ProcessEvent(&event);
        }
        
        Game::Step(deltaTimeSeconds);

        Rendering::BeginImGuiFrame(pRenderContext);
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        Editor::Debug::DrawDebugWindow(pEditorContext, pRenderContext);

        Editor::Sprites::DrawPreviewWindow(pEditorContext);
        Editor::Sprites::DrawMetaspriteWindow(pEditorContext);
        Editor::Sprites::DrawSpriteEditor(pEditorContext);

        Editor::CHR::DrawCHRWindow(pEditorContext);
        Editor::Tiles::DrawMetatileEditor(pEditorContext, pRenderContext);
        Editor::Tiles::DrawTilesetEditor(pEditorContext, pRenderContext);

        Editor::LevelEditor::DrawGameWindow(pEditorContext, pRenderContext);
        Editor::LevelEditor::DrawActorList();
        Editor::LevelEditor::DrawLevelList(pRenderContext);

        ImGui::Render();
        Rendering::Render(pRenderContext);
    }

    Game::Free();
    Editor::FreeEditorContext(pEditorContext);
    Rendering::WaitForAllCommands(pRenderContext);
    Rendering::ShutdownImGui();
    ImGui::DestroyContext();
    Rendering::FreeRenderContext(pRenderContext);

    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    return 0;
}