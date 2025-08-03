#include "rendering_software.h"

#ifdef USE_SOFTWARE_FALLBACK

#include "rendering_util.h"
#include "debug.h"
#include <cstring>
#include <cstdlib>

namespace SoftwareRenderer
{
	static bool initialized = false;
	static u32 imageWidth = 0;
	static u32 imageHeight = 0;
	static u8* imageData = nullptr;
	static u32* paletteColors = nullptr;

	// Scanline data structure that matches the compute shader
	struct ScanlineData {
		u32 spriteCount;
		u32 spriteIndices[MAX_SPRITES_PER_SCANLINE];
		s32 scrollX;
		s32 scrollY;
	};

	static ScanlineData* scanlineData = nullptr;

	void Init(u32 width, u32 height) {
		if (initialized) {
			Free();
		}

		imageWidth = width;
		imageHeight = height;
		
		// Allocate RGB image buffer
		imageData = (u8*)calloc(width * height * 3, sizeof(u8));
		if (!imageData) {
			DEBUG_ERROR("Failed to allocate software renderer image buffer");
			return;
		}

		// Allocate palette color lookup table (assuming 128 colors like in core_types.h)
		paletteColors = (u32*)calloc(COLOR_COUNT, sizeof(u32));
		if (!paletteColors) {
			DEBUG_ERROR("Failed to allocate software renderer palette buffer");
			free(imageData);
			imageData = nullptr;
			return;
		}

		// Allocate scanline data
		scanlineData = (ScanlineData*)calloc(height, sizeof(ScanlineData));
		if (!scanlineData) {
			DEBUG_ERROR("Failed to allocate software renderer scanline buffer");
			free(imageData);
			free(paletteColors);
			imageData = nullptr;
			paletteColors = nullptr;
			return;
		}

		initialized = true;
	}

	void Free() {
		if (imageData) {
			free(imageData);
			imageData = nullptr;
		}
		if (paletteColors) {
			free(paletteColors);
			paletteColors = nullptr;
		}
		if (scanlineData) {
			free(scanlineData);
			scanlineData = nullptr;
		}
		initialized = false;
	}

	bool IsEnabled() {
		return initialized;
	}

	// Helper functions that match the compute shader logic

	static u32 ReadNametableWord(const Nametable* nametables, s32 index, s32 tileOffset) {
		s32 coarse = tileOffset >> 1; // Offset to 2 byte block
		s32 fine = tileOffset & 1; // byte index (1 bit)
		
		if (index < 0 || index >= NAMETABLE_COUNT) {
			return 0;
		}

		const u32* nametableData = (const u32*)&nametables[index].tiles[0];
		u32 result = nametableData[coarse];
		
		// Extract 16 bits starting at bit position (16 * fine)
		return (result >> (16 * fine)) & 0xFFFF;
	}

	static u32 ReadCHRTile(const ChrSheet* chrSheets, u32 chrIndex, u32 tileIndex, u32 offset, u32 flipX, u32 flipY) {
		if (flipX != 0) {
			offset ^= 7;
		}
		if (flipY != 0) {
			offset ^= 56;
		}

		if (chrIndex >= CHR_COUNT || tileIndex >= 0x100) {
			return 0;
		}

		// No 64bit ints in original shader, so tiles split in half
		u32 whichHalf = offset >> 5;  // Divide by 32 (2^5)
		u32 bitOffset = offset & 31;  // Modulo 32 (2^5 - 1)

		// Fetch the tile data (each plane is split into two 32-bit parts)
		const ChrTile& tile = chrSheets[chrIndex].tiles[tileIndex];
		
		// Need to split the 64-bit values into 32-bit halves
		u32 t0, t1, t2;
		if (whichHalf == 0) {
			t0 = (u32)(tile.p0 & 0xFFFFFFFF);
			t1 = (u32)(tile.p1 & 0xFFFFFFFF);
			t2 = (u32)(tile.p2 & 0xFFFFFFFF);
		} else {
			t0 = (u32)(tile.p0 >> 32);
			t1 = (u32)(tile.p1 >> 32);
			t2 = (u32)(tile.p2 >> 32);
		}

		// Extract the bit from each plane
		u32 result = (t0 >> bitOffset) & 1;
		result |= ((t1 >> bitOffset) & 1) << 1;
		result |= ((t2 >> bitOffset) & 1) << 2;

		return result;
	}

