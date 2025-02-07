#include "chr_stream.h"
#include "memory_pool.h"
#include "system.h"
#include "rendering_util.h"

#ifdef EDITOR
#include "editor.h"
#endif

struct ChrTileMetadata {
	u32 refCount = 0;
	s32 srcBankIndex = -1;
	u8 srcBankOffset = 0;
};

struct ChrSheetMetadata {
	ChrTileMetadata tiles[CHR_SIZE_TILES];
};

static ChrSheetMetadata metadata[CHR_COUNT];

static ChrSheet banks[MAX_CHR_BANK_COUNT];
static u32 bankCount;

#pragma pack(push, 1)
struct BitmapHeader {
	u16 fileType;
	u32 fileSize;
	u16 reserved1;
	u16 reserved2;
	u32 bitmapOffset;
	u32 size;
	s32 width;
	s32 height;
	u16 planes;
	u16 bpp;
};
#pragma pack(pop)

static char* LoadBitmapBytes(const char* fname, u32& outWidth, u32& outHeight, u16& outBpp) {
	u32 fileSize;
	char* bmpData = AllocFileBytes(fname, fileSize);
	if (fileSize == 0) {
		DEBUG_ERROR("Failed to read bitmap file!\n");
	}

	BitmapHeader* header = (BitmapHeader*)bmpData;
	//DEBUG_LOG("Type = %d, size = %d, offset = %d, width = %d, height = %d, bpp = %d\n", header->fileType, header->fileSize, header->bitmapOffset, header->width, header->height, header->bpp);

	u32 size = header->width * header->height * header->bpp;
	char* pixels = (char*)calloc(1, size);
	memcpy(pixels, (void*)(bmpData + header->bitmapOffset), size);
	outWidth = header->width;
	outHeight = header->height;
	outBpp = header->bpp;
	free(bmpData);
	return pixels;
}

void LoadChrBank(const char* fname) {
	u32 imgWidth, imgHeight;
	u16 bpp;
	char* imgData = LoadBitmapBytes(fname, imgWidth, imgHeight, bpp);

	if (imgWidth != CHR_DIM_PIXELS || imgHeight != CHR_DIM_PIXELS) {
		DEBUG_ERROR("Invalid chr image dimensions!\n");
	}

	if (bpp != 8) {
		DEBUG_ERROR("Invalid chr image format!\n");
	}

	u32 index = bankCount++;
	ChrSheet* pBank = &banks[index];
	Rendering::Util::CreateChrSheet(imgData, pBank);
	free(imgData);

#ifdef EDITOR
	Editor::SetupChrBankRendering(index, pBank);
#endif
}

u32 GetChrBankCount() {
	return bankCount;
}

void CopyBankToChrSheet(s32 bankIndex, s32 sheetIndex) {
	ChrSheet* src = &banks[bankIndex];
	ChrSheet* dst = Rendering::GetChrPtr(sheetIndex);
	Rendering::Util::CopyChrTiles(src->tiles, dst->tiles, CHR_SIZE_TILES);


}

bool GetSheetTile(s32 sheetIndex, s32 bankIndex, u8 bankOffset, u8* outTile) {
	for (u32 i = 0; i < CHR_SIZE_TILES; i++) {
		const ChrTileMetadata& tile = metadata[sheetIndex].tiles[i];

		if (tile.srcBankIndex == bankIndex && tile.srcBankOffset == bankOffset) {
			*outTile = i;
			return true;
		}
	}
	return false;
}

bool AddBankTileToChrSheet(s32 sheetIndex, s32 bankIndex, u8 bankOffset) {
	u8 existingIndex;
	if (GetSheetTile(sheetIndex, bankIndex, bankOffset, &existingIndex)) {
		metadata[sheetIndex].tiles[existingIndex].refCount++;
		return true;
	}

	// Find first free slot
	for (u32 i = 0; i < CHR_SIZE_TILES; i++) {
		ChrTileMetadata& tile = metadata[sheetIndex].tiles[i];

		if (tile.refCount == 0) {
			tile.srcBankIndex = bankIndex;
			tile.srcBankOffset = bankOffset;
			tile.refCount++;

			ChrSheet* src = &banks[bankIndex];
			ChrSheet* dst = Rendering::GetChrPtr(sheetIndex);
			Rendering::Util::CopyChrTiles(src->tiles + bankOffset, dst->tiles + i, 1);

			return true;
		}
	}
	return false;
}

bool RemoveBankTileFromChrSheet(s32 sheetIndex, s32 bankIndex, u8 bankOffset) {
	u8 existingIndex;
	if (GetSheetTile(sheetIndex, bankIndex, bankOffset, &existingIndex)) {
		metadata[sheetIndex].tiles[existingIndex].refCount--;
		return true;
	}

	return false;
}