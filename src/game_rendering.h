#pragma once
#include "core_types.h"

enum SpriteLayerType : u8 {
	SPRITE_LAYER_UI,
	SPRITE_LAYER_FX,
	SPRITE_LAYER_FG,
	SPRITE_LAYER_BG,

	SPRITE_LAYER_COUNT
};

struct SpriteLayer {
	Sprite* pNextSprite = nullptr;
	u32 spriteCount = 0;
};

namespace Game {
	namespace Rendering {
		void Init();
		void Free();

		glm::vec2 GetViewportPos();
		glm::vec2 SetViewportPos(const glm::vec2& pos, bool loadTiles = true);

		void RefreshViewport();

		bool PositionInViewportBounds(const glm::vec2& pos);
		glm::i16vec2 WorldPosToScreenPixels(const glm::vec2& pos);

		void ClearSpriteLayers(bool fullClear = false);
		Sprite* GetNextFreeSprite(u8 layerIndex, u32 count = 1);

		bool DrawSprite(u8 layerIndex, const Sprite& sprite);
		bool DrawMetaspriteSprite(u8 layerIndex, MetaspriteHandle metaspriteId, u32 spriteIndex, glm::i16vec2 pos, bool hFlip = false, bool vFlip = false, s32 paletteOverride = -1);
		bool DrawMetasprite(u8 layerIndex, MetaspriteHandle metaspriteId, glm::i16vec2 pos, bool hFlip = false, bool vFlip = false, s32 paletteOverride = -1);
		void CopyBankTiles(ChrBankHandle bankId, u32 bankOffset, u32 sheetIndex, u32 sheetOffset, u32 count);

		bool GetPalettePresetColors(PaletteHandle paletteId, u8* pOutColors);
		void WritePaletteColors(u8 paletteIndex, u8* pColors);
		void CopyPaletteColors(PaletteHandle paletteId, u8 paletteIndex);
	}
}