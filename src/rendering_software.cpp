#include "rendering.h"
#include "rendering_util.h"
#include "core_types.h"
#include "debug.h"
#include "memory_arena.h"

static constexpr u32 FRAMEBUFFER_WIDTH = VIEWPORT_WIDTH_PIXELS;
static constexpr u32 FRAMEBUFFER_HEIGHT = VIEWPORT_HEIGHT_PIXELS;
static constexpr u32 FRAMEBUFFER_SIZE_PIXELS = FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT;
static constexpr u32 FRAMEBUFFER_COUNT = 2;

struct Framebuffer {
    u32 pixels[FRAMEBUFFER_SIZE_PIXELS];
};

struct ColorSample {
    u8 paletteIndex;
    u8 colorIndex;
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

u32* g_paletteColors;

static void ClearFramebuffer(Framebuffer& framebuffer) {
    memset(framebuffer.pixels, 0, sizeof(framebuffer.pixels));
}

static glm::uvec2 ScrollScreen(const Scanline& scanline, const glm::uvec2& pixelPos, u32& outNametableIndex) {
    const glm::uvec2 scanlineScroll = glm::uvec2(scanline.scrollX, scanline.scrollY);
    glm::uvec2 scrolledPos = pixelPos + scanlineScroll;
    glm::uvec2 nametablePos = scrolledPos / NAMETABLE_DIM_PIXELS;
    outNametableIndex = (nametablePos.x + nametablePos.y) % 2;
    return scrolledPos % NAMETABLE_DIM_PIXELS;
}

static u8 SampleBackgroundTile(const BgTile& tile, u32 pixelOffset) {
    if (tile.flipHorizontal) pixelOffset ^= 7;
    if (tile.flipVertical) pixelOffset ^= 56;

    const u32 chrPage = (tile.tileId >> 8) & 3;
    const u32 chrTileIndex = tile.tileId & 0xFF;
    const ChrTile& chrTile = g_ChrSheets[chrPage].tiles[chrTileIndex];

    u8 colorIndex = ((chrTile.p0 >> pixelOffset) & 1);
    colorIndex |= ((chrTile.p1 >> pixelOffset) & 1) << 1;
    colorIndex |= ((chrTile.p2 >> pixelOffset) & 1) << 2;
    return colorIndex;
}

static ColorSample SampleBackground(const glm::uvec2& scrolledPos, u32 nametableIndex) {
    glm::uvec2 tilePos = scrolledPos / TILE_DIM_PIXELS;
    u32 tileIndex = tilePos.x + tilePos.y * NAMETABLE_DIM_TILES;
    glm::uvec2 pixelPos = scrolledPos % TILE_DIM_PIXELS;
    u32 pixelIndex = pixelPos.x + pixelPos.y * TILE_DIM_PIXELS;
    
    const BgTile& bgTile = g_Nametables[nametableIndex].tiles[tileIndex];
    u8 colorIndex = SampleBackgroundTile(bgTile, pixelIndex);

    if (colorIndex == 0) {
        return { 0, 0 }; // Transparent
    }

    return { u8(bgTile.palette), colorIndex };
}

static void Draw(Framebuffer& framebuffer) {
    u32* pPixels = framebuffer.pixels;
    for (u32 y = 0; y < FRAMEBUFFER_HEIGHT; y++) {
        Scanline& scanline = g_Scanlines[y];
        for (u32 x = 0; x < FRAMEBUFFER_WIDTH; x++) {
            // Apply scroll
            u32 nametableIndex;
            glm::uvec2 scrolledPos = ScrollScreen(scanline, glm::uvec2(x, y), nametableIndex);

            // Sample background
            ColorSample colorSample = SampleBackground(scrolledPos, nametableIndex);

            // Fetch color from palette
            u8 colorIndex = g_Palettes[colorSample.paletteIndex].colors[colorSample.colorIndex];
            u32 color = g_paletteColors[colorIndex];

            // Set pixel in framebuffer
            pPixels[y * FRAMEBUFFER_WIDTH + x] = color;
        }
    }
}

void Rendering::Init(SDL_Window* sdlWindow) {
    g_SDLWindow = sdlWindow;
    g_WindowSurface = SDL_GetWindowSurface(sdlWindow);
    if (!g_WindowSurface) {
        DEBUG_FATAL("Failed to get window surface\n");
    }

    g_Framebuffers = ArenaAllocator::PushArray<Framebuffer>(ARENA_PERMANENT, FRAMEBUFFER_COUNT);

    for (u32 i = 0; i < FRAMEBUFFER_COUNT; i++) {
        g_FramebufferSurfaces[i] = SDL_CreateRGBSurfaceFrom(g_Framebuffers[i].pixels, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, 32, FRAMEBUFFER_WIDTH * sizeof(u32),
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    }

    g_Palettes = ArenaAllocator::PushArray<Palette>(ARENA_PERMANENT, PALETTE_COUNT);
    g_Sprites = ArenaAllocator::PushArray<Sprite>(ARENA_PERMANENT, MAX_SPRITE_COUNT);
    g_ChrSheets = ArenaAllocator::PushArray<ChrSheet>(ARENA_PERMANENT, CHR_COUNT);
    g_Nametables = ArenaAllocator::PushArray<Nametable>(ARENA_PERMANENT, NAMETABLE_COUNT);
    g_Scanlines = ArenaAllocator::PushArray<Scanline>(ARENA_PERMANENT, SCANLINE_COUNT);

    g_paletteColors = ArenaAllocator::PushArray<u32>(ARENA_PERMANENT, COLOR_COUNT);
    Rendering::Util::GeneratePaletteColors(g_paletteColors);
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
    ClearFramebuffer(g_Framebuffers[0]);
    Draw(g_Framebuffers[0]);
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
