#pragma once
#include "typedef.h"
#include "rendering.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>

constexpr u32 PLAYER_BANK_INDEX = 2;

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

		glm::vec2 GetViewportPos();
		glm::vec2 SetViewportPos(const glm::vec2& pos, bool loadTiles = true);

		void RefreshViewport();

		bool PositionInViewportBounds(const glm::vec2& pos);
		glm::i16vec2 WorldPosToScreenPixels(const glm::vec2& pos);

		void ClearSpriteLayers(bool fullClear = false);
		Sprite* GetNextFreeSprite(u8 layerIndex, u32 count = 1);

		bool DrawSprite(u8 layerIndex, const Sprite& sprite);
		bool DrawMetaspriteSprite(u8 layerIndex, u32 metaspriteIndex, u32 spriteIndex, glm::i16vec2 pos, bool hFlip = false, bool vFlip = false, s32 paletteOverride = -1);
		bool DrawMetasprite(u8 layerIndex, u32 metaspriteIndex, glm::i16vec2 pos, bool hFlip = false, bool vFlip = false, s32 paletteOverride = -1);
		void CopyBankTiles(u32 bankIndex, u32 bankOffset, u32 sheetIndex, u32 sheetOffset, u32 count);

		void GetPalettePresetColors(u8 presetIndex, u8* pOutColors);
		void WritePaletteColors(u8 paletteIndex, u8* pColors);
	}
}