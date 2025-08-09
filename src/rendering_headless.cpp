#ifdef HEADLESS

#include "rendering.h"
#include "core_types.h"
#include "memory_arena.h"
#include <cstring>

// Forward declaration to match the interface  
struct SDL_Window;

// Headless rendering - provides the same interface but does nothing

// Static memory pools to maintain data interface compatibility
static RenderSettings renderSettings = { true };
static Palette palettes[PALETTE_COUNT];
static Sprite sprites[MAX_SPRITE_COUNT];
static ChrSheet chrSheets[CHR_COUNT];
static Nametable nametables[NAMETABLE_COUNT];
static Scanline scanlines[SCANLINE_COUNT];

namespace Rendering
{
	void Init(SDL_Window* sdlWindow) {
		// Initialize memory pools to zero
		memset(&renderSettings, 0, sizeof(renderSettings));
		memset(palettes, 0, sizeof(palettes));
		memset(sprites, 0, sizeof(sprites));
		memset(chrSheets, 0, sizeof(chrSheets));
		memset(nametables, 0, sizeof(nametables));
		memset(scanlines, 0, sizeof(scanlines));
		
		renderSettings.useCRTFilter = true;
	}
	
	void Free() {
		// Nothing to free in headless mode
	}

	// Generic commands - all no-ops in headless mode
	void BeginFrame() {}
	void BeginRenderPass() {}
	void EndFrame() {}
	void WaitForAllCommands() {}
	void ResizeSurface(u32 width, u32 height) {}

	// Data access - return pointers to static memory pools
	RenderSettings* GetSettingsPtr() {
		return &renderSettings;
	}
	
	Palette* GetPalettePtr(u32 paletteIndex) {
		if (paletteIndex >= PALETTE_COUNT) return nullptr;
		return &palettes[paletteIndex];
	}
	
	Sprite* GetSpritesPtr(u32 offset) {
		if (offset >= MAX_SPRITE_COUNT) return nullptr;
		return &sprites[offset];
	}
	
	ChrSheet* GetChrPtr(u32 sheetIndex) {
		if (sheetIndex >= CHR_COUNT) return nullptr;
		return &chrSheets[sheetIndex];
	}
	
	Nametable* GetNametablePtr(u32 index) {
		if (index >= NAMETABLE_COUNT) return nullptr;
		return &nametables[index];
	}
	
	Scanline* GetScanlinePtr(u32 offset) {
		if (offset >= SCANLINE_COUNT) return nullptr;
		return &scanlines[offset];
	}
}

#endif // HEADLESS