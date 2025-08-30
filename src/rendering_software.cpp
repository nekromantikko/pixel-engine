#include "rendering.h"
#include "core_types.h"
#include "debug.h"
#include "memory_arena.h"

static constexpr u32 FRAMEBUFFER_WIDTH = VIEWPORT_WIDTH_PIXELS;
static constexpr u32 FRAMEBUFFER_HEIGHT = VIEWPORT_HEIGHT_PIXELS;
static constexpr u32 FRAMEBUFFER_SIZE_PIXELS = FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT;
static constexpr u32 FRAMEBUFFER_COUNT = 2;

struct Pixel {
    u8 r;
    u8 g;
    u8 b;
};

struct Framebuffer {
    Pixel pixels[FRAMEBUFFER_SIZE_PIXELS];
};

SDL_Window* g_SDLWindow = nullptr;
SDL_Surface* g_WindowSurface = nullptr;

Framebuffer* g_Framebuffers = nullptr;
SDL_Surface* g_FramebufferSurfaces[FRAMEBUFFER_COUNT];

Palette* g_Palettes;
Sprite* g_Sprites;
ChrSheet* g_ChrSheets;
Nametable* g_Nametables;
Scanline* g_Scanlines;

void Rendering::Init(SDL_Window* sdlWindow) {
    g_SDLWindow = sdlWindow;
    g_WindowSurface = SDL_GetWindowSurface(sdlWindow);
    if (!g_WindowSurface) {
        DEBUG_FATAL("Failed to get window surface\n");
    }

    g_Framebuffers = ArenaAllocator::PushArray<Framebuffer>(ARENA_PERMANENT, FRAMEBUFFER_COUNT);

    for (u32 i = 0; i < FRAMEBUFFER_COUNT; i++) {
        g_FramebufferSurfaces[i] = SDL_CreateRGBSurfaceFrom(g_Framebuffers[i].pixels, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, 24, FRAMEBUFFER_WIDTH * sizeof(Pixel),
            0x0000FF, 0x00FF00, 0xFF0000, 0);
    }

    g_Palettes = ArenaAllocator::PushArray<Palette>(ARENA_PERMANENT, PALETTE_COUNT);
    g_Sprites = ArenaAllocator::PushArray<Sprite>(ARENA_PERMANENT, MAX_SPRITE_COUNT);
    g_ChrSheets = ArenaAllocator::PushArray<ChrSheet>(ARENA_PERMANENT, CHR_COUNT);
    g_Nametables = ArenaAllocator::PushArray<Nametable>(ARENA_PERMANENT, NAMETABLE_COUNT);
    g_Scanlines = ArenaAllocator::PushArray<Scanline>(ARENA_PERMANENT, SCANLINE_COUNT);

    for (u32 i = 0; i < FRAMEBUFFER_COUNT; i++) {
        for (u32 p = 0; p < FRAMEBUFFER_SIZE_PIXELS; p++) {
            g_Framebuffers[i].pixels[p] = { 100, 149, 237 }; // Cornflower Blue
        }
    }
}

void Rendering::Free() {
    for (u32 i = 0; i < FRAMEBUFFER_COUNT; i++) {
        SDL_FreeSurface(g_FramebufferSurfaces[i]);
    }
}

void Rendering::BeginFrame() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::BeginRenderPass() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::EndFrame() {
    SDL_BlitScaled(g_FramebufferSurfaces[0], nullptr, g_WindowSurface, nullptr);
    SDL_UpdateWindowSurface(g_SDLWindow);
}

void Rendering::WaitForAllCommands() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::ResizeSurface(u32 width, u32 height) {
    g_WindowSurface = SDL_GetWindowSurface(g_SDLWindow);
}

RenderSettings* Rendering::GetSettingsPtr() {
    // This is just a stub to allow compilation when using the software renderer.
    return nullptr;
}

Palette* Rendering::GetPalettePtr(u32 paletteIndex) {
    if (paletteIndex >= PALETTE_COUNT) {
        return nullptr;
    }
    return &g_Palettes[paletteIndex];
}

Sprite* Rendering::GetSpritesPtr(u32 offset) {
    if (offset >= MAX_SPRITE_COUNT) {
        return nullptr;
    }
    return &g_Sprites[offset];
}

ChrSheet* Rendering::GetChrPtr(u32 sheetIndex) {
    if (sheetIndex >= CHR_COUNT) {
        return nullptr;
    }
    return &g_ChrSheets[sheetIndex];
}

Nametable* Rendering::GetNametablePtr(u32 index) {
    if (index >= NAMETABLE_COUNT) {
        return nullptr;
    }
    return &g_Nametables[index];
}

Scanline* Rendering::GetScanlinePtr(u32 offset) {
    if (offset >= SCANLINE_COUNT) {
        return nullptr;
    }
    return &g_Scanlines[offset];
}

#pragma region Editor
#ifdef EDITOR
void Rendering::InitEditor(SDL_Window* sdlWindow) {
    // This is just a stub to allow compilation when using the software renderer.
    (void)sdlWindow;
}

void Rendering::BeginEditorFrame() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::ShutdownEditor() {
    // This is just a stub to allow compilation when using the software renderer.
}

size_t Rendering::GetEditorBufferSize() {
    // This is just a stub to allow compilation when using the software renderer.
    return 0;
}

bool Rendering::InitEditorBuffer(EditorRenderBuffer* pBuffer, size_t size, const void* data) {
    return false;
}
bool Rendering::UpdateEditorBuffer(const EditorRenderBuffer* pBuffer, const void* data) {
    return false;
}
void Rendering::FreeEditorBuffer(EditorRenderBuffer* pBuffer) {
    // This is just a stub to allow compilation when using the software renderer.
}

size_t Rendering::GetEditorTextureSize() {
    return 0;
}
bool Rendering::InitEditorTexture(EditorRenderTexture* pTexture, u32 width, u32 height, u32 usage, u32 chrSheetOffset, u32 chrPaletteOffset, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
    return false;
}
bool Rendering::UpdateEditorTexture(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
    return false;
}
void* Rendering::GetEditorTextureData(const EditorRenderTexture* pTexture) {
    return nullptr;
}
void Rendering::FreeEditorTexture(EditorRenderTexture* pTexture) {
    // This is just a stub to allow compilation when using the software renderer.
}

// Software
void Rendering::RenderEditorTexture(const EditorRenderTexture* pTexture) {
    // This is just a stub to allow compilation when using the software renderer.
}

// Render pass
void Rendering::RenderEditor() {
    // This is just a stub to allow compilation when using the software renderer.
}
#endif
#pragma endregion
