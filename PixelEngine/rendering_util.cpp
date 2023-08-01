#include "rendering_util.h"
#include "system.h"

namespace Rendering
{
	namespace Util
	{
		void CreateChrSheet(const char* fname, CHRSheet* outSheet) {
			u32 imgWidth, imgHeight;
			u16 bpp;
			char* imgData = LoadBitmapBytes(fname, imgWidth, imgHeight, bpp);

			if (imgWidth != 128 || imgHeight != 128) {
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
					outSheet->p0[tileIndex] = (outSheet->p0[tileIndex] & ~(1ULL << outPixelIndex)) | ((u64)(pixel & 0b00000001) << outPixelIndex);
					outSheet->p1[tileIndex] = (outSheet->p1[tileIndex] & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000010) >> 1) << outPixelIndex);
					outSheet->p2[tileIndex] = (outSheet->p2[tileIndex] & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000100) >> 2) << outPixelIndex);
				}
			}

			free(imgData);
		}

		u8 GetPaletteIndexFromNametableTileAttrib(u8* pNametable, s32 xTile, s32 yTile) {
			s32 xBlock = xTile / 4;
			s32 yBlock = yTile / 4;
			s32 smallBlockOffset = (xTile % 4 / 2) + (yTile % 4 / 2) * 2;
			s32 blockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
			s32 nametableOffset = NAMETABLE_ATTRIBUTE_OFFSET + blockIndex;
			u8 attribute = pNametable[nametableOffset];
			u8 paletteIndex = (attribute >> (smallBlockOffset * 2)) & 0b11;

			return paletteIndex;
		}

		void WriteMetasprite(RenderContext* pContext, Sprite* sprites, u32 count, u32 offset, IVec2 pos, bool hFlip, bool vFlip) {
			// Could probably avoid dynamic memory here by being smarter about it
			Sprite* outSprites = (Sprite*)calloc(count, sizeof(Sprite));

			for (int i = 0; i < count; i++) {
				Sprite sprite = sprites[i];
				if (hFlip) {
					sprite.attributes = sprite.attributes ^ 0b01000000;
					sprite.x = sprite.x * -1 - TILE_SIZE;
				}
				if (vFlip) {
					sprite.attributes = sprite.attributes ^ 0b10000000;
					sprite.y = sprite.y * -1 - TILE_SIZE;
				}
				sprite.y += pos.y;
				sprite.x += pos.x;
				outSprites[i] = sprite;
			}

			WriteSprites(pContext, count, offset, outSprites);
			free(outSprites);
		}

		void WriteChrTiles(RenderContext* pContext, bool sheetIndex, u32 tileCount, u8 srcOffset, u8 dstOffset, CHRSheet* sheet) {
			Rendering::WriteChrMemory(pContext, sizeof(u64)*tileCount, CHR_SHEET_SIZE*sheetIndex + sizeof(u64)*dstOffset, (u8*)sheet + sizeof(u64)*srcOffset);
			Rendering::WriteChrMemory(pContext, sizeof(u64) * tileCount, CHR_SHEET_SIZE * sheetIndex + sizeof(u64) * dstOffset + 0x800, (u8*)sheet + sizeof(u64) * srcOffset + 0x800);
			Rendering::WriteChrMemory(pContext, sizeof(u64) * tileCount, CHR_SHEET_SIZE * sheetIndex + sizeof(u64) * dstOffset + 0x1000, (u8*)sheet + sizeof(u64) * srcOffset + 0x1000);
		}
	}
}