#ifdef PLATFORM_WINDOWS
    #include <windows.h>
#elif PLATFORM_LINUX
    #include <pthread.h>
#endif

#include "software_renderer.h"
#include "debug.h"
#include "memory_arena.h"
#include <thread>
#include <condition_variable>
#include <immintrin.h>
#include <gtc/constants.hpp>

struct ScanlineSpriteInfo {
    u16 spriteCount;
    u16 spriteIndices[MAX_SPRITES_PER_SCANLINE];
};

static u32* g_Framebuffer = nullptr;

static Palette* g_Palettes;
static Sprite* g_Sprites;
static ChrSheet* g_ChrSheets;
static Nametable* g_Nametables;
static Scanline* g_Scanlines;

u32* g_paletteColors;

// Temporary storage
static ScanlineSpriteInfo* g_EvaluatedScanlines = nullptr;
static u8* g_SampledPixels = nullptr;

// Threading
constexpr u32 MAX_WORKER_TASK_COUNT = 64;

struct RenderWorkerTask {
    u32 scanlineCount;
    u32 scanlineOffset;
};

static RenderWorkerTask g_WorkerTasks[MAX_WORKER_TASK_COUNT];
static u32 g_ActiveTaskCount = 0;

static u32 g_WorkerCount = 0;
static u32 g_ActiveWorkers = 0;
static std::thread* g_WorkerThreads = nullptr;

static std::condition_variable g_WorkerCondition;
static std::condition_variable g_AllWorkDoneCondition;
static std::mutex g_WorkerMutex;
static bool g_StopWorkers = false;

// Modulo function that handles negative values
template<typename T>
inline static T Mod(T a, T b) {
    return (a % b + b) % b;
}

inline u8 Reverse8(u8 value) {
    return (value * 0x0202020202ULL & 0x010884422010ULL) % 1023;
}

static inline void SampleChrTileRow(u16 tileId, u8 y, u8 palette, bool flipX, bool flipY, u8 start, u8 end, u8* outRow) {
    const ChrTile* pFlatChrTiles = (ChrTile*)g_ChrSheets;
    const ChrTile& tile = pFlatChrTiles[tileId];

    const u8 row = flipY ? y ^ 7 : y;
    u8 p0 = (tile.p0 >> (row * 8)) & 0xFF;
    u8 p1 = (tile.p1 >> (row * 8)) & 0xFF;
    u8 p2 = (tile.p2 >> (row * 8)) & 0xFF;

    if (flipX) {
        p0 = Reverse8(p0);
        p1 = Reverse8(p1);
        p2 = Reverse8(p2);
    }

    u32 x = 0;
    for (u32 offset = start; offset < end; offset++) {
        u8 ci = ((p0 >> offset) & 1) | (((p1 >> offset) & 1) << 1) | (((p2 >> offset) & 1) << 2);
        outRow[x++] = (ci == 0) ? 0 : (PALETTE_COLOR_COUNT * palette + ci);
    }
}

