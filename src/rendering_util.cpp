#include "rendering_util.h"
#include "system.h"
#include "math.h"
#include <algorithm>
#include <stdio.h>

namespace Rendering
{
	namespace Util
	{
		struct RIFFHeader {
			char signature[4]; // Should be 'RIFF'
			u32 size;
			char type[4];
		};
		struct PaletteChunkHeader {
			char signature[4];
			u32 size;
			u16 version;
			u16 colorCount;
		};

		void CreateChrSheet(const char* fname, ChrSheet* outSheet) {
			u32 imgWidth, imgHeight;
			u16 bpp;
			char* imgData = LoadBitmapBytes(fname, imgWidth, imgHeight, bpp);

			if (imgWidth != CHR_DIM_PIXELS || imgHeight != CHR_DIM_PIXELS) {
				DEBUG_ERROR("Invalid chr image dimensions!\n");
			}

			if (bpp != 8) {
				DEBUG_ERROR("Invalid chr image format!\n");
			}

			for (u32 y = 0; y < imgHeight; y++) {
				for (u32 x = 0; x < imgWidth; x++) {
					u32 coarseX = x / 8;
					u32 coarseY = y / 8;
					u32 fineX = x % 8;
					u32 fineY = y % 8;
					u32 tileIndex = (15 - coarseY) * 16 + coarseX; // Tile 0 is top left instead of bottom left
					u32 inPixelIndex = y * imgWidth + x;
					u32 outPixelIndex = (7 - fineY) * 8 + fineX; // Also pixels go from top to bottom in this program, but bottom to top in bmp, so flip

					u8 pixel = imgData[inPixelIndex];

					ChrTile& tile = outSheet->tiles[tileIndex];
					tile.p0 = (tile.p0 & ~(1ULL << outPixelIndex)) | ((u64)(pixel & 0b00000001) << outPixelIndex);
					tile.p1 = (tile.p1 & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000010) >> 1) << outPixelIndex);
					tile.p2 = (tile.p2 & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000100) >> 2) << outPixelIndex);
				}
			}

			free(imgData);
		}

		void LoadPaletteColorsFromFile(const char* fname, u8* outColors) {
			u32 palFileSize;
			char* palData = AllocFileBytes(fname, palFileSize);

			if (palFileSize < PALETTE_MEMORY_SIZE) {
				DEBUG_ERROR("Invalid palette table file!\n");
			}

			memcpy(outColors, palData, PALETTE_MEMORY_SIZE);
			free(palData);
		}

		void GeneratePaletteColors(u32* data) {
			for (s32 i = 0; i < COLOR_COUNT; i++) {
				s32 hue = i & 0b1111;
				s32 brightness = (i & 0b1110000) >> 4;

				r32 y = (r32)brightness / 7;
				r32 u = 0.0f; 
				r32 v = 0.0f;

				if (hue != 0) {
					// No need to have multiple pure blacks and whites
					y = (r32)(brightness + 1) / 9;

					r32 angle = 2 * pi * (hue - 1) / 15;
					r32 radius = 0.5f * (1.0f - abs(y - 0.5f) * 2);

					u = radius * std::cos(angle);
					v = radius * std::sin(angle);
				}

				// Convert YUV to RGB
				float r = y + v * 1.139883;
				float g = y - 0.394642 * u - 0.580622 * v;
				float b = y + u * 2.032062;

				r = std::max(std::min(r, 1.0f), 0.0f);
				g = std::max(std::min(g, 1.0f), 0.0f);
				b = std::max(std::min(b, 1.0f), 0.0f);

				u32* pixel = data + i;
				u8* pixelBytes = (u8*)pixel;

				pixelBytes[0] = (u8)(r * 255);
				pixelBytes[1] = (u8)(g * 255);
				pixelBytes[2] = (u8)(b * 255);
				pixelBytes[3] = 0;
			}
		}

		void SavePaletteToFile(const char* fname) {
			u32 data[COLOR_COUNT];
			GeneratePaletteColors(data);
			const u32 dataSize = COLOR_COUNT * sizeof(u32);

			FILE* pFile;
			fopen_s(&pFile, fname, "wb");

			if (pFile == NULL) {
				DEBUG_ERROR("Failed to write palette file\n");
			}

			RIFFHeader header{};
			header.signature[0] = 'R';
			header.signature[1] = 'I';
			header.signature[2] = 'F';
			header.signature[3] = 'F';

			header.type[0] = 'P';
			header.type[1] = 'A';
			header.type[2] = 'L';
			header.type[3] = ' ';

			header.size = dataSize + sizeof(PaletteChunkHeader);

			fwrite(&header, sizeof(RIFFHeader), 1, pFile);

			PaletteChunkHeader chunk{};
			chunk.signature[0] = 'd';
			chunk.signature[1] = 'a';
			chunk.signature[2] = 't';
			chunk.signature[3] = 'a';
			
			chunk.size = dataSize;
			chunk.version = 0x0300;
			chunk.colorCount = COLOR_COUNT;

			fwrite(&chunk, sizeof(PaletteChunkHeader), 1, pFile);
			fwrite(data, dataSize, 1, pFile);

			fclose(pFile);
		}

		u8 GetPaletteIndexFromNametableTileAttrib(const Nametable& nametable, s32 xTile, s32 yTile) {
			s32 xBlock = xTile / 4;
			s32 yBlock = yTile / 4;
			s32 smallBlockOffset = (xTile % 4 / 2) + (yTile % 4 / 2) * 2;
			s32 blockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
			u8 attribute = nametable.attributes[blockIndex];
			u8 paletteIndex = (attribute >> (smallBlockOffset * 2)) & 0b11;

			return paletteIndex;
		}

		void CopyMetasprite(const Sprite* src, Sprite* dst, u32 count, IVec2 pos, bool hFlip, bool vFlip) {
			for (int i = 0; i < count; i++) {
				Sprite sprite = src[i];
				if (hFlip) {
					FlipSpritesHorizontal(&sprite, 1);
					sprite.x = sprite.x * -1 - TILE_DIM_PIXELS;
				}
				if (vFlip) {
					FlipSpritesVertical(&sprite, 1);
					sprite.y = sprite.y * -1 - TILE_DIM_PIXELS;
				}
				sprite.y += pos.y;
				sprite.x += pos.x;
				dst[i] = sprite;
			}
		}

		void FlipSpritesHorizontal(Sprite* spr, u32 count) {
			for (int i = 0; i < count; i++) {
				Sprite& sprite = spr[i];
				sprite.flipHorizontal = !sprite.flipHorizontal;
			}
		}
		void FlipSpritesVertical(Sprite* spr, u32 count) {
			for (int i = 0; i < count; i++) {
				Sprite& sprite = spr[i];
				sprite.flipVertical = !sprite.flipVertical;
			}
		}
		void SetSpritesPalette(Sprite* spr, u32 count, u8 palette) {
			for (int i = 0; i < count; i++) {
				Sprite& sprite = spr[i];
				sprite.palette = palette;
			}
		}
		void ClearSprites(Sprite* spr, u32 count) {
			for (int i = 0; i < count; i++) {
				Sprite& sprite = spr[i];

				// Really just moves the sprites off screen (This is how the NES clears sprites as well)
				sprite.y = 288;
			}
		}

		void CopyChrTiles(const ChrTile* src, ChrTile* dst, u32 count) {
			memcpy(dst, src, sizeof(ChrTile) * count);
		}
	}
}