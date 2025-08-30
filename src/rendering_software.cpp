#ifdef PLATFORM_WINDOWS
    #include <windows.h>
#elif PLATFORM_LINUX
    #include <pthread.h>
#endif

#include "rendering.h"
#include "rendering_util.h"
#include "core_types.h"
#include "debug.h"
#include "memory_arena.h"
#include <chrono>
#include <thread>
#include <condition_variable>

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

struct ScanlineSpriteInfo {
    u16 spriteCount;
    u16 spriteIndices[MAX_SPRITES_PER_SCANLINE];
};

SDL_Window* g_SDLWindow = nullptr;
SDL_Surface* g_WindowSurface = nullptr;

Framebuffer* g_Framebuffer = nullptr;
SDL_Surface* g_FramebufferSurface = nullptr;

Palette* g_Palettes;
Sprite* g_Sprites;
ChrSheet* g_ChrSheets;
Nametable* g_Nametables;
Scanline* g_Scanlines;

u32* g_paletteColors;

// Temporary storage
ScanlineSpriteInfo* g_EvaluatedScanlines = nullptr;

// Threading
constexpr u32 MAX_WORKER_TASK_COUNT = 64;

struct RenderWorkerTask {
    u32 scanlineCount;
    u32 scanlineOffset;
};

RenderWorkerTask g_WorkerTasks[MAX_WORKER_TASK_COUNT];
u32 g_ActiveTaskCount = 0;

u32 g_WorkerCount = 0;
u32 g_ActiveWorkers = 0;
std::thread* g_WorkerThreads = nullptr;

std::condition_variable g_WorkerCondition;
std::condition_variable g_AllWorkDoneCondition;
std::mutex g_WorkerMutex;
bool g_StopWorkers = false;

static glm::uvec2 ScrollScreen(const Scanline& scanline, const glm::uvec2& pixelPos, u32& outNametableIndex) {
    const glm::uvec2 scanlineScroll = glm::uvec2(scanline.scrollX, scanline.scrollY);
    glm::uvec2 scrolledPos = pixelPos + scanlineScroll;
    glm::uvec2 nametablePos = scrolledPos / NAMETABLE_DIM_PIXELS;
    outNametableIndex = (nametablePos.x + nametablePos.y) % 2;
    return scrolledPos % NAMETABLE_DIM_PIXELS;
}

static u8 SampleChrTile(u8 chrPage, u8 chrTileIndex, u32 pixelOffset, bool flipX, bool flipY) {
    if (flipX) pixelOffset ^= 7;
    if (flipY) pixelOffset ^= 56;

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
    const u8 chrPage = (bgTile.tileId >> 8) & 3;
    const u8 chrTileIndex = bgTile.tileId & 0xFF;
    u8 colorIndex = SampleChrTile(chrPage, chrTileIndex, pixelIndex, bgTile.flipHorizontal, bgTile.flipVertical);

    if (colorIndex == 0) {
        return { 0, 0 }; // Transparent
    }

    return { u8(bgTile.palette), colorIndex };
}

static void ProcessSprite(const Sprite& sprite, const glm::uvec2& pixelPos, ColorSample& outColorSample) {
    if (pixelPos.x < sprite.x || pixelPos.x >= sprite.x + TILE_DIM_PIXELS) {
        return;
    }

    const glm::uvec2 spritePixelPos = pixelPos - glm::uvec2(sprite.x, sprite.y);
    const u32 spritePixelIndex = spritePixelPos.x + spritePixelPos.y * TILE_DIM_PIXELS;
    
    const u8 chrPage = (sprite.tileId >> 8) & 3;
    const u8 chrTileIndex = sprite.tileId & 0xFF;
    u8 colorIndex = SampleChrTile(chrPage + 4, chrTileIndex, spritePixelIndex, sprite.flipHorizontal, sprite.flipVertical);

    if (colorIndex == 0) {
        return;
    }

    if (sprite.priority == 0 || outColorSample.colorIndex == 0) {
        outColorSample.paletteIndex = u8(sprite.palette + FG_PALETTE_COUNT);
        outColorSample.colorIndex = colorIndex;
    }
}