	static u32 ReadPaletteTable(const Palette* palettes, u32 tableIndex, u32 colorIndex) {
		if (tableIndex >= PALETTE_COUNT || colorIndex >= PALETTE_COLOR_COUNT) {
			return 0;
		}

		return palettes[tableIndex].colors[colorIndex];
	}

	static void Scroll(const ScanlineData& scanline, s32 screenX, s32 screenY, s32* scrollX, s32* scrollY, s32* nametableIndex) {
		*scrollX = screenX + scanline.scrollX;
		*scrollY = screenY + scanline.scrollY;
		
		const s32 nametableSizePixels = 512;
		
		s32 nametableIndexX = *scrollX / nametableSizePixels;
		s32 nametableIndexY = *scrollY / nametableSizePixels;
		
		if (*scrollX < 0) {
			nametableIndexX -= 1;
		}
		if (*scrollY < 0) {
			nametableIndexY -= 1;
		}
		
		*nametableIndex = (nametableIndexX + nametableIndexY) % 2;
		
		*scrollX = *scrollX % nametableSizePixels;
		*scrollY = *scrollY % nametableSizePixels;
		
		if (*scrollX < 0) *scrollX += nametableSizePixels;
		if (*scrollY < 0) *scrollY += nametableSizePixels;
	}

	static void RenderBackground(s32 screenX, s32 screenY, const Nametable* nametables, const ChrSheet* chrSheets, s32* colorIndex, s32* palette) {
		const ScanlineData& scanline = scanlineData[screenY];
		s32 scrollX, scrollY, nametableIndex;
		Scroll(scanline, screenX, screenY, &scrollX, &scrollY, &nametableIndex);

		s32 coarse2dX = scrollX / 8; // position of tile
		s32 coarse2dY = scrollY / 8;
		s32 coarse = coarse2dX + coarse2dY * 64; // Should be 12 bits (yyyyyyxxxxxx)
		s32 fine2dX = scrollX % 8; // position of pixel within tile
		s32 fine2dY = scrollY % 8;
		s32 fine = fine2dX + fine2dY * 8; // Should be 6 bits (yyyxxx)

		u32 tile = ReadNametableWord(nametables, nametableIndex, coarse);
		u32 tileIndex = tile & 0x3FF; // Extract 10 bits
		*palette = (tile >> 10) & 0x7; // Extract 3 bits
		u32 flipX = (tile >> 14) & 1;
		u32 flipY = (tile >> 15) & 1;
		
		u32 chrPage = (tileIndex >> 8) & 3;
		u32 pageTile = tileIndex & 0xFF;
		*colorIndex = ReadCHRTile(chrSheets, chrPage, pageTile, fine, flipX, flipY);

		// Don't draw transparent pixels
		if (*colorIndex == 0) {
			*colorIndex = 0;
			*palette = 0;
		}
	}

	static void RenderSprites(s32 screenX, s32 screenY, const Sprite* sprites, const ChrSheet* chrSheets, s32* colorIndex, s32* palette) {
		const ScanlineData& scanline = scanlineData[screenY];
		u32 spriteCount = (scanline.spriteCount < MAX_SPRITES_PER_SCANLINE) ? scanline.spriteCount : MAX_SPRITES_PER_SCANLINE;

		u32 minSpriteIndex = 0xFFFFFFFF;

		for (u32 i = 0; i < spriteCount; i++) {
			u32 spriteIndex = scanline.spriteIndices[i];
			if (spriteIndex >= MAX_SPRITE_COUNT) continue;
			
			const Sprite& sprite = sprites[spriteIndex];
			s32 spriteX = sprite.x;
			s32 spriteY = sprite.y;
			
			// Is pixel inside sprite?
			if (screenX >= spriteX && screenX < spriteX + 8) {
				u32 tileId = sprite.tileId;
				u32 palTableIndex = sprite.palette + 8; // SPRITE_PALETTE_OFFSET
				u32 priority = sprite.priority;
				u32 flipX = sprite.flipHorizontal;
				u32 flipY = sprite.flipVertical;

				s32 fine2dX = screenX - spriteX;
				s32 fine2dY = screenY - spriteY;
				s32 fine = fine2dX + fine2dY * 8;

				u32 chrPage = (tileId >> 8) & 3;
				u32 pageTile = tileId & 0xFF;
				u32 spriteColorIndex = ReadCHRTile(chrSheets, chrPage + 4, pageTile, fine, flipX, flipY);

				// Don't draw transparent pixels
				if (spriteColorIndex == 0) {
					continue;
				}

				// Sprites aren't guaranteed to be sorted so need to check
				if (minSpriteIndex < spriteIndex) {
					continue;
				}
				
				minSpriteIndex = spriteIndex;

				// Draw sprites on top of bg if priority is 0 or if the bg pixel is not opaque
				if (priority == 0 || *colorIndex == 0) {
					*colorIndex = spriteColorIndex;
					*palette = palTableIndex;
				}
			}
		}
	}

