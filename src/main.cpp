#include <windows.h>
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <cfloat>
#include "rendering.h"
#include "system.h"
#include "game.h"

#include "editor_core.h"
#include "editor_debug.h"
#include "editor_sprites.h"
#include "editor_chr.h"
#include "editor_tiles.h"
#include "editor_level.h"

static Rendering::RenderContext* pRenderContext;
static Editor::EditorContext* pEditorContext;
static bool running;

static inline s64 GetPerformanceFrequency() {
    LARGE_INTEGER perfFreq;
    QueryPerformanceFrequency(&perfFreq);
    return perfFreq.QuadPart;
}

static inline s64 GetPerfomanceCounter() {
    LARGE_INTEGER perfCount;
    QueryPerformanceCounter(&perfCount);
    return perfCount.QuadPart;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK MainWindowCallback(_In_ HWND hwnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam) {
    LRESULT result = 0;

    switch (uMsg)
    {
        case WM_SIZE:
        {
            OutputDebugStringA("WM_SIZE\n");
            if (pRenderContext != nullptr) {
                Rendering::ResizeSurface(pRenderContext, LOWORD(lParam), HIWORD(lParam));
            }
            break;
        }
        case WM_DESTROY:
        {
            running = false;
            break;
        }
        case WM_CLOSE:
        {
            running = false;
            break;
        }
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(hwnd, &paint);
            PatBlt(deviceContext, paint.rcPaint.left, paint.rcPaint.top, paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top, WHITENESS);
            EndPaint(hwnd, &paint);

            break;
        }
        default:
        {
            result = ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
            if (!result) {
                result = DefWindowProc(hwnd, uMsg, wParam, lParam);
            }
            break;
        }
    }

    return result;
}

int APIENTRY WinMain(_In_ HINSTANCE hInst, _In_ HINSTANCE hInstPrev, _In_ PSTR cmdline, _In_ int cmdshow) {
#ifdef PLATFORM_WINDOWS
    DEBUG_LOG("Using windows platform...\n");
#endif
    WNDCLASSA windowClass = {};

    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = MainWindowCallback;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInst; // Alternatively => GetModuleHandle(0);
    windowClass.hIcon = 0;
    windowClass.hCursor = 0;
    windowClass.hbrBackground = 0;
    windowClass.lpszMenuName = 0;
    windowClass.lpszClassName = "PixelEngineWindowClass";

    if (RegisterClassA(&windowClass) == 0) {
        DEBUG_ERROR("Failed to register window class\n");
    }

    HWND windowHandle = CreateWindowExA(
        0,
        windowClass.lpszClassName,
        "Nekro Pixel Engine",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1920,
        1080,
        0,
        0,
        hInst,
        0
    );

    if (windowHandle == nullptr) {
        DEBUG_ERROR("Failed to create window\n");
    }

    MSG message;

    Rendering::Surface surface{};
    surface.hInstance = hInst,
        surface.hWnd = windowHandle;

    pRenderContext = Rendering::CreateRenderContext(surface);
    DEBUG_LOG("Render context = %x\n", pRenderContext);

    ImGui::CreateContext();
    ImGui_ImplWin32_Init(windowHandle);
    Rendering::InitImGui(pRenderContext);

    pEditorContext = Editor::CreateEditorContext(pRenderContext);

    const s64 perfFreq = GetPerformanceFrequency();
    s64 currentTime = GetPerfomanceCounter();

    SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_HAPTIC);
    
    Game::Initialize(pRenderContext);
    Game::LoadLevel(0);

    running = true;
    while (running) {
        while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
            }
            
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        s64 newTime = GetPerfomanceCounter();
        s64 deltaTime = newTime - currentTime;
        r64 deltaTimeSeconds = (r64)deltaTime / perfFreq;
        currentTime = newTime;

        if (deltaTimeSeconds >= FLT_MIN) {
            Game::Step(deltaTimeSeconds);
        }

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

    SDL_Quit();
    return 0;
}