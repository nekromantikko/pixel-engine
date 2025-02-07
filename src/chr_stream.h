#pragma once
#include "rendering.h"

void LoadChrBank(const char* fname);

u32 GetChrBankCount();
void CopyBankToChrSheet(s32 bankIndex, s32 sheetIndex);
bool GetSheetTile(s32 sheetIndex, s32 bankIndex, u8 bankOffset, u8* outTile);
bool AddBankTileToChrSheet(s32 sheetIndex, s32 bankIndex, u8 bankOffset);
bool RemoveBankTileFromChrSheet(s32 sheetIndex, s32 bankIndex, u8 bankOffset);