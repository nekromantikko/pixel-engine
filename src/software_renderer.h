#pragma once
#include "core_types.h"

constexpr u32 SOFTWARE_FRAMEBUFFER_WIDTH = VIEWPORT_WIDTH_PIXELS;
constexpr u32 SOFTWARE_FRAMEBUFFER_HEIGHT = VIEWPORT_HEIGHT_PIXELS;
constexpr u32 SOFTWARE_FRAMEBUFFER_SIZE_PIXELS = SOFTWARE_FRAMEBUFFER_WIDTH * SOFTWARE_FRAMEBUFFER_HEIGHT;

namespace Rendering {
    namespace Software {
        void Init();
        void Free();

        void DrawFrame(u32* framebuffer);

        // Data access
        Palette* GetPalette(u32 paletteIndex);
        Sprite* GetSprites(u32 offset);
        ChrSheet* GetChrSheet(u32 sheetIndex);
        Nametable* GetNametable(u32 index);
        Scanline* GetScanline(u32 offset);
        const u32* GetPaletteColors();

        // Utils
        void GeneratePaletteColors(u32* data);
        void DrawPalette(const Palette* pPalette, u32* outPixels);
        void DrawChrSheet(const ChrSheet* pSheet, const Palette* pPalette, u32 stride, u32* outPixels);
    }
}