static inline void EvaluateScanlineSprites() {
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

static inline void ResolvePaletteColors(const u8* pSamples, u32* pOutColors, u32 count) {
    for (u32 i = 0; i < count; i++) {
        const u8& sample = pSamples[i];
        u8 colorIndex = ((u8*)g_Palettes)[sample];
        u32 color = g_paletteColors[colorIndex];
        pOutColors[i] = color;
    }
}

static inline void ResolvePaletteColorsAVX(const u8* pSamples, u32* pOutColors, u32 count) {
    for (u32 i = 0; i < count; i += 8) {
        __m128i idx8 = _mm_loadl_epi64((const __m128i*)(pSamples + i));
        __m256i idx32 = _mm256_cvtepu8_epi32(idx8);
        __m256i intIndices = _mm256_srli_epi32(idx32, 2);
        __m256i paletteWords = _mm256_i32gather_epi32((const int*)g_Palettes, intIndices, 4);
        __m256i byteOffsets = _mm256_and_si256(idx32, _mm256_set1_epi32(3));
        __m256i shiftAmounts = _mm256_slli_epi32(byteOffsets, 3);
        __m256i shifted = _mm256_srlv_epi32(paletteWords, shiftAmounts);
        __m256i colorIndices = _mm256_and_si256(shifted, _mm256_set1_epi32(0xFF));
        __m256i finalColors = _mm256_i32gather_epi32((const int*)g_paletteColors, colorIndices, 4);
        _mm256_storeu_si256((__m256i*)(pOutColors + i), finalColors);
    }
}

static inline const BgTile* GetNametableTile(s32 x, s32 y) {
    const BgTile* pFlatBgTiles = (BgTile*)g_Nametables;

    const s32 nametableIndex = ((y / NAMETABLE_DIM_PIXELS) + (x / NAMETABLE_DIM_PIXELS)) & 1;
    const s32 offsetX = Mod(x, s32(NAMETABLE_DIM_PIXELS));
    const s32 offsetY = Mod(y, s32(NAMETABLE_DIM_PIXELS));
    const s32 tileIndex = (offsetY / TILE_DIM_PIXELS) * NAMETABLE_DIM_TILES + (offsetX / TILE_DIM_PIXELS);

    return &pFlatBgTiles[nametableIndex * NAMETABLE_SIZE_TILES + tileIndex];
}

static void DrawScanlines(u32 count, u32 offset) {
    const u32 framebufferOffset = offset * SOFTWARE_FRAMEBUFFER_WIDTH;
    const Scanline* pScanlines = g_Scanlines + offset;
    u8* pSamples = g_SampledPixels + framebufferOffset;

    // Step 1: Sample background
    for (u32 i = 0; i < count; i++) {
        const Scanline& scanline = pScanlines[i];
        u8* pScanlineSamples = pSamples + i * SOFTWARE_FRAMEBUFFER_WIDTH;

        const s32 pixelY = i + offset;
        const s32 scrolledY = pixelY + scanline.scrollY;
        const s32 tileOffsetY = Mod(scrolledY, s32(TILE_DIM_PIXELS));

        // Handle partial left tile row
        s32 scrolledX = scanline.scrollX;
        s32 tileOffsetX = Mod(scrolledX, s32(TILE_DIM_PIXELS));
        s32 x = 0;
        if (tileOffsetX != 0) {
            const BgTile* pTile = GetNametableTile(scrolledX, scrolledY);
            SampleChrTileRow(pTile->tileId, tileOffsetY, pTile->palette, pTile->flipHorizontal, pTile->flipVertical, tileOffsetX, 8, &pScanlineSamples[x]);
            x += 8 - tileOffsetX;
        }

        // Handle full tile rows
        while (x + TILE_DIM_PIXELS <= SOFTWARE_FRAMEBUFFER_WIDTH) {
            scrolledX = x + scanline.scrollX;
            const BgTile* pTile = GetNametableTile(scrolledX, scrolledY);

            SampleChrTileRow(pTile->tileId, tileOffsetY, pTile->palette, pTile->flipHorizontal, pTile->flipVertical, 0, 8, &pScanlineSamples[x]);
            x += TILE_DIM_PIXELS;
        }

        // Handle partial right tile row
        if (x < SOFTWARE_FRAMEBUFFER_WIDTH) {
            scrolledX = x + scanline.scrollX;
            const BgTile* pTile = GetNametableTile(scrolledX, scrolledY);
            SampleChrTileRow(pTile->tileId, tileOffsetY, pTile->palette, pTile->flipHorizontal, pTile->flipVertical, 0, SOFTWARE_FRAMEBUFFER_WIDTH - x, &pScanlineSamples[x]);
        }
    }

    // Step 2: Sample sprites
    for (u32 i = 0; i < count; i++) {
        u8* pScanlineSamples = pSamples + i * SOFTWARE_FRAMEBUFFER_WIDTH;

        const s32 pixelY = i + offset;
        const ScanlineSpriteInfo& scanlineInfo = g_EvaluatedScanlines[pixelY];

        if (scanlineInfo.spriteCount == 0) {
            continue;
        }

        // Iterate backwards so that lower index sprites have priority
        for (s32 j = scanlineInfo.spriteCount - 1; j >= 0; j--) {
            const Sprite& sprite = g_Sprites[scanlineInfo.spriteIndices[j]];

            if (sprite.x + TILE_DIM_PIXELS < 0 || sprite.x >= SOFTWARE_FRAMEBUFFER_WIDTH) {
                continue;
            }

            s32 yOffset = pixelY - sprite.y;
            u8 start = sprite.x < 0 ? -sprite.x : 0;
            u8 end = sprite.x + TILE_DIM_PIXELS > SOFTWARE_FRAMEBUFFER_WIDTH ? SOFTWARE_FRAMEBUFFER_WIDTH - sprite.x : TILE_DIM_PIXELS;

            u8 rowWidth = end - start;
            u8 row[TILE_DIM_PIXELS] = {0};
            SampleChrTileRow(sprite.tileId + CHR_PAGE_COUNT * CHR_SIZE_TILES, yOffset, sprite.palette + BG_PALETTE_COUNT, sprite.flipHorizontal, sprite.flipVertical, start, end, row);

            for (s32 k = 0; k < rowWidth; k++) {
                if (row[k] != 0 && (sprite.priority == 0 || pScanlineSamples[sprite.x + k] == 0)) {
                    pScanlineSamples[sprite.x + k] = row[k];
                }
            }
        }
    }

    // Step 3: Write to framebuffer
    u32* pPixels = g_Framebuffer + framebufferOffset;
    const u32 pixelCount = SOFTWARE_FRAMEBUFFER_WIDTH * count;
    const u32 simdCount = pixelCount & ~7;
    const u32 remainder = pixelCount & 7;
    ResolvePaletteColorsAVX(pSamples, pPixels, simdCount);
    ResolvePaletteColors(pSamples + simdCount, pPixels + simdCount, remainder);
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
    g_Framebuffer = nullptr;
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

void Rendering::Software::Init() {
    g_Palettes = ArenaAllocator::PushArray<Palette>(ARENA_PERMANENT, PALETTE_COUNT);
    g_Sprites = ArenaAllocator::PushArray<Sprite>(ARENA_PERMANENT, MAX_SPRITE_COUNT);
    g_ChrSheets = ArenaAllocator::PushArray<ChrSheet>(ARENA_PERMANENT, CHR_COUNT);
    g_Nametables = ArenaAllocator::PushArray<Nametable>(ARENA_PERMANENT, NAMETABLE_COUNT);
    g_Scanlines = ArenaAllocator::PushArray<Scanline>(ARENA_PERMANENT, SCANLINE_COUNT);

    g_paletteColors = ArenaAllocator::PushArray<u32>(ARENA_PERMANENT, COLOR_COUNT);
    GeneratePaletteColors(g_paletteColors);

    g_EvaluatedScanlines = ArenaAllocator::PushArray<ScanlineSpriteInfo>(ARENA_PERMANENT, SCANLINE_COUNT);
    g_SampledPixels = ArenaAllocator::PushArray<u8>(ARENA_PERMANENT, SOFTWARE_FRAMEBUFFER_SIZE_PIXELS);

    g_WorkerCount = 4; // TODO: Determine optimal worker count based on hardware capabilities
    void* workerMemory = ArenaAllocator::Push(ARENA_PERMANENT, sizeof(std::thread) * g_WorkerCount, alignof(std::thread));
    g_WorkerThreads = (std::thread*)workerMemory;
    for (u32 i = 0; i < g_WorkerCount; i++) {
        new (&g_WorkerThreads[i]) std::thread(WorkerLoop);
    }
}

void Rendering::Software::Free() {
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
}

void Rendering::Software::DrawFrame(u32* framebuffer) {
    g_Framebuffer = framebuffer;
    memset(g_SampledPixels, 0, SOFTWARE_FRAMEBUFFER_SIZE_PIXELS);
    EvaluateScanlineSprites();
    Draw();
}

Palette* Rendering::Software::GetPalette(u32 paletteIndex) {
    if (paletteIndex >= PALETTE_COUNT) {
        return nullptr;
    }
    return &g_Palettes[paletteIndex];
}

Sprite* Rendering::Software::GetSprites(u32 offset) {
    if (offset >= MAX_SPRITE_COUNT) {
        return nullptr;
    }
    return &g_Sprites[offset];
}

ChrSheet* Rendering::Software::GetChrSheet(u32 sheetIndex) {
    if (sheetIndex >= CHR_COUNT) {
        return nullptr;
    }
    return &g_ChrSheets[sheetIndex];
}

Nametable* Rendering::Software::GetNametable(u32 index) {
    if (index >= NAMETABLE_COUNT) {
        return nullptr;
    }
    return &g_Nametables[index];
}

Scanline* Rendering::Software::GetScanline(u32 offset) {
    if (offset >= SCANLINE_COUNT) {
        return nullptr;
    }
    return &g_Scanlines[offset];
}

const u32* Rendering::Software::GetPaletteColors() {
    return g_paletteColors;
}

void Rendering::Software::GeneratePaletteColors(u32* data) {
    for (s32 i = 0; i < COLOR_COUNT; i++) {
        s32 hue = i & 0b1111;
        s32 brightness = (i & 0b1110000) >> 4;

        r32 y = (r32)brightness / 7;
        r32 u = 0.0f; 
        r32 v = 0.0f;

        if (hue != 0) {
            // No need to have multiple pure blacks and whites
            y = (r32)(brightness + 1) / 9;

            r32 angle = 2 * glm::pi<r32>() * (hue - 1) / 15;
            r32 radius = 0.5f * (1.0f - glm::abs(y - 0.5f) * 2);

            u = radius * glm::cos(angle);
            v = radius * glm::sin(angle);
        }

        // Convert YUV to RGB
        r32 r = y + v * 1.139883;
        r32 g = y - 0.394642 * u - 0.580622 * v;
        r32 b = y + u * 2.032062;

        r = glm::clamp(r, 0.0f, 1.0f);
        g = glm::clamp(g, 0.0f, 1.0f);
        b = glm::clamp(b, 0.0f, 1.0f);

        u32* pixel = data + i;
        u8* pixelBytes = (u8*)pixel;

        pixelBytes[0] = (u8)(r * 255);
        pixelBytes[1] = (u8)(g * 255);
        pixelBytes[2] = (u8)(b * 255);
        pixelBytes[3] = 255;
    }
}

void Rendering::Software::DrawPalette(const Palette* pPalette, u32* outPixels) {
    for (u32 i = 0; i < PALETTE_COLOR_COUNT; i++) {
        u8 colorIndex = pPalette->colors[i];
        outPixels[i] = g_paletteColors[colorIndex];
    }
}

void Rendering::Software::DrawChrSheet(const ChrSheet* pSheet, const Palette* pPalette, u32 stride, u32* outPixels) {
    for (u32 y = 0; y < CHR_DIM_PIXELS; y++) {
        for (u32 x = 0; x < CHR_DIM_PIXELS; x++) {
            u32 coarseX = x / 8;
            u32 coarseY = y / 8;
            u32 fineX = x % 8;
            u32 fineY = y % 8;
            u32 tileIndex = coarseY * 16 + coarseX;
            u32 outPixelIndex = y * stride + x;
            u32 tilePixelIndex = fineY * 8 + fineX;

            const ChrTile& tile = pSheet->tiles[tileIndex];
            u8 bit0 = (tile.p0 >> tilePixelIndex) & 1;
            u8 bit1 = (tile.p1 >> tilePixelIndex) & 1;
            u8 bit2 = (tile.p2 >> tilePixelIndex) & 1;
            u8 colorIndex = (bit0 << 0) | (bit1 << 1) | (bit2 << 2);
            outPixels[outPixelIndex] = g_paletteColors[pPalette->colors[colorIndex]];
        }
    }
}