static void SampleSprites(const glm::uvec2& pixelPos, ColorSample& outColorSample) {
    const ScanlineSpriteInfo& scanlineInfo = g_EvaluatedScanlines[pixelPos.y];

    if (scanlineInfo.spriteCount == 0) {
        return;
    }

    // Iterate backwards so that lower index sprites have priority
    for (s32 i = scanlineInfo.spriteCount - 1; i >= 0; i--) {
        const Sprite& sprite = g_Sprites[scanlineInfo.spriteIndices[i]];
        ProcessSprite(sprite, pixelPos, outColorSample);
    }
}

static void EvaluateScanlineSprites() {
    memset(g_EvaluatedScanlines, 0, sizeof(ScanlineSpriteInfo) * SCANLINE_COUNT);
    for (u32 i = 0; i < MAX_SPRITE_COUNT; i++) {
        const Sprite& sprite = g_Sprites[i];
        for (s32 y = sprite.y; y < sprite.y + TILE_DIM_PIXELS; y++) {
            if (y < 0 || y >= SCANLINE_COUNT) {
                continue;
            }

            ScanlineSpriteInfo& info = g_EvaluatedScanlines[y];
            if (info.spriteCount < MAX_SPRITES_PER_SCANLINE) {
                info.spriteIndices[info.spriteCount++] = i;
            }
        }
    }
}

static void DrawScanlines(u32 count, u32 offset) {
    u32* pPixels = g_Framebuffer->pixels;
    for (u32 y = offset; y < offset + count; y++) {
        Scanline& scanline = g_Scanlines[y];
        for (u32 x = 0; x < FRAMEBUFFER_WIDTH; x++) {
            // Apply scroll
            u32 nametableIndex;
            glm::uvec2 pixelPos = glm::uvec2(x, y);
            glm::uvec2 scrolledPos = ScrollScreen(scanline, pixelPos, nametableIndex);

            ColorSample colorSample = SampleBackground(scrolledPos, nametableIndex);
            SampleSprites(pixelPos, colorSample);

            // Fetch color from palette
            u8 colorIndex = g_Palettes[colorSample.paletteIndex].colors[colorSample.colorIndex];
            u32 color = g_paletteColors[colorIndex];

            // Set pixel in framebuffer
            pPixels[y * FRAMEBUFFER_WIDTH + x] = color;
        }
    }
}

static void Draw() {
    const u32 scanlinesPerThread = SCANLINE_COUNT / g_WorkerCount;

    {
        std::unique_lock<std::mutex> lock(g_WorkerMutex);
        for (u32 i = 0; i < g_WorkerCount; i++) {
            g_WorkerTasks[g_ActiveTaskCount++] = { scanlinesPerThread, i * scanlinesPerThread };
        }
        g_WorkerCondition.notify_all();
    }

    {
        std::unique_lock<std::mutex> lock(g_WorkerMutex);
        while (g_ActiveWorkers > 0 || g_ActiveTaskCount > 0) {
            g_AllWorkDoneCondition.wait(lock);
        }
    }

}

static void WorkerLoop() {
#ifdef PLATFORM_WINDOWS
    SetThreadDescription(GetCurrentThread(), L"RenderWorker");
#elif PLATFORM_LINUX
    pthread_setname_np(pthread_self(), "RenderWorker");
#endif

    while (true) {
        RenderWorkerTask task;
        {
            std::unique_lock<std::mutex> lock(g_WorkerMutex);
            while (g_ActiveTaskCount == 0 && !g_StopWorkers) {
                g_WorkerCondition.wait(lock);
            }

            if (g_StopWorkers) {
                return;
            }

            task = g_WorkerTasks[--g_ActiveTaskCount];
            g_ActiveWorkers++;
        } // Lock goes out of scope here

        DrawScanlines(task.scanlineCount, task.scanlineOffset);

        {
            std::unique_lock<std::mutex> lock(g_WorkerMutex);
            g_ActiveWorkers--;
            if (g_ActiveWorkers == 0 && g_ActiveTaskCount == 0) {
                g_AllWorkDoneCondition.notify_all();
            }
        }
    }
}

