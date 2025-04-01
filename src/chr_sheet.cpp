#include "chr_sheet.h"
#include "debug.h"
#include <cstdlib>

#pragma pack(push, 1)
struct BitmapInfo {
	u32 infoSize;
	s32 width;
	s32 height;
	u16 planes;
	u16 bpp;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BitmapHeader {
	char signature[2];
	u32 size;
	u32 reserved;
	u32 dataOffset;
	BitmapInfo info;
};
#pragma pack(pop)

static void CreateFromBmp(const char* pixels, ChrSheet* pOutSheet) {
	for (u32 y = 0; y < CHR_DIM_PIXELS; y++) {
		for (u32 x = 0; x < CHR_DIM_PIXELS; x++) {
			u32 coarseX = x / 8;
			u32 coarseY = y / 8;
			u32 fineX = x % 8;
			u32 fineY = y % 8;
			u32 tileIndex = (15 - coarseY) * 16 + coarseX; // Tile 0 is top left instead of bottom left
			u32 inPixelIndex = y * CHR_DIM_PIXELS + x;
			u32 outPixelIndex = (7 - fineY) * 8 + fineX; // Also pixels go from top to bottom in this program, but bottom to top in bmp, so flip

			u8 pixel = pixels[inPixelIndex];

			ChrTile& tile = pOutSheet->tiles[tileIndex];
			tile.p0 = (tile.p0 & ~(1ULL << outPixelIndex)) | ((u64)(pixel & 0b00000001) << outPixelIndex);
			tile.p1 = (tile.p1 & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000010) >> 1) << outPixelIndex);
			tile.p2 = (tile.p2 & ~(1ULL << outPixelIndex)) | ((u64)((pixel & 0b00000100) >> 2) << outPixelIndex);
		}
	}
}

bool Assets::LoadChrSheetFromFile(const std::filesystem::path& path, void* pOutData) {
	if (!std::filesystem::exists(path)) {
		DEBUG_ERROR("File (%s) does not exist\n", path.string().c_str());
		return false;
	}

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		DEBUG_ERROR("Failed to open file\n");
		return false;
	}

	BitmapHeader header{};
	fread(&header, sizeof(BitmapHeader), 1, pFile);
	if (memcmp(header.signature, "BM", 2) != 0) {
		DEBUG_ERROR("Invalid bitmap file\n");
		fclose(pFile);
		return false;
	}

	if (header.info.bpp != 8) {
		DEBUG_ERROR("Bitmap bpp %d is invalid (Should be 8)\n", header.info.bpp);
		fclose(pFile);
		return false;
	}

	if (header.info.width != CHR_DIM_PIXELS || header.info.height != CHR_DIM_PIXELS) {
		DEBUG_ERROR("Bitmap size (%d x %d) is invalid (Should be %d x %d)\n", header.info.width, header.info.height, CHR_DIM_PIXELS, CHR_DIM_PIXELS);
		fclose(pFile);
		return false;
	}

	fseek(pFile, header.dataOffset, SEEK_SET);
	char pixels[CHR_DIM_PIXELS * CHR_DIM_PIXELS];
	fread(pixels, 1, CHR_DIM_PIXELS * CHR_DIM_PIXELS, pFile);

	fclose(pFile);

	CreateFromBmp(pixels, (ChrSheet*)pOutData);

	return true;
}