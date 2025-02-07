#include "chr_stream.h"
#include "memory_pool.h"
#include "system.h"
#include "rendering_util.h"

#ifdef EDITOR
#include "editor.h"
#endif

struct ChrBankData {
	ChrSheet bank;
	u32 refCounts[CHR_SIZE_TILES];
};

static ChrBankData banks[MAX_CHR_BANK_COUNT];
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

	ChrBankData bankData{};
	Rendering::Util::CreateChrSheet(imgData, &bankData.bank);
	free(imgData);

	u32 index = bankCount++;
	banks[index] = bankData;
#ifdef EDITOR
	Editor::SetupChrBankRendering(index, &bankData.bank);
#endif
}

u32 GetChrBankCount() {
	return bankCount;
}