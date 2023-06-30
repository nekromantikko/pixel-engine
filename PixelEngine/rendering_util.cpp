#include "rendering_util.h"
#include "system.h"

namespace Rendering
{
	namespace Util
	{
		void CreateChrSheet(const char* fname, u8* outBytes) {
			u32 imgWidth, imgHeight;
			u16 bpp;
			char* imgData = LoadBitmapBytes(fname, imgWidth, imgHeight, bpp);

			if (imgWidth != 128 || imgHeight != 128) {
				ERROR("Invalid chr image dimensions!\n");
			}

			if (bpp != 8) {
				ERROR("Invalid chr image format!\n");
			}

			Tile* tileData = (Tile*)outBytes;
			for (u32 y = 0; y < imgHeight; y++) {
				for (u32 x = 0; x < imgWidth; x++) {
					u32 coarseX = x / 8;
					u32 coarseY = y / 8;
					u32 fineX = x % 8;
					u32 fineY = y % 8;
					u32 tileIndex = (15 - coarseY) * 16 + coarseX; // Tile 0 is top left instead of bottom left
					u32 inPixelIndex = y * imgWidth + x;
					u32 outPixelIndex = (7 - fineY) * 8 + fineX; // Also pixels go from top to bottom in this program, but bottom to top in bmp, so flip
					tileData[tileIndex].pixels[outPixelIndex] = imgData[inPixelIndex];
				}
			}

			free(imgData);
		}
	}
}