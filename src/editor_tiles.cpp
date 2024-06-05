#include "editor_tiles.h"
#include "editor_util.h"
#include "collision.h"
#include "tileset.h"
#include <stdio.h>

namespace Editor {
	namespace Tiles {
		constexpr r32 tilePreviewSize = 64;
		constexpr r32 pixelSize = tilePreviewSize / TILE_SIZE;

		constexpr u32 metatileTypeCount = 2;
		constexpr const char* metatileTypeNames[metatileTypeCount] = { "Empty", "Solid" };

		constexpr s32 renderScale = 2;
		constexpr s32 gridSizeTiles = 16;
		constexpr s32 gridStepPixels = (TILE_SIZE * 2) * renderScale;
		constexpr s32 gridSizePixels = gridSizeTiles * gridStepPixels;

		u32 selectedMetatileIndex = 0;
		u32 selectedTileIndex = 0;

		void DrawMetatileEditor(EditorContext* pContext, Rendering::RenderContext* pRenderContext) {
			ImGui::Begin("Metatile Editor");

			ImGui::Text("0x%02x", selectedMetatileIndex);

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 tilePos = Util::DrawTileGrid(pContext, tilePreviewSize, 2, &selectedTileIndex);
			Util::DrawMetatile(pContext, selectedMetatileIndex, tilePos, tilePreviewSize / 2);
			Util::DrawTileGridSelection(pContext, tilePos, tilePreviewSize, 2, selectedTileIndex);

			ImGui::SameLine();
			if (ImGui::Button("CHR")) {
				Tileset::Metatile& metatile = Tileset::GetMetatile(selectedMetatileIndex);
				metatile.tiles[selectedTileIndex] = pContext->chrSelection[0];
			}

			Tileset::TileType& type = Tileset::GetTileType(selectedMetatileIndex);
			s32 typeInt = (s32)type;
			if (ImGui::SliderInt("Type", &typeInt, 0, metatileTypeCount - 1, metatileTypeNames[typeInt])) {
				type = (Tileset::TileType)typeInt;
			}

			s32 palette = Tileset::GetPalette(selectedMetatileIndex);
			ImGui::SliderInt("Palette", &palette, 0, 3);
			u8& attribute = Tileset::GetAttribute(selectedMetatileIndex);
			u8 attribSubIndex = selectedMetatileIndex % 4;
			attribute &= ~(0b11 << attribSubIndex * 2);
			attribute |= (palette & 0b11) << attribSubIndex * 2;

			ImGui::End();
		}

		void DrawTilesetEditor(EditorContext* pContext, Rendering::RenderContext* pRenderContext) {
			ImGui::Begin("Tileset");

			if (ImGui::Button("Save")) {
				Tileset::SaveTileset("assets/forest.til");
			}
			ImGui::SameLine();
			if (ImGui::Button("Revert changes")) {
				Tileset::LoadTileset("assets/forest.til");
			}

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			u32 newSelection = selectedMetatileIndex;
			ImVec2 chrPos = Util::DrawTileGrid(pContext, gridSizePixels, gridSizeTiles, &newSelection);

			// Rewrite level editor clipboard if new selection was made
			if (newSelection != selectedMetatileIndex) {
				selectedMetatileIndex = newSelection;
				pContext->levelClipboard[0] = selectedMetatileIndex;
				pContext->levelSelectionOffset = ImVec2(0, 0);
				pContext->levelSelectionSize = ImVec2(1, 1);
			}

			for (s32 i = 0; i < 256; i++) {
				ImVec2 metatileCoord = ImVec2(i % 16, i / 16);
				ImVec2 metatileOffset = ImVec2(chrPos.x + metatileCoord.x * gridStepPixels, chrPos.y + metatileCoord.y * gridStepPixels);

				Util::DrawMetatile(pContext, i, metatileOffset, gridStepPixels / 2);
			}
			Util::DrawTileGridSelection(pContext, chrPos, gridSizePixels, gridSizeTiles, selectedMetatileIndex);

			ImGui::End();
		}

	}
}