void Rendering::Init(SDL_Window* sdlWindow) {
    g_SDLWindow = sdlWindow;
    g_WindowSurface = SDL_GetWindowSurface(sdlWindow);
    if (!g_WindowSurface) {
        DEBUG_FATAL("Failed to get window surface\n");
    }

    g_Framebuffer = ArenaAllocator::Push<Framebuffer>(ARENA_PERMANENT);
    g_FramebufferSurface = SDL_CreateRGBSurfaceFrom(g_Framebuffer->pixels, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, 32, FRAMEBUFFER_WIDTH * sizeof(u32),
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

    g_Palettes = ArenaAllocator::PushArray<Palette>(ARENA_PERMANENT, PALETTE_COUNT);
    g_Sprites = ArenaAllocator::PushArray<Sprite>(ARENA_PERMANENT, MAX_SPRITE_COUNT);
    g_ChrSheets = ArenaAllocator::PushArray<ChrSheet>(ARENA_PERMANENT, CHR_COUNT);
    g_Nametables = ArenaAllocator::PushArray<Nametable>(ARENA_PERMANENT, NAMETABLE_COUNT);
    g_Scanlines = ArenaAllocator::PushArray<Scanline>(ARENA_PERMANENT, SCANLINE_COUNT);

    g_paletteColors = ArenaAllocator::PushArray<u32>(ARENA_PERMANENT, COLOR_COUNT);
    Rendering::Util::GeneratePaletteColors(g_paletteColors);

    g_EvaluatedScanlines = ArenaAllocator::PushArray<ScanlineSpriteInfo>(ARENA_PERMANENT, SCANLINE_COUNT);

    g_WorkerCount = 4; // TODO: Determine optimal worker count based on hardware capabilities
    void* workerMemory = ArenaAllocator::Push(ARENA_PERMANENT, sizeof(std::thread) * g_WorkerCount, alignof(std::thread));
    g_WorkerThreads = (std::thread*)workerMemory;
    for (u32 i = 0; i < g_WorkerCount; i++) {
        new (&g_WorkerThreads[i]) std::thread(WorkerLoop);
    }
}

void Rendering::Free() {
    {
        std::unique_lock<std::mutex> lock(g_WorkerMutex);
        g_StopWorkers = true;
    }
    g_WorkerCondition.notify_all();

    for (u32 i = 0; i < g_WorkerCount; i++) {
        if (g_WorkerThreads[i].joinable()) {
            g_WorkerThreads[i].join();
        }
    }

    SDL_FreeSurface(g_FramebufferSurface);
}

void Rendering::BeginFrame() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::BeginRenderPass() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::EndFrame() {
    auto t1 = std::chrono::high_resolution_clock::now();
    EvaluateScanlineSprites();
    auto t2 = std::chrono::high_resolution_clock::now();
    Draw();
    auto t3 = std::chrono::high_resolution_clock::now();
    SDL_BlitScaled(g_FramebufferSurface, nullptr, g_WindowSurface, nullptr);
    auto t4 = std::chrono::high_resolution_clock::now();
    SDL_UpdateWindowSurface(g_SDLWindow);

    DEBUG_LOG("Evaluate time: %lldus\n", std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
    DEBUG_LOG("Draw time: %lldus\n", std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count());
    DEBUG_LOG("Blit time: %lldus\n", std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count());
    DEBUG_LOG("FRAME END\n");
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
