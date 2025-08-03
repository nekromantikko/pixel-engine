#pragma once
#include "typedef.h"
#include "core_types.h"

#ifdef USE_SOFTWARE_FALLBACK

namespace SoftwareRenderer
{
	// Initialize the software renderer
	void Init(u32 width, u32 height);
	
	// Cleanup software renderer resources
	void Free();

	// Check if software rendering is enabled/available
	bool IsEnabled();

	// Render a frame using software rendering
	// Returns pointer to the rendered image data (RGB format)
	u8* RenderFrame(
		const Palette* palettes,
		const ChrSheet* chrSheets,
		const Nametable* nametables,
		const Sprite* sprites,
		const Scanline* scanlines,
		u32 spriteCount
	);

	// Get the rendered image dimensions
	void GetImageDimensions(u32* width, u32* height);

	// Get the rendered image data pointer
	u8* GetImageData();
}

#endif // USE_SOFTWARE_FALLBACK