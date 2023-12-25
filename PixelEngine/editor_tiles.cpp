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

		void DrawMetatile(EditorContext* pContext, u32 index, s32 palette, ImVec2 pos, r32 tileSize) {
			Tileset::Metatile& metatile = Tileset::GetMetatile(index);

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			for (s32 t = 0; t < 4; t++) {
				ImVec2 tileCoord = ImVec2(t % 2, t / 2);
				ImVec2 tileOffset = ImVec2(pos.x + tileCoord.x * tileSize, pos.y + tileCoord.y * tileSize);
				u8 tileIndex = metatile.tiles[t];
				ImVec2 tileChrCoord = Util::GetTileCoord(tileIndex);
				ImVec2 tileStart = Util::TileCoordToTexCoord(tileChrCoord, 0);
				ImVec2 tileEnd = Util::TileCoordToTexCoord(ImVec2(tileChrCoord.x + 1, tileChrCoord.y + 1), 0);
				drawList->AddImage(pContext->chrTexture[palette], tileOffset, ImVec2(tileOffset.x + tileSize, tileOffset.y + tileSize), tileStart, tileEnd);
			}
		}

		void DrawMetatileEditor(EditorContext* pContext, Rendering::RenderContext* pRenderContext) {
			ImGui::Begin("Metatile Editor");

			ImGui::Text("0x%02x", selectedMetatileIndex);

			u8& attribute = Tileset::GetAttribute(selectedMetatileIndex);
			u8 attribSubIndex = selectedMetatileIndex % 4;
			s32 palette = (attribute >> attribSubIndex * 2) & 0b11;

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 tilePos = Util::DrawTileGrid(pContext, tilePreviewSize, 2, &selectedTileIndex);
			DrawMetatile(pContext, selectedMetatileIndex, palette, tilePos, tilePreviewSize / 2);
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

			ImGui::SliderInt("Palette", &palette, 0, 3);
			attribute &= ~(0b11 << attribSubIndex * 2);
			attribute |= (palette & 0b11) << attribSubIndex * 2;

			ImGui::End();
		}

		void DrawTilesetEditor(EditorContext* pContext, Rendering::RenderContext* pRenderContext) {
			ImGui::Begin("Tileset");

			if (ImGui::Button("Save")) {
				Tileset::SaveTileset("forest.til");
			}
			ImGui::SameLine();
			if (ImGui::Button("Revert changes")) {
				Tileset::LoadTileset("forest.til");
			}

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 chrPos = Util::DrawTileGrid(pContext, gridSizePixels, gridSizeTiles, &selectedMetatileIndex);
			for (s32 i = 0; i < 256; i++) {
				ImVec2 metatileCoord = ImVec2(i % 16, i / 16);
				ImVec2 metatileOffset = ImVec2(chrPos.x + metatileCoord.x * gridStepPixels, chrPos.y + metatileCoord.y * gridStepPixels);

				u8& attribute = Tileset::GetAttribute(i);
				u8 attribSubIndex = i % 4;
				s32 palette = (attribute >> attribSubIndex * 2) & 0b11;

				DrawMetatile(pContext, i, palette, metatileOffset, gridStepPixels / 2);
			}
			Util::DrawTileGridSelection(pContext, chrPos, gridSizePixels, gridSizeTiles, selectedMetatileIndex);

			ImGui::End();
		}

	}
}