	// Evaluate scanlines (matches scanline_evaluate.comp functionality)
	static void EvaluateScanlines(const Sprite* sprites, u32 spriteCount, const Scanline* scanlines) {
		// Clear scanline data
		for (u32 y = 0; y < imageHeight; y++) {
			scanlineData[y].spriteCount = 0;
			scanlineData[y].scrollX = scanlines[y].scrollX;
			scanlineData[y].scrollY = scanlines[y].scrollY;
		}

		// Build sprite lists per scanline
		for (u32 spriteIndex = 0; spriteIndex < spriteCount; spriteIndex++) {
			const Sprite& sprite = sprites[spriteIndex];
			u32 spriteY = sprite.y;
			
			// Check which scanlines this sprite affects (8 pixels tall)
			for (u32 y = spriteY; y < spriteY + 8 && y < imageHeight; y++) {
				if (scanlineData[y].spriteCount < MAX_SPRITES_PER_SCANLINE) {
					scanlineData[y].spriteIndices[scanlineData[y].spriteCount] = spriteIndex;
					scanlineData[y].spriteCount++;
				}
			}
		}
	}

	// Generate palette colors lookup table (simplified)
	static void GeneratePaletteColors() {
		// This is a simplified palette - in a real implementation you'd want the actual NES palette
		// This creates a basic gradient for testing
		for (u32 i = 0; i < COLOR_COUNT; i++) {
			// Create a basic color palette similar to NES palette structure
			u8 r = (u8)((i & 0x30) << 2) | ((i & 0x0C) << 2) | (i & 0x03);
			u8 g = (u8)((i & 0x30) << 1) | ((i & 0x0C) << 1) | (i & 0x03);
			u8 b = (u8)((i & 0x30) << 0) | ((i & 0x0C) << 0) | (i & 0x03);
			paletteColors[i] = (r << 16) | (g << 8) | b;
		}
	}

	u8* RenderFrame(
		const Palette* palettes,
		const ChrSheet* chrSheets,
		const Nametable* nametables,
		const Sprite* sprites,
		const Scanline* scanlines,
		u32 spriteCount
	) {
		if (!initialized) {
			return nullptr;
		}

		// Generate palette colors (should probably be done once at init, but this works for now)
		GeneratePaletteColors();

		// Evaluate scanlines first (matches the compute shader pipeline)
		EvaluateScanlines(sprites, spriteCount, scanlines);

		// Render each pixel
		for (u32 y = 0; y < imageHeight; y++) {
			for (u32 x = 0; x < imageWidth; x++) {
				s32 colorIndex = 0;
				s32 palette = 0;

				// Render background
				RenderBackground(x, y, nametables, chrSheets, &colorIndex, &palette);

				// Render sprites on top
				RenderSprites(x, y, sprites, chrSheets, &colorIndex, &palette);

				// Get final color
				u32 paletteIndex = ReadPaletteTable(palettes, palette, colorIndex);
				u32 finalColor = paletteColors[paletteIndex];

				// Store in RGB format
				u32 pixelIndex = (y * imageWidth + x) * 3;
				imageData[pixelIndex + 0] = (finalColor >> 16) & 0xFF; // R
				imageData[pixelIndex + 1] = (finalColor >> 8) & 0xFF;  // G
				imageData[pixelIndex + 2] = finalColor & 0xFF;         // B
			}
		}

		return imageData;
	}

	void GetImageDimensions(u32* width, u32* height) {
		if (width) *width = imageWidth;
		if (height) *height = imageHeight;
	}

	u8* GetImageData() {
		return imageData;
	}
}

#endif // USE_SOFTWARE_FALLBACK