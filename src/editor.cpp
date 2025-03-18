#include "editor.h"
#include "editor_actor.h"
#include "system.h"
#include <cassert>
#include <limits>
#include <type_traits>
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>

#include "tiles.h"
#include "rendering_util.h"
#include "metasprite.h"
#include "game.h"
#include "room.h"
#include "game_rendering.h"
#include "game_state.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "audio.h"
#include "random.h"
#include "dungeon.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/matrix_transform_2d.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <span>
#include <execution>
#include <stack>
#include <utility>

constexpr u32 CLIPBOARD_DIM_TILES = (VIEWPORT_WIDTH_TILES / 2) + 1;

struct TilemapClipboard {
	u8 clipboard[CLIPBOARD_DIM_TILES * CLIPBOARD_DIM_TILES];
	ImVec2 size = { 1, 1 };
	ImVec2 offset = { 0, 0 };
};

struct RoomToolsState {
	bool propertiesOpen = true;
	bool tilemapOpen = true;
	bool actorsOpen = true;
};

enum RoomEditMode {
	EDIT_MODE_NONE = 0,
	EDIT_MODE_TILES = 1,
	EDIT_MODE_ACTORS = 2
};

struct EditorContext {
	ImTextureID chrTexture;
	ImTextureID paletteTexture;
	ImTextureID gameViewTexture;

	// Timing
	r64 secondsElapsed;

	// Console
	std::vector<char*> consoleLog;

	// Editor state
	bool demoWindowOpen = false;
	bool debugWindowOpen = true;
	bool spriteWindowOpen = false;
	bool tilesetWindowOpen = false;
	bool roomWindowOpen = false;
	bool actorWindowOpen = false;
	bool audioWindowOpen = false;
	bool dungeonWindowOpen = false;

	// Debug overlay
	bool showDebugOverlay = true;
	bool drawActorHitboxes = false;
	bool drawActorPositions = false;
};

static EditorContext* pContext;

#pragma region Utils
static inline glm::vec4 NormalizedToChrTexCoord(const glm::vec4& normalized, u8 chrIndex, u8 palette) {
	constexpr r32 INV_CHR_COUNT = 1.0f / CHR_COUNT;
	constexpr r32 INV_SHEET_PALETTE_COUNT = (1.0f / PALETTE_COUNT) * CHR_COUNT;
	
	// Apply palette and sheet index offsets to normalized coordinates
	return glm::vec4(
		(normalized.x + palette) * INV_SHEET_PALETTE_COUNT,
		(normalized.y + chrIndex) * INV_CHR_COUNT,
		(normalized.z + palette) * INV_SHEET_PALETTE_COUNT,
		(normalized.w + chrIndex) * INV_CHR_COUNT
	);
}

static glm::vec4 ChrTileToTexCoord(u8 tileIndex, u8 chrIndex, u8 palette) {
	constexpr r32 INV_CHR_DIM_TILES = 1.0f / CHR_DIM_TILES;
	constexpr u32 CHR_DIM_TILES_BITS = 0xf;
	constexpr u32 CHR_DIM_TILES_LOG2 = 4;

	// Compute base tile coordinates (bitwise optimization)
	const u8 tileX = tileIndex & CHR_DIM_TILES_BITS;
	const u8 tileY = tileIndex >> CHR_DIM_TILES_LOG2;

	// Convert to normalized texture coordinates
	const glm::vec4 normalized(
		tileX * INV_CHR_DIM_TILES,
		tileY * INV_CHR_DIM_TILES,
		(tileX + 1) * INV_CHR_DIM_TILES,
		(tileY + 1) * INV_CHR_DIM_TILES
	);

	return NormalizedToChrTexCoord(normalized, chrIndex, palette);
}

static ImVec2 DrawTileGrid(ImVec2 size, r32 gridStep, s32* selection = nullptr, bool* focused = nullptr) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 topLeft = ImGui::GetCursorScreenPos();
	const ImVec2 btmRight = ImVec2(topLeft.x + size.x, topLeft.y + size.y);

	// Invisible button to prevent dragging window
	ImGui::InvisibleButton("##canvas", size);

	if (focused != nullptr) {
		*focused = ImGui::IsItemActive();
	}

	constexpr r32 invBgColorIndex = 1.0f / (PALETTE_COUNT * PALETTE_COLOR_COUNT);
	drawList->AddImage(pContext->paletteTexture, topLeft, btmRight, ImVec2(0, 0), ImVec2(invBgColorIndex, 1.0f));
	for (r32 x = 0; x < size.x; x += gridStep)
		drawList->AddLine(ImVec2(topLeft.x + x, topLeft.y), ImVec2(topLeft.x + x, btmRight.y), IM_COL32(200, 200, 200, 40));
	for (r32 y = 0; y < size.y; y += gridStep)
		drawList->AddLine(ImVec2(topLeft.x, topLeft.y + y), ImVec2(btmRight.x, topLeft.y + y), IM_COL32(200, 200, 200, 40));

	if (selection != nullptr && ImGui::IsItemActive()) {
		// Handle selection
		ImGuiIO& io = ImGui::GetIO();
		const bool gridClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x >= topLeft.x && io.MousePos.x < topLeft.x + size.x && io.MousePos.y >= topLeft.y && io.MousePos.y < topLeft.y + size.y;
		if (gridClicked) {
			const ImVec2 mousePosRelative = ImVec2(io.MousePos.x - topLeft.x, io.MousePos.y - topLeft.y);
			const ImVec2 clickedTileCoord = ImVec2(glm::floor(mousePosRelative.x / gridStep), glm::floor(mousePosRelative.y / gridStep));
			const s32 xDivisions = size.x / gridStep;
			const s32 clickedTileIndex = clickedTileCoord.y * xDivisions + clickedTileCoord.x;
			*selection = clickedTileIndex;
		}
	}

	return topLeft;
}

static void DrawTileGridSelection(ImVec2 gridPos, ImVec2 gridSize, r32 gridStep, u32 selection) {
	const s32 xDivisions = gridSize.x / gridStep;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 selectedTilePos = ImVec2(gridPos.x + gridStep * (selection % xDivisions), gridPos.y + gridStep * (selection / xDivisions));
	drawList->AddRect(selectedTilePos, ImVec2(selectedTilePos.x + gridStep, selectedTilePos.y + gridStep), IM_COL32(255, 255, 255, 255));
}

// See GetMetatileVertices for human-readable version
static void GetMetatileVerticesAVX(const Metatile& metatile, const ImVec2& pos, r32 scale, ImVec2* outVertices, ImVec2* outUV) {
	constexpr r32 TILE_SIZE = 1.0f / METATILE_DIM_TILES;
	constexpr r32 INV_CHR_COUNT = 1.0f / CHR_COUNT;
	constexpr r32 INV_CHR_DIM_TILES = 1.0f / CHR_DIM_TILES;
	constexpr u32 CHR_DIM_TILES_BITS = 0xf;
	constexpr u32 CHR_DIM_TILES_LOG2 = 4;
	constexpr r32 INV_SHEET_PALETTE_COUNT = (1.0f / PALETTE_COUNT) * CHR_COUNT;

	const __m256 xMask = _mm256_setr_ps(0, 1, 0, 1, 1, 0, 1, 0);
	const __m256 yMask = _mm256_setr_ps(0, 0, 0, 0, 1, 1, 1, 1);

	const s32 i0 = metatile.tiles[0].tileId;
	const s32 i1 = metatile.tiles[1].tileId;
	const s32 i2 = metatile.tiles[2].tileId;
	const s32 i3 = metatile.tiles[3].tileId;

	const u32 p0 = metatile.tiles[0].palette;
	const u32 p1 = metatile.tiles[1].palette;
	const u32 p2 = metatile.tiles[2].palette;
	const u32 p3 = metatile.tiles[3].palette;

	const bool hf0 = metatile.tiles[0].flipHorizontal;
	const bool hf1 = metatile.tiles[1].flipHorizontal;
	const bool hf2 = metatile.tiles[2].flipHorizontal;
	const bool hf3 = metatile.tiles[3].flipHorizontal;

	const bool vf0 = metatile.tiles[0].flipVertical;
	const bool vf1 = metatile.tiles[1].flipVertical;
	const bool vf2 = metatile.tiles[2].flipVertical;
	const bool vf3 = metatile.tiles[3].flipVertical;

	const __m256i ti0 = _mm256_setr_epi32(i0, i0, i1, i1, i0, i0, i1, i1);
	const __m256i ti1 = _mm256_setr_epi32(i2, i2, i3, i3, i2, i2, i3, i3);
	const __m256 ci = _mm256_set1_ps(INV_CHR_COUNT);
	const __m256 cit = _mm256_set1_ps(INV_CHR_DIM_TILES);
	const __m256i cb = _mm256_set1_epi32(CHR_DIM_TILES_BITS);

	const __m256 p0v = _mm256_setr_ps(p0, p0, p1, p1, p0, p0, p1, p1);
	const __m256 p1v = _mm256_setr_ps(p2, p2, p3, p3, p2, p2, p3, p3);

	const __m256 h0v = _mm256_setr_ps(hf0, hf0, hf1, hf1, hf0, hf0, hf1, hf1);
	const __m256 h1v = _mm256_setr_ps(hf2, hf2, hf3, hf3, hf2, hf2, hf3, hf3);

	const __m256 v0v = _mm256_setr_ps(vf0, vf0, vf1, vf1, vf0, vf0, vf1, vf1);
	const __m256 v1v = _mm256_setr_ps(vf2, vf2, vf3, vf3, vf2, vf2, vf3, vf3);

	const __m256 pi = _mm256_set1_ps(INV_SHEET_PALETTE_COUNT);

	const __m256 s = _mm256_set1_ps(scale * TILE_SIZE);
	const __m256 tx = _mm256_set1_ps(pos.x);
	const __m256 ty = _mm256_set1_ps(pos.y);

	__m256 x = _mm256_setr_ps(0, 1, 1, 2, 1, 0, 2, 1);
	x = _mm256_mul_ps(x, s);
	x = _mm256_add_ps(x, tx);

	__m256 y0 = _mm256_setr_ps(0, 0, 0, 0, 1, 1, 1, 1);
	y0 = _mm256_mul_ps(y0, s);
	y0 = _mm256_add_ps(y0, ty);

	const __m256i tix0 = _mm256_and_si256(ti0, cb);
	const __m256i tiy0 = _mm256_srli_epi32(ti0, CHR_DIM_TILES_LOG2);
	const __m256 fhMask0 = _mm256_xor_ps(xMask, h0v);
	__m256 u0 = _mm256_cvtepi32_ps(tix0);
	u0 = _mm256_add_ps(u0, fhMask0);
	u0 = _mm256_mul_ps(u0, cit);

	u0 = _mm256_add_ps(u0, p0v);
	u0 = _mm256_mul_ps(u0, pi);

	const __m256 fvMask0 = _mm256_xor_ps(yMask, v0v);
	__m256 v0 = _mm256_cvtepi32_ps(tiy0);
	v0 = _mm256_add_ps(v0, fvMask0);
	v0 = _mm256_mul_ps(v0, cit);

	v0 = _mm256_mul_ps(v0, ci);

	__m256 y1 = _mm256_setr_ps(1, 1, 1, 1, 2, 2, 2, 2);
	y1 = _mm256_mul_ps(y1, s);
	y1 = _mm256_add_ps(y1, ty);

	const __m256i tix1 = _mm256_and_si256(ti1, cb);
	const __m256i tiy1 = _mm256_srli_epi32(ti1, CHR_DIM_TILES_LOG2);
	const __m256 fhMask1 = _mm256_xor_ps(xMask, h1v);
	__m256 u1 = _mm256_cvtepi32_ps(tix1);
	u1 = _mm256_add_ps(u1, fhMask1);
	u1 = _mm256_mul_ps(u1, cit);

	u1 = _mm256_add_ps(u1, p1v);
	u1 = _mm256_mul_ps(u1, pi);

	const __m256 fvMask1 = _mm256_xor_ps(yMask, v1v);
	__m256 v1 = _mm256_cvtepi32_ps(tiy1);
	v1 = _mm256_add_ps(v1, fvMask1);
	v1 = _mm256_mul_ps(v1, cit);

	v1 = _mm256_mul_ps(v1, ci);

	_mm256_store_ps((r32*)outVertices, _mm256_unpacklo_ps(x, y0));
	_mm256_store_ps((r32*)outVertices + 8, _mm256_unpackhi_ps(x, y0));
	_mm256_store_ps((r32*)outVertices + 16, _mm256_unpacklo_ps(x, y1));
	_mm256_store_ps((r32*)outVertices + 24, _mm256_unpackhi_ps(x, y1));

	_mm256_store_ps((r32*)outUV, _mm256_unpacklo_ps(u0, v0));
	_mm256_store_ps((r32*)outUV + 8, _mm256_unpackhi_ps(u0, v0));
	_mm256_store_ps((r32*)outUV + 16, _mm256_unpacklo_ps(u1, v1));
	_mm256_store_ps((r32*)outUV + 24, _mm256_unpackhi_ps(u1, v1));
}

static void GetMetatileVertices(const Metatile& metatile, const ImVec2& pos, r32 scale, ImVec2* outVertices, ImVec2* outUV) {
	constexpr r32 tileSize = 1.0f / METATILE_DIM_TILES;

	for (u32 i = 0; i < METATILE_TILE_COUNT; i++) {
		const ImVec2 pMin = ImVec2((i & 1) * tileSize, (i >> 1) * tileSize);
		const ImVec2 pMax = ImVec2(pMin.x + tileSize, pMin.y + tileSize);

		ImVec2 p0 = pMin;
		ImVec2 p1(pMax.x, pMin.y);
		ImVec2 p2 = pMax;
		ImVec2 p3(pMin.x, pMax.y);

		// Transform vertices
		p0 = ImVec2(p0.x * scale + pos.x, p0.y * scale + pos.y);
		p1 = ImVec2(p1.x * scale + pos.x, p1.y * scale + pos.y);
		p2 = ImVec2(p2.x * scale + pos.x, p2.y * scale + pos.y);
		p3 = ImVec2(p3.x * scale + pos.x, p3.y * scale + pos.y);

		outVertices[i * 4] = p0;
		outVertices[i * 4 + 1] = p1;
		outVertices[i * 4 + 2] = p2;
		outVertices[i * 4 + 3] = p3;

		const glm::vec4 uvMinMax = ChrTileToTexCoord(metatile.tiles[i].tileId, 0, metatile.tiles[i].palette);

		outUV[i * 4] = ImVec2(uvMinMax.x, uvMinMax.y);
		outUV[i * 4 + 1] = ImVec2(uvMinMax.z, uvMinMax.y);
		outUV[i * 4 + 2] = ImVec2(uvMinMax.z, uvMinMax.w);
		outUV[i * 4 + 3] = ImVec2(uvMinMax.x, uvMinMax.w);
	}
}

static void WriteMetatile(const ImVec2* verts, const ImVec2* uv, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	for (u32 i = 0; i < METATILE_TILE_COUNT; i++) {
		drawList->PrimWriteIdx(drawList->_VtxCurrentIdx);
		drawList->PrimWriteIdx(drawList->_VtxCurrentIdx + 1);
		drawList->PrimWriteIdx(drawList->_VtxCurrentIdx + 2);
		drawList->PrimWriteIdx(drawList->_VtxCurrentIdx);
		drawList->PrimWriteIdx(drawList->_VtxCurrentIdx + 2);
		drawList->PrimWriteIdx(drawList->_VtxCurrentIdx + 3);

		drawList->PrimWriteVtx(verts[i * 4], uv[i * 4], color);
		drawList->PrimWriteVtx(verts[i * 4 + 1], uv[i * 4 + 1], color);
		drawList->PrimWriteVtx(verts[i * 4 + 2], uv[i * 4 + 2], color);
		drawList->PrimWriteVtx(verts[i * 4 + 3], uv[i * 4 + 3], color);
	}
}

static void DrawMetatile(const Metatile& metatile, ImVec2 pos, r32 size, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	const r32 tileSize = size / METATILE_DIM_TILES;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushTextureID(pContext->chrTexture);
	drawList->PrimReserve(METATILE_TILE_COUNT * 6, METATILE_TILE_COUNT * 4);

	ImVec2 verts[METATILE_TILE_COUNT * 4];
	ImVec2 uv[METATILE_TILE_COUNT * 4];

	GetMetatileVerticesAVX(metatile, pos, size, verts, uv);
	WriteMetatile(verts, uv, color);

	drawList->PopTextureID();
}

static void DrawNametable(ImVec2 size, const Nametable& nametable) {
	const r32 gridStep = size.x / NAMETABLE_DIM_TILES;
	const ImVec2 tablePos = DrawTileGrid(size, gridStep);

	const r32 scale = size.x / NAMETABLE_DIM_PIXELS;
	const r32 metatileDrawSize = METATILE_DIM_PIXELS * scale;

	for (u32 i = 0; i < NAMETABLE_SIZE_METATILES; i++) {
		u32 x = i % NAMETABLE_DIM_METATILES;
		u32 y = i / NAMETABLE_DIM_METATILES;

		ImVec2 pos = ImVec2(tablePos.x + (x * metatileDrawSize), tablePos.y + (y * metatileDrawSize));

		const Metatile metatile = Rendering::Util::GetNametableMetatile(&nametable, i);
		DrawMetatile(metatile, pos, metatileDrawSize);
	}
}

static bool DrawPaletteButton(u8 palette) {
	constexpr r32 invPaletteCount = 1.0f / PALETTE_COUNT;
	return ImGui::ImageButton("", pContext->paletteTexture, ImVec2(80, 10), ImVec2(invPaletteCount * palette, 0), ImVec2(invPaletteCount * (palette + 1), 1));
}

static void DrawCHRSheet(r32 size, u32 index, u8 palette, s32* selectedTile) {
	constexpr s32 gridSizeTiles = CHR_DIM_TILES;

	const r32 renderScale = size / (gridSizeTiles * TILE_DIM_PIXELS);
	const r32 gridStepPixels = TILE_DIM_PIXELS * renderScale;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 gridSize = ImVec2(size, size);
	const ImVec2 chrPos = DrawTileGrid(gridSize, gridStepPixels, selectedTile);
	const glm::vec4 uvMinMax = NormalizedToChrTexCoord({ 0,0,1,1, }, index, palette);

	drawList->AddImage(pContext->chrTexture, chrPos, ImVec2(chrPos.x + size, chrPos.y + size), ImVec2(uvMinMax.x, uvMinMax.y), ImVec2(uvMinMax.z, uvMinMax.w));
	if (selectedTile != nullptr && *selectedTile >= 0) {
		DrawTileGridSelection(chrPos, gridSize, gridStepPixels, *selectedTile);
	}
}

static void DrawTilemap(const Tilemap* pTilemap, const ImVec2& metatileOffset, const ImVec2& metatileSize, const ImVec2& pos, r32 scale) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Draw background color
	constexpr r32 invBgColorIndex = 1.0f / (PALETTE_COUNT * PALETTE_COLOR_COUNT);
	drawList->AddImage(pContext->paletteTexture, pos, ImVec2(pos.x + metatileSize.x * scale, pos.y + metatileSize.y * scale), ImVec2(0, 0), ImVec2(invBgColorIndex, 1.0f));

	const Tileset* pTileset = Tiles::GetTileset();
	const u32 tileCount = metatileSize.x * metatileSize.y * METATILE_TILE_COUNT;

	drawList->PushTextureID(pContext->chrTexture);
	drawList->PrimReserve(tileCount * 6, tileCount * 4); // NOTE: Seems as if primitives for max. 4096 tiles can be reserved...

	ImVec2 verts[METATILE_TILE_COUNT * 4];
	ImVec2 uv[METATILE_TILE_COUNT * 4];

	const u32 xOffsetInt = u32(metatileOffset.x);
	const u32 yOffsetInt = u32(metatileOffset.y);
	const r32 xOffsetRemainder = metatileOffset.x - xOffsetInt;
	const r32 yOffsetRemainder = metatileOffset.y - yOffsetInt;

	for (u32 y = 0; y < metatileSize.y; y++) {
		for (u32 x = 0; x < metatileSize.x; x++) {
			const u32 i = (x + xOffsetInt) + (y + yOffsetInt) * pTilemap->width;
			const u8 tilesetTileIndex = pTilemap->tiles[i];
			const TilesetTile& tilesetTile = pTileset->tiles[tilesetTileIndex];
			const Metatile& metatile = tilesetTile.metatile;

			const ImVec2 drawPos = ImVec2(pos.x + (x - xOffsetRemainder) * scale, pos.y + (y - yOffsetRemainder) * scale);
			GetMetatileVerticesAVX(metatile, drawPos, scale, verts, uv);
			WriteMetatile(verts, uv);
		}
	}

	drawList->PopTextureID();
}

static void DrawTileset(const Tileset* pTileset, r32 size, s32* selectedMetatile) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const r32 renderScale = size / (TILESET_DIM * METATILE_DIM_PIXELS);
	const r32 gridStepPixels = METATILE_DIM_PIXELS * renderScale;

	const ImVec2 gridSize = ImVec2(size, size);
	const ImVec2 chrPos = DrawTileGrid(gridSize, gridStepPixels, selectedMetatile);

	const u32 tilesetTileDim = TILESET_DIM * METATILE_DIM_TILES;
	const u32 tilesetTileCount = tilesetTileDim * tilesetTileDim;

	drawList->PushTextureID(pContext->chrTexture);
	drawList->PrimReserve(tilesetTileCount * 6, tilesetTileCount * 4);

	ImVec2 verts[METATILE_TILE_COUNT * 4];
	ImVec2 uv[METATILE_TILE_COUNT * 4];

	for (s32 i = 0; i < TILESET_SIZE; i++) {
		ImVec2 metatileCoord = ImVec2(i % TILESET_DIM, i / TILESET_DIM);
		ImVec2 metatileOffset = ImVec2(chrPos.x + metatileCoord.x * gridStepPixels, chrPos.y + metatileCoord.y * gridStepPixels);

		const Metatile& metatile = pTileset->tiles[i].metatile;
		GetMetatileVerticesAVX(metatile, metatileOffset, renderScale * TILESET_DIM, verts, uv);
		WriteMetatile(verts, uv);
	}
	drawList->PopTextureID();

	if (selectedMetatile != nullptr && *selectedMetatile >= 0) {
		DrawTileGridSelection(chrPos, gridSize, gridStepPixels, *selectedMetatile);
	}
}

static void DrawSprite(const Sprite& sprite, const ImVec2& pos, r32 renderScale, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const r32 tileDrawSize = TILE_DIM_PIXELS * renderScale;

	const u8 index = (u8)sprite.tileId;
	const bool flipX = sprite.flipHorizontal;
	const bool flipY = sprite.flipVertical;
	const u8 palette = sprite.palette;

	glm::vec4 uvMinMax = ChrTileToTexCoord(index, 1, palette);

	drawList->AddImage(pContext->chrTexture, pos, ImVec2(pos.x + tileDrawSize, pos.y + tileDrawSize), ImVec2(flipX ? uvMinMax.z : uvMinMax.x, flipY ? uvMinMax.w : uvMinMax.y), ImVec2(!flipX ? uvMinMax.z : uvMinMax.x, !flipY ? uvMinMax.w : uvMinMax.y), color);
}

static void DrawMetasprite(const Metasprite* pMetasprite, const ImVec2& origin, r32 renderScale, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const r32 tileDrawSize = TILE_DIM_PIXELS * renderScale;
	for (s32 i = pMetasprite->spriteCount - 1; i >= 0; i--) {
		const Sprite& sprite = pMetasprite->spritesRelativePos[i];
		const ImVec2 pos = ImVec2(origin.x + renderScale * sprite.x, origin.y + renderScale * sprite.y);
		DrawSprite(sprite, pos, renderScale, color);
	}
}

static AABB GetActorBoundingBox(const RoomActor* pActor) {
	AABB result{};
	if (pActor == nullptr) {
		return result;
	}

	constexpr r32 tileWorldDim = 1.0f / METATILE_DIM_TILES;
	
	// TODO: What if animation changes bounds?
	const ActorPrototype* pPrototype = Assets::GetActorPrototype(pActor->prototypeIndex);
	const Animation& anim = pPrototype->animations[0];
	switch (anim.type) {
	case ANIMATION_TYPE_SPRITES: {
		const Metasprite* pMetasprite = Metasprites::GetMetasprite(anim.metaspriteIndex);
		Sprite& sprite = pMetasprite->spritesRelativePos[0];
		result.min = { (r32)sprite.x / METATILE_DIM_PIXELS, (r32)sprite.y / METATILE_DIM_PIXELS };
		result.max = { result.min.x + tileWorldDim, result.min.y + tileWorldDim };
		break;
	}
	case ANIMATION_TYPE_METASPRITES: {
		const Metasprite* pMetasprite = Metasprites::GetMetasprite(anim.metaspriteIndex);

		result.x1 = std::numeric_limits<r32>::max();
		result.x2 = std::numeric_limits<r32>::min();
		result.y1 = std::numeric_limits<r32>::max();
		result.y2 = std::numeric_limits<r32>::min();

		for (u32 i = 0; i < pMetasprite->spriteCount; i++) {
			Sprite& sprite = pMetasprite->spritesRelativePos[i];
			const glm::vec2 spriteMin = { (r32)sprite.x / METATILE_DIM_PIXELS, (r32)sprite.y / METATILE_DIM_PIXELS };
			const glm::vec2 spriteMax = { spriteMin.x + tileWorldDim, spriteMin.y + tileWorldDim };
			result.x1 = glm::min(result.x1, spriteMin.x);
			result.x2 = glm::max(result.x2, spriteMax.x);
			result.y1 = glm::min(result.y1, spriteMin.y);
			result.y2 = glm::max(result.y2, spriteMax.y);
		}
		break;
	}
	default:
		break;
	}

	return result;
}

static void DrawActor(const ActorPrototype* pPrototype, const ImVec2& origin, r32 renderScale, s32 animIndex = 0, s32 frameIndex = 0, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	const Animation& anim = pPrototype->animations[animIndex];
	switch (anim.type) {
	case ANIMATION_TYPE_SPRITES: {
		const Metasprite* pMetasprite = Metasprites::GetMetasprite(anim.metaspriteIndex);
		Sprite& sprite = pMetasprite->spritesRelativePos[frameIndex];
		ImVec2 pos = ImVec2(origin.x + renderScale * sprite.x, origin.y + renderScale * sprite.y);
		DrawSprite(sprite, pos, renderScale, color);
		break;
	}
	case ANIMATION_TYPE_METASPRITES: {
		const Metasprite* pMetasprite = Metasprites::GetMetasprite(anim.metaspriteIndex + frameIndex);
		DrawMetasprite(pMetasprite, origin, renderScale, color);
		break;
	}
	default:
		break;
	}
}

static void DrawHitbox(const AABB* pHitbox, const ImVec2 origin, const r32 renderScale, ImU32 color = IM_COL32(0, 255, 0, 80)) {
	const r32 colliderDrawScale = METATILE_DIM_PIXELS * renderScale;

	const ImVec2 pMin = ImVec2(origin.x + colliderDrawScale * pHitbox->x1, origin.y + colliderDrawScale * pHitbox->y1);
	const ImVec2 pMax = ImVec2(origin.x + colliderDrawScale * pHitbox->x2, origin.y + colliderDrawScale * pHitbox->y2);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(pMin, pMax, color);
}

static void swap(Metasprite& a, Metasprite& b) {
	char* nameA = Metasprites::GetName(&a);
	char* nameB = Metasprites::GetName(&b);

	u32 tempSpriteCount = a.spriteCount;
	char tempName[METASPRITE_MAX_NAME_LENGTH];
	Sprite tempSprites[METASPRITE_MAX_SPRITE_COUNT];

	memcpy(tempName, nameA, METASPRITE_MAX_NAME_LENGTH);
	memcpy(tempSprites, a.spritesRelativePos, METASPRITE_MAX_SPRITE_COUNT * sizeof(Sprite));

	a.spriteCount = b.spriteCount;
	memcpy(nameA, nameB, METASPRITE_MAX_NAME_LENGTH);
	memcpy(a.spritesRelativePos, a.spritesRelativePos, METASPRITE_MAX_SPRITE_COUNT * sizeof(Sprite));

	b.spriteCount = tempSpriteCount;
	memcpy(nameB, tempName, METASPRITE_MAX_NAME_LENGTH);
	memcpy(b.spritesRelativePos, tempSprites, METASPRITE_MAX_SPRITE_COUNT * sizeof(Sprite));
}

// Callback for displaying waveform in audio window
static r32 GetAudioSample(void* data, s32 idx) {
	return ((r32)((u8*)data)[idx]);
}

template <typename T>
static bool TrySwapElements(T* elements, ImVector<s32>& elementIndices, s32 i, s32 dir) {
	s32& elementIndex = elementIndices[i];
	s32 nextElementIndex = elementIndex + dir;

	if (elementIndices.contains(nextElementIndex)) {
		return false;
	}

	std::swap(elements[elementIndex], elements[nextElementIndex]);
	elementIndex += dir;

	return true;
}

template <typename T>
static void MoveElements(T* elements, ImVector<s32>& elementIndices, s32 step) {
	if (step == 0) {
		return;
	}

	s32 absStep = glm::abs(step);
	s32 dir = step / absStep;

	ImVector<s32> alreadyMoved = {};
	for (s32 s = 0; s < absStep; s++) {
		alreadyMoved.clear();
		while (alreadyMoved.size() < elementIndices.size()) {
			for (u32 i = 0; i < elementIndices.size(); i++) {
				if (alreadyMoved.contains(i))
					continue;

				if (TrySwapElements(elements, elementIndices, i, dir)) {
					alreadyMoved.push_back(i);
				}
			}
		}
	}
}

template <typename T>
static void MoveElementsRange(T* elements, s32 begin, s32 end, s32 step, ImVector<s32>* selection) {
	ImVector<s32> movedIndices;
	for (s32 i = begin; i < end; i++) {
		movedIndices.push_back(i);
	}
	MoveElements(elements, movedIndices, step);

	// Fix selection after moving
	if (selection != nullptr) {
		for (s32 i = 0; i < selection->size(); i++) {
			s32& elementIndex = (*selection)[i];
			if (elementIndex >= begin && elementIndex < end) {
				elementIndex += step;
			}
		}
	}
}

static bool CanMoveElements(u32 totalCount, const ImVector<s32>& elementIndices, s32 step) {
	s32 minIndex = INT_LEAST32_MAX;
	s32 maxIndex = 0;

	for (u32 i = 0; i < elementIndices.size(); i++) {
		minIndex = glm::min(minIndex, elementIndices[i]);
		maxIndex = glm::max(maxIndex, elementIndices[i]);
	}

	if (step < 0) {
		return minIndex + step >= 0;
	}
	else {
		return maxIndex + step < totalCount;
	}
}

template <typename T>
static u32 PushElement(T* elements, u32& count, const T& element) {
	const u32 newIndex = count++;
	elements[newIndex] = element;

	return newIndex;
}

template <typename T>
static u32 PushElement(T* elements, u32& count) {
	return PushElement(elements, count, T{});
}

template <typename T>
static void PopElement(T* elements, u32& count) {
	elements[--count] = T{};
}

static bool SelectElement(ImVector<s32>& selection, bool selectionLocked, s32 index) {
	bool multiple = ImGui::IsKeyDown(ImGuiKey_ModCtrl);
	bool selected = selection.contains(index);

	if (selectionLocked) {
		return selected;
	}

	if (multiple) {
		if (selected) {
			selection.find_erase_unsorted(index);
			return false;
		}
	}
	else {
		selection.clear();
	}

	selection.push_back(index);
	return true;
}

template <typename T>
static void DrawGenericEditableList(T* elements, u32& count, u32 maxCount, ImVector<s32>& selection, const char* labelPrefix, bool selectionLocked = false, void (*drawExtraStuff)(const T&) = nullptr) {
	ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
	for (u32 i = 0; i < count; i++) {
		T& element = elements[i];
		static char labelStr[64];
		snprintf(labelStr, 64, "%s 0x%02x", labelPrefix, i);

		bool selected = selection.contains(i);

		ImGui::PushID(i);
		ImGui::SetNextItemAllowOverlap();
		if (ImGui::Selectable(labelStr, selected, selectableFlags, ImVec2(0, 0))) {
			SelectElement(selection, selectionLocked, i);
		}
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::Selectable("Remove")) {
				if (selected) {
					selection.find_erase_unsorted(i);
				}
				MoveElementsRange<T>(elements, i + 1, count, -1, &selection);
				PopElement<T>(elements, count);
			}
			if (ImGui::Selectable("Insert above")) {
				u32 newInd = PushElement<T>(elements, count);
				MoveElementsRange<T>(elements, i, newInd, 1, &selection);
				if (!selectionLocked) {
					selection.clear();
					selection.push_back(i);
				}
			}
			if (ImGui::Selectable("Insert below")) {
				u32 newInd = PushElement<T>(elements, count);
				MoveElementsRange<T>(elements, i + 1, newInd, 1, &selection);
				if (!selectionLocked) {
					selection.clear();
					selection.push_back(i + 1);
				}
			}
			if (ImGui::Selectable("Duplicate")) { // And insert below
				u32 newInd = PushElement<T>(elements, count, element);
				MoveElementsRange<T>(elements, i + 1, newInd, 1, &selection);
				if (!selectionLocked) {
					selection.clear();
					selection.push_back(i + 1);
				}
			}
			ImGui::EndPopup();
		}

		if (selected) {
			ImGui::SetItemDefaultFocus();
		}

		if (drawExtraStuff) {
			drawExtraStuff(element);
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			if (selected || SelectElement(selection, selectionLocked, i)) {
				// Set payload to carry the index of our item (could be anything)
				ImGui::SetDragDropPayload("swap_sprites", &i, sizeof(u32));
				ImGui::Text("%s", labelStr);
			}
			else {
				ImGui::Text("Selection locked!");
			}

			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("swap_sprites"))
			{
				int sourceIndex = *(const u32*)payload->Data;

				s32 step = i - sourceIndex;
				const bool canMove = CanMoveElements(count, selection, step);

				if (canMove) {
					MoveElements<T>(elements, selection, step);
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::PopID();
	}
}

template <typename T>
static void DrawTypeSelectionCombo(const char* label, const char* const* const typeNames, u32 typeCount, T& selection) {
	static_assert(std::is_integral<T>::value);

	const char* noSelectionLabel = "NONE";
	const char* noItemsLabel = "NO ITEMS";
	const char* selectedLabel = typeCount > 0 ? (selection >= 0 ? typeNames[selection] : noSelectionLabel) : noItemsLabel;

	if (ImGui::BeginCombo(label, selectedLabel)) {
		if (ImGui::Selectable(noSelectionLabel, selection < 0)) {
			selection = -1;
		}

		for (u32 i = 0; i < typeCount; i++) {
			ImGui::PushID(i);

			const bool selected = selection == i;
			if (ImGui::Selectable(typeNames[i], selected)) {
				selection = i;
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
}
#pragma endregion

#pragma region Debug
static void DrawArrow(ImVec2 start, ImVec2 end, float arrowSize, ImU32 color) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	// Draw the line
	drawList->AddLine(start, end, color);

	// Calculate arrowhead points
	ImVec2 direction = { end.x - start.x, end.y - start.y };
	float length = sqrtf(direction.x * direction.x + direction.y * direction.y);
	if (length == 0.0f) return; // Avoid division by zero

	direction.x /= length;
	direction.y /= length;

	ImVec2 perpendicular = { -direction.y, direction.x };

	ImVec2 arrowTip = end;
	ImVec2 arrowLeft = { end.x - direction.x * arrowSize + perpendicular.x * arrowSize * 0.5f,
						 end.y - direction.y * arrowSize + perpendicular.y * arrowSize * 0.5f };
	ImVec2 arrowRight = { end.x - direction.x * arrowSize - perpendicular.x * arrowSize * 0.5f,
						  end.y - direction.y * arrowSize - perpendicular.y * arrowSize * 0.5f };

	// Draw the arrowhead
	drawList->AddTriangleFilled(arrowTip, arrowLeft, arrowRight, color);
}

static void DrawActorDebugInfo(const ImVec2& pos, const ImVec2& size) {
	const glm::vec2 viewportPos = Game::Rendering::GetViewportPos();
	const r32 renderScale = size.y / VIEWPORT_HEIGHT_PIXELS;

	const DynamicActorPool* actors = Game::GetActors();
	const glm::vec2 viewportPixelPos = viewportPos * r32(METATILE_DIM_PIXELS);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	for (u32 i = 0; i < actors->Count(); i++)
	{
		PoolHandle<Actor> handle = actors->GetHandle(i);
		const Actor* pActor = actors->Get(handle);

		const glm::vec2 actorPixelPos = pActor->position * (r32)METATILE_DIM_PIXELS;
		const glm::vec2 pixelOffset = actorPixelPos - viewportPixelPos;
		const ImVec2 drawPos = ImVec2(pos.x + pixelOffset.x * renderScale, pos.y + pixelOffset.y * renderScale);

		if (pContext->drawActorHitboxes) {
			DrawHitbox(&pActor->pPrototype->hitbox, drawPos, renderScale);
		}
		if (pContext->drawActorPositions) {
			char positionText[64];
			sprintf(positionText, "(%.2f, %.2f)", pActor->position.x, pActor->position.y);

			drawList->AddText(drawPos, IM_COL32(255, 255, 0, 255), positionText);
		}
	}
}

static void DrawDebugOverlay() {
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Game View Overlay", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);

	DrawActorDebugInfo(viewport->Pos, viewport->Size);

	ImGui::End();
	ImGui::PopStyleVar(2);
}

static void DrawDebugConsole() {
	if (ImGui::SmallButton("Clear log")) {
		pContext->consoleLog.clear();
	}

	ImGui::Separator();

	if (ImGui::BeginChild("Output", ImVec2(0,0), 0, ImGuiWindowFlags_HorizontalScrollbar)) {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

		for (const char* msg : pContext->consoleLog) {
			ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			if (strstr(msg, "[error]")) { 
				color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(msg);
			ImGui::PopStyleColor();
		}

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::PopStyleVar();
	}
	ImGui::EndChild();
}

static void DrawDebugWindow() {
	ImGui::Begin("Debug", &pContext->debugWindowOpen);

	if (ImGui::BeginTabBar("Debug tabs")) {
		if (ImGui::BeginTabItem("Console")) {

			DrawDebugConsole();

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Sprites")) {
			// TODO: Display layers on different tabs
			ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody;
			if (ImGui::BeginTable("sprites", 7, flags)) {
				ImGui::TableSetupColumn("Sprite");
				ImGui::TableSetupColumn("Pos");
				ImGui::TableSetupColumn("Tile");
				ImGui::TableSetupColumn("Palette");
				ImGui::TableSetupColumn("Priority");
				ImGui::TableSetupColumn("Flip H");
				ImGui::TableSetupColumn("Flip V");
				ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
				ImGui::TableHeadersRow();

				Sprite* sprites = Rendering::GetSpritesPtr(0);
				for (u32 i = 0; i < MAX_SPRITE_COUNT; i++) {
					const Sprite& sprite = sprites[i];
					ImGui::PushID(i);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("0x%03x", i);
					ImGui::TableNextColumn();
					ImGui::Text("(%d, %d)", sprite.x, sprite.y);
					ImGui::TableNextColumn();
					ImGui::Text("0x%02x", sprite.tileId);
					ImGui::TableNextColumn();
					ImGui::Text("0x%02x", sprite.palette);
					ImGui::TableNextColumn();
					ImGui::Text("0x%01x", sprite.priority);
					ImGui::TableNextColumn();
					ImGui::Text("0x%01x", sprite.flipHorizontal);
					ImGui::TableNextColumn();
					ImGui::Text("0x%01x", sprite.flipVertical);
					ImGui::PopID();
				}

				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Nametables")) {
			const ImVec2 nametableSizePx = ImVec2(NAMETABLE_DIM_PIXELS, NAMETABLE_DIM_PIXELS);

			for (u32 i = 0; i < NAMETABLE_COUNT; i++) {
				ImGui::PushID(i);
				Nametable* const nametables = Rendering::GetNametablePtr(0);
				DrawNametable(nametableSizePx, nametables[i]);
				ImGui::PopID();
				ImGui::SameLine();
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Pattern tables")) {
			static s32 selectedPalettes[2]{};

			for (int i = 0; i < PALETTE_COUNT; i++) {
				if (i == BG_PALETTE_COUNT) {
					ImGui::NewLine();
				}

				ImGui::PushID(i);
				if (DrawPaletteButton(i)) {
					if (i < BG_PALETTE_COUNT) {
						selectedPalettes[0] = i;
					}
					else selectedPalettes[1] = i - BG_PALETTE_COUNT;
				}
				ImGui::PopID();
				ImGui::SameLine();
			}
			ImGui::NewLine();

			constexpr s32 renderScale = 3;
			const r32 chrWidth = CHR_DIM_PIXELS * renderScale;

			for (u32 i = 0; i < CHR_COUNT; i++) {
				ImGui::PushID(i);
				DrawCHRSheet(chrWidth, i, selectedPalettes[i], nullptr);
				ImGui::PopID();
				ImGui::SameLine();
			}
			
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Palette")) {
			ImGui::Text("TODO: Render all colors into a texture and display here");
			if (ImGui::Button("Save palette to file")) {
				Rendering::Util::SavePaletteToFile("generated.pal");
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Settings")) {
			ImGui::Checkbox("Show Game View Overlay", &pContext->showDebugOverlay);
			ImGui::SeparatorText("Overlay settings");
			ImGui::Checkbox("Draw actor hitboxes", &pContext->drawActorHitboxes);
			ImGui::Checkbox("Draw actor positions", &pContext->drawActorPositions);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}
#pragma endregion

#pragma region Sprites
static void DrawMetaspriteList(s32& selection) {
	static constexpr u32 maxLabelNameLength = METASPRITE_MAX_NAME_LENGTH + 8;
	char label[maxLabelNameLength];

	Metasprite* pMetasprites = Metasprites::GetMetasprite(0);
	for (u32 i = 0; i < MAX_METASPRITE_COUNT; i++)
	{
		const char* name = Metasprites::GetName(i);
		ImGui::PushID(i);

		snprintf(label, maxLabelNameLength, "0x%02x: %s", i, name);

		const bool selected = selection == i;
		if (ImGui::Selectable(label, selected)) {
			selection = i;
		}

		if (selected) {
			ImGui::SetItemDefaultFocus();
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			ImGui::SetDragDropPayload("dd_metasprites", &i, sizeof(u32));
			ImGui::Text("%s", name);

			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("dd_metasprites"))
			{
				int sourceIndex = *(const u32*)payload->Data;

				s32 step = i - sourceIndex;

				ImVector<s32> vec;
				vec.push_back(sourceIndex);

				const bool canMove = CanMoveElements(MAX_METASPRITE_COUNT, vec, step);

				if (canMove) {
					MoveElements<Metasprite>(pMetasprites, vec, step);
					selection = vec[0];
				}
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::PopID();
	}
}

static void DrawMetaspritePreview(Metasprite& metasprite, ImVector<s32>& spriteSelection, bool selectionLocked, r32 size) {
	constexpr s32 gridSizeTiles = 8;

	const r32 renderScale = size / (gridSizeTiles * TILE_DIM_PIXELS);
	const r32 gridStepPixels = TILE_DIM_PIXELS * renderScale;
	
	static bool gridFocused = false;
	static bool dragging = false;
	static ImVec2 dragDelta = ImVec2(0, 0);

	ImVec2 gridPos = DrawTileGrid(ImVec2(size, size), gridStepPixels, nullptr, &gridFocused);
	ImVec2 origin = ImVec2(gridPos.x + size / 2, gridPos.y + size / 2);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddLine(ImVec2(origin.x - 10, origin.y), ImVec2(origin.x + 10, origin.y), IM_COL32(200, 200, 200, 255));
	drawList->AddLine(ImVec2(origin.x, origin.y - 10), ImVec2(origin.x, origin.y + 10), IM_COL32(200, 200, 200, 255));

	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && gridFocused) {
		dragging = true;
		dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

		// Pixel snap
		dragDelta.x = glm::roundEven(dragDelta.x / renderScale) * renderScale;
		dragDelta.y = glm::roundEven(dragDelta.y / renderScale) * renderScale;
	}
	s32 trySelect = (gridFocused && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ? -2 : -1; // -2 = deselect all (Clicked outside tiles)
	ImGuiIO& io = ImGui::GetIO();

	for (s32 i = metasprite.spriteCount - 1; i >= 0; i--) {
		Sprite& sprite = metasprite.spritesRelativePos[i];

		const u8 index = (u8)sprite.tileId;
		const bool flipX = sprite.flipHorizontal;
		const bool flipY = sprite.flipVertical;
		const u8 palette = sprite.palette;

		glm::vec4 uvMinMax = ChrTileToTexCoord(index, 1, palette);
		ImVec2 pos = ImVec2(origin.x + renderScale * sprite.x, origin.y + renderScale * sprite.y);

		// Select sprite by clicking (Topmost sprite gets selected)
		bool spriteClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x >= pos.x && io.MousePos.x < pos.x + gridStepPixels && io.MousePos.y >= pos.y && io.MousePos.y < pos.y + gridStepPixels;
		if (spriteClicked) {
			trySelect = i;
		}

		bool selected = spriteSelection.contains(i);
		// Move sprite if dragged
		ImVec2 posWithDrag = selected ? ImVec2(pos.x + dragDelta.x, pos.y + dragDelta.y) : pos;

		drawList->AddImage(pContext->chrTexture, posWithDrag, ImVec2(posWithDrag.x + gridStepPixels, posWithDrag.y + gridStepPixels), ImVec2(flipX ? uvMinMax.z : uvMinMax.x, flipY ? uvMinMax.w : uvMinMax.y), ImVec2(!flipX ? uvMinMax.z : uvMinMax.x, !flipY ? uvMinMax.w : uvMinMax.y));


		// Commit drag
		if (selected && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			ImVec2 deltaInTileCoord = ImVec2(dragDelta.x / renderScale, dragDelta.y / renderScale);
			sprite.x += deltaInTileCoord.x;
			sprite.y += deltaInTileCoord.y;
		}

		// Draw selection box
		if (selected) {
			drawList->AddRect(posWithDrag, ImVec2(posWithDrag.x + gridStepPixels, posWithDrag.y + gridStepPixels), IM_COL32(255, 255, 255, 255));
		}
	}

	if (trySelect >= 0 && !selectionLocked && !dragging) {
		if (ImGui::IsKeyDown(ImGuiKey_ModCtrl))
		{
			if (spriteSelection.contains(trySelect))
				spriteSelection.find_erase_unsorted(trySelect);
			else
				spriteSelection.push_back(trySelect);
		}
		else if (!spriteSelection.contains(trySelect))
		{
			spriteSelection.clear();
			spriteSelection.push_back(trySelect);
		}
	}
	else if (trySelect == -2 && !ImGui::IsKeyDown(ImGuiKey_ModCtrl) && !selectionLocked) {
		spriteSelection.clear();
	}

	// Reset drag delta when mouse released
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		dragging = false;
		dragDelta = ImVec2(0, 0);
	}
}

static void DrawSpriteEditor(Metasprite& metasprite, ImVector<s32>& spriteSelection) {
	ImGui::SeparatorText("Sprite editor");
	
	if (spriteSelection.empty()) {
		ImGui::TextUnformatted("No sprite selected");
	}
	else if (spriteSelection.size() > 1) {
		ImGui::TextUnformatted("Multiple sprites selected");
	}
	else {
		s32& spriteIndex = spriteSelection[0];
		Sprite& sprite = metasprite.spritesRelativePos[spriteIndex];
		s32 index = (s32)sprite.tileId;

		bool flipX = sprite.flipHorizontal;
		bool flipY = sprite.flipVertical;

		s32 newId = (s32)sprite.tileId;
		r32 chrSheetSize = 256;
		DrawCHRSheet(chrSheetSize, 1, sprite.palette, &newId);

		if (newId != sprite.tileId) {
			sprite.tileId = (u8)newId;
		}

		ImGui::SameLine();
		ImGui::BeginChild("sprite palette", ImVec2(0, chrSheetSize));
		{
			for (int i = 0; i < FG_PALETTE_COUNT; i++) {
				ImGui::PushID(i);
				if (DrawPaletteButton(i + BG_PALETTE_COUNT)) {
					sprite.palette = i;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		ImGui::Text("Position: (%d, %d)", sprite.x, sprite.y);

		if (ImGui::Checkbox("Flip horizontal", &flipX)) {
			sprite.flipHorizontal = flipX;
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Flip vertical", &flipY)) {
			sprite.flipVertical = flipY;
		}
	}
}

static void DrawSpriteListPreview(const Sprite& sprite) {
	// Draw a nice little preview of the sprite
	u8 index = (u8)sprite.tileId;
	const bool flipX = sprite.flipHorizontal;
	const bool flipY = sprite.flipVertical;
	glm::vec4 uvMinMax = ChrTileToTexCoord(index, 1, sprite.palette);
	ImGuiStyle& style = ImGui::GetStyle();
	r32 itemHeight = ImGui::GetItemRectSize().y - style.FramePadding.y;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	const ImVec2 topLeft = ImVec2(ImGui::GetItemRectMin().x + 88, ImGui::GetItemRectMin().y + style.FramePadding.y);
	const ImVec2 btmRight = ImVec2(topLeft.x + itemHeight, topLeft.y + itemHeight);
	drawList->AddImage(pContext->chrTexture, topLeft, btmRight, ImVec2(flipX ? uvMinMax.z : uvMinMax.x, flipY ? uvMinMax.w : uvMinMax.y), ImVec2(!flipX ? uvMinMax.z : uvMinMax.x, !flipY ? uvMinMax.w : uvMinMax.y));
}

static void DrawMetaspriteEditor(Metasprite& metasprite, ImVector<s32>& spriteSelection, bool& selectionLocked, bool& showColliderPreview) {
	ImGui::Checkbox("Lock selection", &selectionLocked);

	ImGui::BeginDisabled(metasprite.spriteCount == METASPRITE_MAX_SPRITE_COUNT);
	if (ImGui::Button("+")) {
		PushElement<Sprite>(metasprite.spritesRelativePos, metasprite.spriteCount);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(metasprite.spriteCount == 0);
	if (ImGui::Button("-")) {
		PopElement<Sprite>(metasprite.spritesRelativePos, metasprite.spriteCount);
	}
	ImGui::EndDisabled();

	ImGui::BeginChild("Sprite list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

	DrawGenericEditableList<Sprite>(metasprite.spritesRelativePos, metasprite.spriteCount, METASPRITE_MAX_SPRITE_COUNT, spriteSelection, "Sprite", selectionLocked, DrawSpriteListPreview);

	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("Sprite editor");
	DrawSpriteEditor(metasprite, spriteSelection);
	ImGui::EndChild();
}

static void DrawSpriteWindow() {
	ImGui::Begin("Metasprites", &pContext->spriteWindowOpen, ImGuiWindowFlags_MenuBar);

	static s32 selection = 0;

	Metasprite* pMetasprites = Metasprites::GetMetasprite(0);
	Metasprite& selectedMetasprite = pMetasprites[selection];

	static ImVector<s32> spriteSelection;
	static bool selectionLocked = false;

	static bool showColliderPreview = false;

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save")) {
				Metasprites::Save("assets/meta.spr");
			}
			if (ImGui::MenuItem("Revert changes")) {
				Metasprites::Load("assets/meta.spr");
				spriteSelection.clear();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::BeginChild("Metasprite list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	s32 newSelection = selection;
	DrawMetaspriteList(newSelection);
	if (newSelection != selection) {
		selection = newSelection;
		spriteSelection.clear();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("Metasprite editor");

	char* name = Metasprites::GetName(selection);
	ImGui::SeparatorText(name);
	ImGui::InputText("Name", name, METASPRITE_MAX_NAME_LENGTH);

	ImGui::Separator();

	constexpr r32 previewSize = 256;
	ImGui::BeginChild("Metasprite preview", ImVec2(previewSize, previewSize));
	DrawMetaspritePreview(selectedMetasprite, spriteSelection, selectionLocked, previewSize);
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::BeginChild("Metasprite properties");
	DrawMetaspriteEditor(selectedMetasprite, spriteSelection, selectionLocked, showColliderPreview);
	ImGui::EndChild();

	ImGui::EndChild();

	// Copy from other metasprite
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("dd_metasprites"))
		{
			int metaspriteIndex = *(const u32*)payload->Data;

			// Would be nice to have a tooltip here but it didn't work :c

			if (metaspriteIndex != selection) {
				Metasprites::Copy(metaspriteIndex, selection);
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::End();
}
#pragma endregion

#pragma region Tileset
static void DrawTilesetWindow() {
	ImGui::Begin("Tileset", &pContext->tilesetWindowOpen, ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save")) {
				Tiles::SaveTileset("assets/forest.til");
			}
			if (ImGui::MenuItem("Revert changes")) {
				Tiles::LoadTileset("assets/forest.til");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	static s32 selectedMetatileIndex = 0;
	static s32 selectedTileIndex = 0;

	constexpr s32 renderScale = 2;
	constexpr s32 gridSizeTiles = 16;
	constexpr s32 gridStepPixels = METATILE_DIM_PIXELS * renderScale;
	constexpr s32 gridSizePixels = gridSizeTiles * gridStepPixels;

	Tileset* pTileset = Tiles::GetTileset();
	DrawTileset(pTileset, gridSizePixels, &selectedMetatileIndex);

	ImGui::SameLine();

	ImGui::BeginChild("Metatile editor");
	{
		constexpr r32 tilePreviewSize = 64;
		constexpr r32 pixelSize = tilePreviewSize / TILE_DIM_PIXELS;

		ImGui::SeparatorText("Metatile editor");

		Metatile& metatile = pTileset->tiles[selectedMetatileIndex].metatile;
		s32 tileId = metatile.tiles[selectedTileIndex].tileId;

		s32 palette = metatile.tiles[selectedTileIndex].palette;
		r32 chrSheetSize = 256;
		ImGui::PushID(0);
		DrawCHRSheet(chrSheetSize, 0, palette, &tileId);
		ImGui::PopID();
		if (tileId != metatile.tiles[selectedTileIndex].tileId) {
			metatile.tiles[selectedTileIndex].tileId = tileId;
		}

		ImGui::Text("0x%02x", selectedMetatileIndex);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 gridSize = ImVec2(tilePreviewSize, tilePreviewSize);
		ImGui::PushID(1);
		const ImVec2 tilePos = DrawTileGrid(gridSize, gridStepPixels, &selectedTileIndex);
		ImGui::PopID();
		DrawMetatile(metatile, tilePos, tilePreviewSize);
		DrawTileGridSelection(tilePos, gridSize, gridStepPixels, selectedTileIndex);

		s32& type = pTileset->tiles[selectedMetatileIndex].type;
		ImGui::SliderInt("Type", &type, 0, TILE_TYPE_COUNT - 1, METATILE_TYPE_NAMES[type]);

		ImGui::SeparatorText("Tile Settings");

		if (ImGui::SliderInt("Palette", &palette, 0, BG_PALETTE_COUNT - 1)) {
			metatile.tiles[selectedTileIndex].palette = palette;
		}

		bool flipHorizontal = metatile.tiles[selectedTileIndex].flipHorizontal;
		if (ImGui::Checkbox("Flip Horizontal", &flipHorizontal)) {
			metatile.tiles[selectedTileIndex].flipHorizontal = flipHorizontal;
		}

		bool flipVertical = metatile.tiles[selectedTileIndex].flipVertical;
		if (ImGui::Checkbox("Flip Vertical", &flipVertical)) {
			metatile.tiles[selectedTileIndex].flipVertical = flipVertical;
		}
	}
	ImGui::EndChild();


	ImGui::End();
}
#pragma endregion

#pragma region Room editor
static void DrawScreenBorders(u32 index, ImVec2 pMin, ImVec2 pMax, r32 renderScale) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	
	static char screenLabelText[16];
	snprintf(screenLabelText, 16, "%#04x", index);

	drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255));

	const ImVec2 textPos = ImVec2(pMin.x + TILE_DIM_PIXELS * renderScale, pMin.y + TILE_DIM_PIXELS * renderScale);
	drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), screenLabelText);
}

static void DrawRoomOverlay(const RoomTemplate* pTemplate, const glm::vec2& viewportPos, const ImVec2 topLeft, const ImVec2 btmRight, const r32 renderScale) {
	const glm::vec2 viewportPixelPos = viewportPos * r32(METATILE_DIM_PIXELS);
	const ImVec2 viewportDrawSize = ImVec2(VIEWPORT_WIDTH_PIXELS * renderScale, VIEWPORT_HEIGHT_PIXELS * renderScale);

	const s32 screenStartX = viewportPos.x / VIEWPORT_WIDTH_METATILES;
	const s32 screenStartY = viewportPos.y / VIEWPORT_HEIGHT_METATILES;

	const s32 screenEndX = (viewportPos.x + VIEWPORT_WIDTH_METATILES) / VIEWPORT_WIDTH_METATILES;
	const s32 screenEndY = (viewportPos.y + VIEWPORT_HEIGHT_METATILES) / VIEWPORT_HEIGHT_METATILES;

	for (s32 y = screenStartY; y <= screenEndY; y++) {
		for (s32 x = screenStartX; x <= screenEndX; x++) {
			const glm::vec2 screenPixelPos = { x * VIEWPORT_WIDTH_PIXELS, y * VIEWPORT_HEIGHT_PIXELS };
			const ImVec2 pMin = ImVec2((screenPixelPos.x - viewportPixelPos.x) * renderScale + topLeft.x, (screenPixelPos.y - viewportPixelPos.y) * renderScale + topLeft.y);
			const ImVec2 pMax = ImVec2(pMin.x + viewportDrawSize.x, pMin.y + viewportDrawSize.y);

			const s32 i = x + y * ROOM_MAX_DIM_SCREENS;

			DrawScreenBorders(i, pMin, pMax, renderScale);
		}
	}
}

static PoolHandle<RoomActor> GetHoveredActorHandle(const RoomTemplate* pTemplate, const ImVec2& mousePosInWorldCoords) {
	auto result = PoolHandle<RoomActor>::Null();
	// TODO: Some quadtree action needed desperately
	for (u32 i = 0; i < pTemplate->actors.Count(); i++) {
		PoolHandle<RoomActor> handle = pTemplate->actors.GetHandle(i);
		const RoomActor* pActor = pTemplate->actors.Get(handle);

		const AABB bounds = GetActorBoundingBox(pActor);
		if (Collision::PointInsideBox({ mousePosInWorldCoords.x, mousePosInWorldCoords.y }, bounds, pActor->position)) {
			result = handle;
			break;
		}
	}
	return result;
}

static void DrawRoomView(RoomTemplate* pTemplate, u32 editMode, TilemapClipboard& clipboard, PoolHandle<RoomActor>& selectedActorHandle, glm::vec2& viewportPos) {
	Nametable* pNametables = Rendering::GetNametablePtr(0);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 topLeft = ImGui::GetCursorScreenPos();
	static const r32 aspectRatio = (r32)VIEWPORT_WIDTH_PIXELS / VIEWPORT_HEIGHT_PIXELS;
	const r32 contentWidth = ImGui::GetContentRegionAvail().x;
	const r32 contentHeight = contentWidth / aspectRatio;
	const r32 renderScale = contentWidth / VIEWPORT_WIDTH_PIXELS;
	ImVec2 btmRight = ImVec2(topLeft.x + contentWidth, topLeft.y + contentHeight);

	ImGuiIO& io = ImGui::GetIO();
	const r32 tileDrawSize = METATILE_DIM_PIXELS * renderScale;
	const ImVec2 mousePosInViewportCoords = ImVec2((io.MousePos.x - topLeft.x) / tileDrawSize, (io.MousePos.y - topLeft.y) / tileDrawSize);
	const ImVec2 mousePosInWorldCoords = ImVec2(mousePosInViewportCoords.x + viewportPos.x, mousePosInViewportCoords.y + viewportPos.y);

	// Invisible button to prevent dragging window
	ImGui::InvisibleButton("##canvas", ImVec2(contentWidth, contentHeight), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

	const bool hovered = ImGui::IsItemHovered(); // Hovered
	const bool active = ImGui::IsItemActive();   // Held

	// Context menu handling
	if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		if (editMode == EDIT_MODE_ACTORS) {
			ImGui::OpenPopup("ActorContextMenu");
		}
	}

	drawList->PushClipRect(topLeft, btmRight, true);

	DrawTilemap(&pTemplate->tilemap, ImVec2(viewportPos.x, viewportPos.y), ImVec2(VIEWPORT_WIDTH_METATILES + 1, VIEWPORT_HEIGHT_METATILES + 1), topLeft, renderScale * METATILE_DIM_PIXELS);

	// View scrolling
	bool scrolling = false;

	static ImVec2 dragStartPos = ImVec2(0, 0);
	static ImVec2 dragDelta = ImVec2(0, 0);

	if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
		dragStartPos = io.MousePos;
		selectedActorHandle = PoolHandle<RoomActor>::Null();
	}

	if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		const ImVec2 newDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
		glm::vec2 newViewportPos = viewportPos;
		newViewportPos.x -= (newDelta.x - dragDelta.x) / renderScale / METATILE_DIM_PIXELS;
		newViewportPos.y -= (newDelta.y - dragDelta.y) / renderScale / METATILE_DIM_PIXELS;
		dragDelta = newDelta;

		const glm::vec2 min(0.0f, 0.0f);
		const glm::vec2 max = {
			(pTemplate->width - 1) * VIEWPORT_WIDTH_METATILES,
			(pTemplate->height - 1) * VIEWPORT_HEIGHT_METATILES 
		};

		viewportPos = glm::clamp(newViewportPos, min, max);
		scrolling = true;
	}

	// Reset drag delta when mouse released
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
		dragDelta = ImVec2(0, 0);
	}

	DrawRoomOverlay(pTemplate, viewportPos, topLeft, btmRight, renderScale);

	// Draw actors
	const glm::vec2 viewportPixelPos = viewportPos * r32(METATILE_DIM_PIXELS);
	for (u32 i = 0; i < pTemplate->actors.Count(); i++)
	{
		PoolHandle<RoomActor> handle = pTemplate->actors.GetHandle(i);
		const RoomActor* pActor = pTemplate->actors.Get(handle);

		const glm::vec2 actorPixelPos = pActor->position * (r32)METATILE_DIM_PIXELS;
		const glm::vec2 pixelOffset = actorPixelPos - viewportPixelPos;
		const ImVec2 drawPos = ImVec2(topLeft.x + pixelOffset.x * renderScale, topLeft.y + pixelOffset.y * renderScale);

		const u8 opacity = editMode == EDIT_MODE_ACTORS ? 255 : 80;
		const ActorPrototype* pPrototype = Assets::GetActorPrototype(pActor->prototypeIndex);
		DrawActor(pPrototype, drawPos, renderScale, 0, 0, IM_COL32(255, 255, 255, opacity));
	}

	const ImVec2 hoveredTileWorldPos = ImVec2(glm::floor(mousePosInWorldCoords.x), glm::floor(mousePosInWorldCoords.y));

	switch (editMode) {
	case EDIT_MODE_ACTORS:
	{
		RoomActor* pActor = pTemplate->actors.Get(selectedActorHandle);
		const AABB actorBounds = GetActorBoundingBox(pActor);
		PoolHandle<RoomActor> hoveredActorHandle = GetHoveredActorHandle(pTemplate, mousePosInWorldCoords);

		// Selection
		if (!scrolling) {
			static glm::vec2 selectionStartPos{};

			if (active && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
				selectedActorHandle = hoveredActorHandle;
				selectionStartPos = { mousePosInWorldCoords.x, mousePosInWorldCoords.y };
			}

			if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && pActor != nullptr) {
				const ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
				const glm::vec2 deltaInWorldCoords = { dragDelta.x / tileDrawSize, dragDelta.y / tileDrawSize };

				pActor->position = selectionStartPos + deltaInWorldCoords;
			}
		}

		if (ImGui::BeginPopup("ActorContextMenu")) {
			if (selectedActorHandle != PoolHandle<RoomActor>::Null()) {
				if (ImGui::MenuItem("Remove actor")) {
					pTemplate->actors.Remove(selectedActorHandle);
				}
			}
			else if (ImGui::MenuItem("Add actor")) {
				PoolHandle<RoomActor> handle = pTemplate->actors.Add();
				RoomActor* pNewActor = pTemplate->actors.Get(handle);
				pNewActor->prototypeIndex = 0;
				pNewActor->id = Random::GenerateUUID32();
				pNewActor->position = { mousePosInWorldCoords.x, mousePosInWorldCoords.y };
			}
			ImGui::EndPopup();
		}

		if (pActor != nullptr) {
			const AABB boundsAbs(actorBounds.min + pActor->position, actorBounds.max + pActor->position);
			const ImVec2 pMin = ImVec2((boundsAbs.min.x - viewportPos.x) * tileDrawSize + topLeft.x, (boundsAbs.min.y - viewportPos.y) * tileDrawSize + topLeft.y);
			const ImVec2 pMax = ImVec2((boundsAbs.max.x - viewportPos.x) * tileDrawSize + topLeft.x, (boundsAbs.max.y - viewportPos.y) * tileDrawSize + topLeft.y);

			drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255));
		}

		break;
	}
	case EDIT_MODE_TILES:
	{
		static bool selecting = false;

		// Selection
		if (!scrolling) {
			static ImVec2 selectionStartPos = ImVec2(0, 0);
			static ImVec2 selectionTopLeft;
			static ImVec2 selectionBtmRight;

			if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				selectionStartPos = hoveredTileWorldPos;
				selectionTopLeft = hoveredTileWorldPos;
				selectionBtmRight = ImVec2(hoveredTileWorldPos.x + 1, hoveredTileWorldPos.y + 1);
				selecting = true;
			}

			if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
				selectionTopLeft = ImVec2(glm::min(selectionStartPos.x, hoveredTileWorldPos.x), glm::min(selectionStartPos.y, hoveredTileWorldPos.y));
				selectionBtmRight = ImVec2(glm::max(selectionStartPos.x, hoveredTileWorldPos.x) + 1, glm::max(selectionStartPos.y, hoveredTileWorldPos.y) + 1);

				const ImVec2 selectionTopLeftInPixelCoords = ImVec2((selectionTopLeft.x - viewportPos.x) * tileDrawSize + topLeft.x, (selectionTopLeft.y - viewportPos.y) * tileDrawSize + topLeft.y);
				const ImVec2 selectionBtmRightInPixelCoords = ImVec2((selectionBtmRight.x - viewportPos.x) * tileDrawSize + topLeft.x, (selectionBtmRight.y - viewportPos.y) * tileDrawSize + topLeft.y);

				drawList->AddRectFilled(selectionTopLeftInPixelCoords, selectionBtmRightInPixelCoords, IM_COL32(255, 255, 255, 63));
			}

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
				selecting = false;
				u32 selectionWidth = (selectionBtmRight.x - selectionTopLeft.x);
				u32 selectionHeight = (selectionBtmRight.y - selectionTopLeft.y);

				clipboard.size = ImVec2((r32)selectionWidth, (r32)selectionHeight);
				clipboard.offset = ImVec2(selectionTopLeft.x - hoveredTileWorldPos.x, selectionTopLeft.y - hoveredTileWorldPos.y);

				for (u32 x = 0; x < selectionWidth; x++) {
					for (u32 y = 0; y < selectionHeight; y++) {
						u32 clipboardIndex = y * selectionWidth + x;

						const glm::ivec2 metatileWorldPos = { selectionTopLeft.x + x, selectionTopLeft.y + y };
						const s32 tilesetIndex = Tiles::GetTilesetTileIndex(&pTemplate->tilemap, metatileWorldPos);
						clipboard.clipboard[clipboardIndex] = tilesetIndex;
					}
				}
			}
		}

		if (selecting) {
			break;
		}

		const u32 clipboardWidth = (u32)clipboard.size.x;
		const u32 clipboardHeight = (u32)clipboard.size.y;
		const ImVec2 clipboardTopLeft = ImVec2(hoveredTileWorldPos.x + clipboard.offset.x, hoveredTileWorldPos.y + clipboard.offset.y);
		const ImVec2 clipboardBtmRight = ImVec2(clipboardTopLeft.x + clipboardWidth, clipboardTopLeft.y + clipboardHeight);
		for (u32 x = 0; x < clipboardWidth; x++) {
			for (u32 y = 0; y < clipboardHeight; y++) {
				u32 clipboardIndex = y * clipboardWidth + x;
				const glm::ivec2 metatileWorldPos = { clipboardTopLeft.x + x, clipboardTopLeft.y + y };
				const ImVec2 metatileInViewportCoords = ImVec2(metatileWorldPos.x - viewportPos.x, metatileWorldPos.y - viewportPos.y);
				const ImVec2 metatileInPixelCoords = ImVec2(metatileInViewportCoords.x * tileDrawSize + topLeft.x, metatileInViewportCoords.y * tileDrawSize + topLeft.y);
				const u8 metatileIndex = clipboard.clipboard[clipboardIndex];
					
				const Tileset* pTileset = Tiles::GetTileset();
				const Metatile& metatile = pTileset->tiles[metatileIndex].metatile;
				DrawMetatile(metatile, metatileInPixelCoords, tileDrawSize, IM_COL32(255, 255, 255, 127));

				// Paint metatiles
				if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && active) {
					Tiles::SetTilesetTile(&pTemplate->tilemap, metatileWorldPos, metatileIndex);

					const u32 nametableIndex = Rendering::Util::GetNametableIndexFromMetatilePos(metatileWorldPos);
					const glm::ivec2 nametablePos = Rendering::Util::GetNametableOffsetFromMetatilePos(metatileWorldPos);
					Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametablePos, metatile);
				}
			}
		}

		const ImVec2 clipboardTopLeftInPixelCoords = ImVec2((clipboardTopLeft.x - viewportPos.x) * tileDrawSize + topLeft.x, (clipboardTopLeft.y - viewportPos.y) * tileDrawSize + topLeft.y);
		const ImVec2 clipboardBtmRightInPixelCoords = ImVec2((clipboardBtmRight.x - viewportPos.x) * tileDrawSize + topLeft.x, (clipboardBtmRight.y - viewportPos.y) * tileDrawSize + topLeft.y);
		drawList->AddRect(clipboardTopLeftInPixelCoords, clipboardBtmRightInPixelCoords, IM_COL32(255, 255, 255, 255));
		break;
	}
	default:
		break;
	}

	drawList->PopClipRect();
}

static void DrawRoomTools(RoomTemplate* pTemplate, u32& editMode, RoomToolsState& state, TilemapClipboard& clipboard, PoolHandle<RoomActor>& selectedActorHandle) {
	const ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs;

	if (ImGui::BeginTabBar("Room tool tabs")) {
		if (state.propertiesOpen && ImGui::BeginTabItem("Properties", &state.propertiesOpen)) {
			editMode = EDIT_MODE_NONE;

			ImGui::SeparatorText(pTemplate->name);

			ImGui::InputText("Name", pTemplate->name, ROOM_MAX_NAME_LENGTH);

			s32 size[2] = { pTemplate->width, pTemplate->height };
			if (ImGui::InputInt2("Size", size)) {
				pTemplate->width = glm::clamp(size[0], 1, s32(ROOM_MAX_DIM_SCREENS));
				pTemplate->height = glm::clamp(size[1], 1, s32(ROOM_MAX_DIM_SCREENS));
			}

			ImGui::EndTabItem();
		}

		if (state.tilemapOpen && ImGui::BeginTabItem("Tilemap", &state.tilemapOpen)) {
			editMode = EDIT_MODE_TILES;
			ImGuiStyle& style = ImGui::GetStyle();

			const s32 currentSelection = (clipboard.size.x == 1 && clipboard.size.y == 1) ? clipboard.clipboard[0] : -1;
			s32 newSelection = currentSelection;

			const Tileset* pTileset = Tiles::GetTileset();
			DrawTileset(pTileset, ImGui::GetContentRegionAvail().x - style.WindowPadding.x, &newSelection);

			// Rewrite level editor clipboard if new selection was made
			if (newSelection != currentSelection) {
				clipboard.clipboard[0] = newSelection;
				clipboard.offset = ImVec2(0, 0);
				clipboard.size = ImVec2(1, 1);
			}

			if (currentSelection >= 0) {
				ImGui::Text("0x%02x", currentSelection);
			}

			ImGui::EndTabItem();
		}

		if (state.actorsOpen && ImGui::BeginTabItem("Actors", &state.actorsOpen)) {
			editMode = EDIT_MODE_ACTORS;

			RoomActor* pActor = pTemplate->actors.Get(selectedActorHandle);
			if (pActor == nullptr) {
				ImGui::Text("No actor selected");
			}
			else {
				ImGui::PushID(selectedActorHandle.Raw());

				ImGui::Text("UUID: %lu", pActor->id);

				if (ImGui::BeginCombo("Prototype", Assets::GetActorPrototypeName(pActor->prototypeIndex))) {
					for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
						ImGui::PushID(i);

						const bool selected = pActor->prototypeIndex == i;

						if (ImGui::Selectable(Assets::GetActorPrototypeName(i), selected)) {
							pActor->prototypeIndex = i;
						}

						if (selected) {
							ImGui::SetItemDefaultFocus();
						}
						ImGui::PopID();
					}

					ImGui::EndCombo();
				}

				ImGui::Separator();

				ImGui::InputFloat2("Position", (r32*)&pActor->position);

				ImGui::PopID();
			}

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

static void DrawRoomWindow() {
	ImGui::Begin("Room editor", &pContext->roomWindowOpen, ImGuiWindowFlags_MenuBar);

	static u32 selectedRoom = 0;
	static PoolHandle<RoomActor> selectedActorHandle = PoolHandle<RoomActor>::Null();

	static u32 editMode = EDIT_MODE_NONE;
	static TilemapClipboard clipboard{};
	static RoomToolsState toolsState{};
	static glm::vec2 viewportPos(0.0f);

	RoomTemplate* pEditedRoom = Assets::GetRoomTemplate(selectedRoom);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save")) {
				Assets::SaveRoomTemplates("assets/rooms.ass");
			}
			if (ImGui::MenuItem("Revert changes")) {
				Assets::LoadRoomTemplates("assets/rooms.ass");
				Game::Rendering::RefreshViewport();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Tools")) {
			if (ImGui::MenuItem("Properties")) {
				toolsState.propertiesOpen = true;
			}
			if (ImGui::MenuItem("Tilemap")) {
				toolsState.tilemapOpen = true;
			}
			if (ImGui::MenuItem("Actors")) {
				toolsState.actorsOpen = true;
			}

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::BeginChild("Room list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	{
		static constexpr u32 maxLabelNameLength = ROOM_MAX_NAME_LENGTH + 8;
		char label[maxLabelNameLength];

		for (u32 i = 0; i < MAX_ROOM_TEMPLATE_COUNT; i++)
		{
			ImGui::PushID(i);

			const RoomTemplate* pTemplate = Assets::GetRoomTemplate(i);
			snprintf(label, maxLabelNameLength, "%#04x: %s", i, pTemplate->name);

			const bool selected = selectedRoom == i;

			if (ImGui::Selectable(label, selected)) {
				selectedRoom = i;
				viewportPos = glm::vec2(0.0f);
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}

			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	const bool showToolsWindow = (toolsState.actorsOpen || toolsState.propertiesOpen || toolsState.tilemapOpen);
	const r32 toolWindowWidth = showToolsWindow ? 350.0f : 0.0f;
	ImGuiStyle& style = ImGui::GetStyle();
	const r32 gameViewWidth = ImGui::GetContentRegionAvail().x - style.WindowPadding.x - toolWindowWidth;

	ImGui::BeginChild("Game view", ImVec2(gameViewWidth, 0));
	ImGui::NewLine();
	DrawRoomView(pEditedRoom, editMode, clipboard, selectedActorHandle, viewportPos);
	ImGui::EndChild();

	ImGui::SameLine();

	// Reset edit mode, it will be set by the tools window
	editMode = EDIT_MODE_NONE;
	if (showToolsWindow) {
		ImGui::BeginChild("Room tools", ImVec2(toolWindowWidth,0));
		DrawRoomTools(pEditedRoom, editMode, toolsState, clipboard, selectedActorHandle);
		ImGui::EndChild();
	}

	ImGui::End();
}
#pragma endregion

#pragma region Actor prototypes
static ImVec2 DrawActorPreview(const ActorPrototype* pPrototype, s32 animIndex, s32 frameIndex, r32 size) {
	constexpr s32 gridSizeTiles = 8;

	const r32 renderScale = size / (gridSizeTiles * TILE_DIM_PIXELS);
	const r32 gridStepPixels = TILE_DIM_PIXELS * renderScale;
	ImVec2 gridPos = DrawTileGrid(ImVec2(size, size), gridStepPixels);
	ImVec2 origin = ImVec2(gridPos.x + size / 2, gridPos.y + size / 2);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddLine(ImVec2(origin.x - 10, origin.y), ImVec2(origin.x + 10, origin.y), IM_COL32(200, 200, 200, 255));
	drawList->AddLine(ImVec2(origin.x, origin.y - 10), ImVec2(origin.x, origin.y + 10), IM_COL32(200, 200, 200, 255));

	DrawActor(pPrototype, origin, renderScale, animIndex, frameIndex);

	return gridPos;
}

static void DrawActorPrototypeList(s32& selection) {
	static constexpr u32 maxLabelNameLength = ACTOR_PROTOTYPE_MAX_NAME_LENGTH + 8;
	char label[maxLabelNameLength];

	ActorPrototype* pPrototypes = Assets::GetActorPrototype(0);
	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++)
	{
		ImGui::PushID(i);

		const auto name = Assets::GetActorPrototypeName(i);
		snprintf(label, maxLabelNameLength, "0x%02x: %s", i, name);

		const bool selected = selection == i;
		if (ImGui::Selectable(label, selected)) {
			selection = i;
		}

		if (selected) {
			ImGui::SetItemDefaultFocus();
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			const TActorPrototypeIndex index = TActorPrototypeIndex(i);
			ImGui::SetDragDropPayload("dd_actor_prototype", &index, sizeof(TActorPrototypeIndex));
			ImGui::Text("%s", name);

			ImGui::EndDragDropSource();
		}
		ImGui::PopID();
	}
}

static void DrawActorPrototypeSelector(const char* label, const char* const* prototypeNames, TActorPrototypeIndex& selection) {
	DrawTypeSelectionCombo(label, prototypeNames, MAX_ACTOR_PROTOTYPE_COUNT, selection);
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("dd_actor_prototype"))
		{
			const TActorPrototypeIndex sourceIndex = *(const TActorPrototypeIndex*)payload->Data;
			selection = sourceIndex;
		}
		ImGui::EndDragDropTarget();
	}
}

static void DrawActorPrototypeProperty(const ActorEditorProperty& property, ActorPrototypeData& data, const char* const* prototypeNames) {
	void* propertyData = (u8*)&data + property.offset;

	switch (property.type) {
	case ACTOR_EDITOR_PROPERTY_SCALAR: {
		ImGui::InputScalarN(property.name, property.dataType, propertyData, property.components);
		break;
	}
	case ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX: {
		TActorPrototypeIndex& prototypeIndex = *(TActorPrototypeIndex*)propertyData;
		DrawActorPrototypeSelector(property.name, prototypeNames, prototypeIndex);
		break;
	}
	default: {
		break;
	}
	}
}

static void DrawActorWindow() {
	ImGui::Begin("Actor prototypes", &pContext->actorWindowOpen, ImGuiWindowFlags_MenuBar);

	const r32 framesElapsed = pContext->secondsElapsed * 60.f;

	static s32 selection;
	static bool showHitboxPreview = false;

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save")) {
				Assets::SaveActorPrototypes("assets/actors.prt");
			}
			if (ImGui::MenuItem("Revert changes")) {
				Assets::LoadActorPrototypes("assets/actors.prt");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	static ImVector<s32> selectedAnims;
	static s32 currentAnim = 0;

	ImGui::BeginChild("Prototype list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	s32 newSelection = selection;
	DrawActorPrototypeList(newSelection);
	if (newSelection != selection) {
		selection = newSelection;
		selectedAnims.clear();
		currentAnim = 0;
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("Prototype editor");
	{
		ActorPrototype* pPrototype = Assets::GetActorPrototype(selection);
		char* prototypeName = Assets::GetActorPrototypeName(selection);

		ImGui::SeparatorText(prototypeName);

		ImGui::InputText("Name", prototypeName, ACTOR_PROTOTYPE_MAX_NAME_LENGTH);

		ImGui::Separator();

		constexpr r32 previewSize = 256;
		ImGui::BeginChild("Actor preview", ImVec2(previewSize, previewSize));

		const Animation& anim = pPrototype->animations[currentAnim];
		const r32 animFramesElapsed = framesElapsed / anim.frameLength;
		const s32 currentFrame = anim.frameCount == 0 ? 0 : (s32)glm::floor(animFramesElapsed) % anim.frameCount;

		ImVec2 metaspriteGridPos = DrawActorPreview(pPrototype, currentAnim, currentFrame, previewSize);

		if (showHitboxPreview) {
			AABB& hitbox = pPrototype->hitbox;

			const r32 gridSizeTiles = 8;
			const r32 renderScale = previewSize / (gridSizeTiles * TILE_DIM_PIXELS);

			ImVec2 origin = ImVec2(metaspriteGridPos.x + previewSize / 2, metaspriteGridPos.y + previewSize / 2);

			DrawHitbox(&hitbox, origin, renderScale);
		}
		ImGui::EndChild();

		ImGui::SameLine();
		ImGuiStyle& style = ImGui::GetStyle();
		const r32 timelineWidth = ImGui::GetContentRegionAvail().x;
		ImGui::BeginChild("Timeline", ImVec2(timelineWidth - style.WindowPadding.x, previewSize));
		{
			ImGui::SeparatorText("Timeline");

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			static const ImVec2 timelineSize = ImVec2(timelineWidth, 32);
			const ImVec2 topLeft = ImGui::GetCursorScreenPos();
			const ImVec2 btmRight = ImVec2(topLeft.x + timelineSize.x, topLeft.y + timelineSize.y);
			const r32 yHalf = (topLeft.y + btmRight.y) / 2.0f;

			ImGui::InvisibleButton("##canvas", timelineSize);

			const ImU32 color = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Border]);
			drawList->AddLine(ImVec2(topLeft.x, btmRight.y), ImVec2(btmRight.x, btmRight.y), color);

			const r32 frameWidth = timelineSize.x / anim.frameCount;
			for (u32 i = 0; i < anim.frameCount; i++) {
				const r32 x = i * frameWidth + topLeft.x;

				drawList->AddLine(ImVec2(x, topLeft.y), ImVec2(x, btmRight.y), color, 2.0f);

				for (u32 s = 0; s < anim.frameLength; s++) {
					const r32 xSmallNormalized = (r32)s / anim.frameLength;
					const r32 xSmall = xSmallNormalized * frameWidth + x;

					drawList->AddLine(ImVec2(xSmall, yHalf), ImVec2(xSmall, btmRight.y), color, 1.0f);
				}
			}

			const u32 totalTicks = anim.frameCount * anim.frameLength;
			const r32 xCurrentPosNormalized = glm::mod(framesElapsed, (r32)totalTicks) / totalTicks;
			const r32 xCurrentPos = xCurrentPosNormalized * timelineSize.x + topLeft.x;
			drawList->AddCircleFilled(ImVec2(xCurrentPos, btmRight.y), 8.0f, IM_COL32(255, 255, 0, 255));

			const r32 xLoopPointNormalized = r32(anim.loopPoint * anim.frameLength) / totalTicks;
			const r32 xLoopPoint = xLoopPointNormalized * timelineSize.x + topLeft.x;
			drawList->AddCircleFilled(ImVec2(xLoopPoint, btmRight.y), 8.0f, IM_COL32(128, 0, 255, 255));
		}
		ImGui::EndChild();

		if (ImGui::BeginTabBar("Actor editor tabs")) {
			if (ImGui::BeginTabItem("Behaviour")) {

				DrawTypeSelectionCombo("Type", Editor::actorTypeNames, ACTOR_TYPE_COUNT, pPrototype->type);
				const auto& editorData = Editor::actorEditorData[pPrototype->type];

				u32 subtypeCount = editorData.GetSubtypeCount();
				pPrototype->subtype = glm::clamp(pPrototype->subtype, u16(0), u16(subtypeCount - 1));
				DrawTypeSelectionCombo("Subtype", editorData.GetSubtypeNames(), subtypeCount, pPrototype->subtype);

				ImGui::SeparatorText("Type data");

				const char* prototypeNames[MAX_ACTOR_PROTOTYPE_COUNT]{};
				Assets::GetActorPrototypeNames(prototypeNames);

				u32 propCount = editorData.GetPropertyCount(pPrototype->subtype);
				for (u32 i = 0; i < propCount; i++) {
					DrawActorPrototypeProperty(editorData.GetProperty(pPrototype->subtype, i), pPrototype->data, prototypeNames);
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Animations")) {

				ImGui::BeginDisabled(pPrototype->animCount == ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT);
				if (ImGui::Button("+")) {
					PushElement<Animation>(pPrototype->animations, pPrototype->animCount);
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(pPrototype->animCount == 1);
				if (ImGui::Button("-")) {
					PopElement<Animation>(pPrototype->animations, pPrototype->animCount);
				}
				ImGui::EndDisabled();

				ImGui::BeginChild("Anim list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
				DrawGenericEditableList<Animation>(pPrototype->animations, pPrototype->animCount, ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT, selectedAnims, "Animation");

				ImGui::EndChild();

				ImGui::SameLine();

				ImGui::BeginChild("Animation editor");
				{
					ImGui::SeparatorText("Animation editor");
					if (selectedAnims.empty()) {
						ImGui::TextUnformatted("No animation selected");
					}
					else if (selectedAnims.size() > 1) {
						ImGui::TextUnformatted("Multiple animations selected");
					}
					else {
						currentAnim = selectedAnims[0];
						Animation& anim = pPrototype->animations[currentAnim];

						DrawTypeSelectionCombo("Animation type", ANIMATION_TYPE_NAMES, ANIMATION_TYPE_COUNT, anim.type);

						// TODO: Use DrawTypeSelectionCombo
						if (ImGui::BeginCombo("Metasprite", Metasprites::GetName(anim.metaspriteIndex))) {
							for (u32 i = 0; i < MAX_METASPRITE_COUNT; i++) {
								ImGui::PushID(i);

								const bool selected = anim.metaspriteIndex == i;
								if (ImGui::Selectable(Metasprites::GetName(i), selected)) {
									anim.metaspriteIndex = i;
								}

								if (selected) {
									ImGui::SetItemDefaultFocus();
								}
								ImGui::PopID();
							}
							ImGui::EndCombo();
						}

						const Metasprite* pMetasprite = Metasprites::GetMetasprite(anim.metaspriteIndex);
						static const s16 step = 1;
						ImGui::InputScalar("Frame count", ImGuiDataType_U16, &anim.frameCount, &step);
						const u16 maxFrameCount = anim.type == ANIMATION_TYPE_SPRITES ? pMetasprite->spriteCount : MAX_METASPRITE_COUNT - anim.metaspriteIndex;
						anim.frameCount = glm::clamp(anim.frameCount, u16(0), maxFrameCount);

						ImGui::InputScalar("Loop point", ImGuiDataType_S16, &anim.loopPoint, &step);
						anim.loopPoint = glm::clamp(anim.loopPoint, s16(-1), s16(anim.frameCount - 1));

						ImGui::InputScalar("Frame length", ImGuiDataType_U8, &anim.frameLength, &step);
					}
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Collision")) {
				ImGui::Checkbox("Show hitbox preview", &showHitboxPreview);

				ImGui::BeginChild("Collider editor");
				{
					ImGui::SeparatorText("Hitbox editor");

					AABB& hitbox = pPrototype->hitbox;
					glm::vec2 hitboxDim = (hitbox.max - hitbox.min);
					const glm::vec2 hitboxCenter = hitbox.min + hitboxDim / 2.0f;
					glm::vec2 newCenter = hitboxCenter;
					if (ImGui::InputFloat2("Offset", (r32*)&newCenter)) {
						hitboxDim.x = glm::max(0.0f, hitboxDim.x);
						hitbox.x1 = newCenter.x - hitboxDim.x / 2.0f;
						hitbox.x2 = newCenter.x + hitboxDim.x / 2.0f;

						hitboxDim.y = glm::max(0.0f, hitboxDim.y);
						hitbox.y1 = newCenter.y - hitboxDim.y / 2.0f;
						hitbox.y2 = newCenter.y + hitboxDim.y / 2.0f;
					}

					if (ImGui::InputFloat("Width", &hitboxDim.x, 0.125f, 0.0625f)) {
						hitboxDim.x = glm::max(0.0f, hitboxDim.x);
						hitbox.x1 = newCenter.x - hitboxDim.x / 2.0f;
						hitbox.x2 = newCenter.x + hitboxDim.x / 2.0f;
					}

					if (ImGui::InputFloat("Height", &hitboxDim.y, 0.125f, 0.0625f)) {
						hitboxDim.y = glm::max(0.0f, hitboxDim.y);
						hitbox.y1 = newCenter.y - hitboxDim.y / 2.0f;
						hitbox.y2 = newCenter.y + hitboxDim.y / 2.0f;
					}

				}
				ImGui::EndChild();

				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

	}
	ImGui::EndChild();

	ImGui::End();
}
#pragma endregion

#pragma region Audio
static void DrawPulseControls(bool idx) {
	ImGui::SeparatorText(idx == 0 ? "Pulse 1" : "Pulse 2");
	const u32 channel = idx == 0 ? CHAN_ID_PULSE0 : CHAN_ID_PULSE1;

	ImGui::PushID(idx);

	u8 bytes[4]{};
	Audio::ReadChannel(channel, bytes);

	int duty = bytes[0] >> 6;
	int volume = bytes[0] & 0b00001111;
	int period = bytes[2] | ((bytes[3] & 0x07) << 8);
	bool sweepEnabled = (bytes[1] >> 7);
	int sweepPeriod = (bytes[1] >> 4) & 0x07;
	int sweepAmount = (bytes[1] & 0x07);
	bool sweepNegate = (bytes[1] >> 3) & 1;
	bool useEnvelope = !((bytes[0] >> 4) & 1);
	bool loop = (bytes[0] >> 5) & 1;
	int lengthCounterLoad = (bytes[3] >> 3);

	if (ImGui::SliderInt("Duty cycle", &duty, 0, 3)) {
		u8 byte0 = (duty << 6) | (loop << 5) | (!useEnvelope << 4) | volume;
		Audio::WriteChannel(channel, 0, byte0);
	}

	if (ImGui::SliderInt("Volume", &volume, 0, 15)) {
		u8 byte0 = (duty << 6) | (loop << 5) | (!useEnvelope << 4) | volume;
		Audio::WriteChannel(channel, 0, byte0);
	}

	if (ImGui::SliderInt("Period", &period, 0, 0x7ff)) {
		u8 byte2 = period & 0xff;
		u8 byte3 = (lengthCounterLoad << 3) | (period >> 8);
		Audio::WriteChannel(channel, 2, byte2);
		Audio::WriteChannel(channel, 3, byte3);
	}

	if (ImGui::Checkbox("Enable sweep", &sweepEnabled)) {
		u8 byte1 = (sweepEnabled << 7) | (sweepPeriod << 4) | (sweepNegate << 3) | sweepAmount;
		Audio::WriteChannel(channel, 1, byte1);
	}
	if (ImGui::SliderInt("Sweep period", &sweepPeriod, 0, 7)) {
		u8 byte1 = (sweepEnabled << 7) | (sweepPeriod << 4) | (sweepNegate << 3) | sweepAmount;
		Audio::WriteChannel(channel, 1, byte1);
	}
	if (ImGui::SliderInt("Sweep amount", &sweepAmount, 0, 7)) {
		u8 byte1 = (sweepEnabled << 7) | (sweepPeriod << 4) | (sweepNegate << 3) | sweepAmount;
		Audio::WriteChannel(channel, 1, byte1);
	}
	if (ImGui::Checkbox("Negate sweep", &sweepNegate)) {
		u8 byte1 = (sweepEnabled << 7) | (sweepPeriod << 4) | (sweepNegate << 3) | sweepAmount;
		Audio::WriteChannel(channel, 1, byte1);
	}

	if (ImGui::Checkbox("Use envelope", &useEnvelope)) {
		u8 byte0 = (duty << 6) | (loop << 5) | (!useEnvelope << 4) | volume;
		Audio::WriteChannel(channel, 0, byte0);
	}

	if (ImGui::Checkbox("Loop", &loop)) {
		u8 byte0 = (duty << 6) | (loop << 5) | (!useEnvelope << 4) | volume;
		Audio::WriteChannel(channel, 0, byte0);
	}

	if (ImGui::SliderInt("Length counter load", &lengthCounterLoad, 0, 31)) {
		u8 byte3 = (lengthCounterLoad << 3) | (period >> 8);
		Audio::WriteChannel(channel, 3, byte3);
	}

	ImGui::PopID();
}

static void DrawTriangleControls() {
	ImGui::SeparatorText("Triangle");

	ImGui::PushID(2);

	u8 bytes[4]{};
	Audio::ReadChannel(CHAN_ID_TRIANGLE, bytes);

	int period = bytes[2] | ((bytes[3] & 0x07) << 8);
	int linearPeriod = bytes[0] & 0x7f;
	bool loop = bytes[0] >> 7;
	int lengthCounterLoad = (bytes[3] >> 3);

	if (ImGui::SliderInt("Period", &period, 0, 0x7ff)) {
		u8 byte2 = period & 0xff;
		u8 byte3 = (lengthCounterLoad << 3) | (period >> 8);
		Audio::WriteChannel(CHAN_ID_TRIANGLE, 2, byte2);
		Audio::WriteChannel(CHAN_ID_TRIANGLE, 3, byte3);
	}
	if (ImGui::SliderInt("Linear period", &linearPeriod, 0, 0x7f)) {
		u8 byte0 = (loop << 7) | linearPeriod;
		Audio::WriteChannel(CHAN_ID_TRIANGLE, 0, byte0);
	}

	if (ImGui::Checkbox("Loop", &loop)) {
		u8 byte0 = (loop << 7) | linearPeriod;
		Audio::WriteChannel(CHAN_ID_TRIANGLE, 0, byte0);
	}

	if (ImGui::SliderInt("Length counter load", &lengthCounterLoad, 0, 31)) {
		u8 byte3 = (lengthCounterLoad << 3) | (period >> 8);
		Audio::WriteChannel(CHAN_ID_TRIANGLE, 3, byte3);
	}

	ImGui::PopID();
}

static void DrawNoiseControls() {
	ImGui::SeparatorText("Noise");

	ImGui::PushID(3);

	u8 bytes[4]{};
	Audio::ReadChannel(CHAN_ID_NOISE, bytes);

	int volume = bytes[0] & 0b00001111;
	bool useEnvelope = !((bytes[0] >> 4) & 1);
	bool loop = (bytes[0] >> 5) & 1;
	int period = bytes[2] & 0x0f;
	bool mode = bytes[2] >> 7;
	int lengthCounterLoad = (bytes[3] >> 3);

	if (ImGui::Checkbox("Mode", &mode)) {
		u8 byte2 = (mode << 7) | period;
		Audio::WriteChannel(CHAN_ID_NOISE, 2, byte2);
	}

	if (ImGui::SliderInt("Volume", &volume, 0, 15)) {
		u8 byte0 = (loop << 5) | (!useEnvelope << 4) | volume;
		Audio::WriteChannel(CHAN_ID_NOISE, 0, byte0);
	}

	if (ImGui::SliderInt("Period", &period, 0, 0x0f)) {
		u8 byte2 = (mode << 7) | period;
		Audio::WriteChannel(CHAN_ID_NOISE, 2, byte2);
	}

	if (ImGui::Checkbox("Use envelope", &useEnvelope)) {
		u8 byte0 = (loop << 5) | (!useEnvelope << 4) | volume;
		Audio::WriteChannel(CHAN_ID_NOISE, 0, byte0);
	}

	if (ImGui::Checkbox("Loop", &loop)) {
		u8 byte0 = (loop << 5) | (!useEnvelope << 4) | volume;
		Audio::WriteChannel(CHAN_ID_NOISE, 0, byte0);
	}

	if (ImGui::SliderInt("Length counter load", &lengthCounterLoad, 0, 31)) {
		u8 byte3 = (lengthCounterLoad << 3);
		Audio::WriteChannel(CHAN_ID_NOISE, 3, byte3);
	}

	ImGui::PopID();
}

static void DrawAudioWindow() {
	ImGui::Begin("Audio", &pContext->audioWindowOpen);

	static u8 buffer[1024];
	Audio::ReadDebugBuffer(buffer, 1024);
	ImGui::PlotLines("Waveform", GetAudioSample, buffer, 1024, 0, nullptr, 0.0f, 255.0f, ImVec2(0, 80.0f));

	DrawPulseControls(0);
	DrawPulseControls(1);
	DrawTriangleControls();
	DrawNoiseControls();

	ImGui::End();
}
#pragma endregion

#pragma region Dungeon editor
struct EditorRoomInstance {
	u32 id;
	const RoomTemplate* pTemplate;

	glm::vec2 gridPos;
};

struct EditorDungeon {
	//char* name;
	std::unordered_map<u32, EditorRoomInstance> rooms;
};

static EditorDungeon ConvertFromDungeon(u32 dungeonIndex) {
	EditorDungeon outDungeon{};
	const Dungeon& dungeon = *Assets::GetDungeon(dungeonIndex);
	//outDungeon.name = dungeon.name;

	for (u32 i = 0; i < DUNGEON_GRID_SIZE; i++) {
		const DungeonCell& cell = dungeon.grid[i];

		if (cell.roomIndex >= 0 && cell.screenIndex == 0) {
			const RoomInstance& room = dungeon.rooms[cell.roomIndex];
			const glm::vec2 pos(i % DUNGEON_GRID_DIM, i / DUNGEON_GRID_DIM);

			outDungeon.rooms.emplace(room.id, EditorRoomInstance{
				.id = room.id,
				.pTemplate = Assets::GetRoomTemplate(room.templateIndex),
				.gridPos = pos
				});
		}
	}

	return outDungeon;
}

static void ConvertToDungeon(const EditorDungeon& dungeon, Dungeon* pOutDungeon) {
	pOutDungeon->roomCount = dungeon.rooms.size();

	s8 roomIndex = 0;
	for (auto& [id, room] : dungeon.rooms) {
		pOutDungeon->rooms[roomIndex] = RoomInstance{
			.id = id,
			.templateIndex = Assets::GetRoomTemplateIndex(room.pTemplate)
		};

		const u32 width = room.pTemplate->width;
		const u32 height = room.pTemplate->height;

		for (u32 y = 0; y < height; y++) {
			for (u32 x = 0; x < width; x++) {
				const u8 screenIndex = x + y * ROOM_MAX_DIM_SCREENS;
				const u32 gridIndex = (room.gridPos.x + x) + (room.gridPos.y + y) * DUNGEON_GRID_DIM;
				pOutDungeon->grid[gridIndex] = {
					.roomIndex = roomIndex,
					.screenIndex = screenIndex,
				};
			}
		}

		++roomIndex;
	}
}

static void DrawRoom(const EditorRoomInstance& room, const glm::mat3& gridToScreen, bool& outHovered) {
	outHovered = false;

	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Node body
	const r32 width = room.pTemplate->width;
	const r32 height = room.pTemplate->height;

	const glm::vec2 roomTopLeft = gridToScreen * glm::vec3(room.gridPos.x, room.gridPos.y, 1.0f);
	const glm::vec2 roomBtmRight = gridToScreen * glm::vec3(room.gridPos.x + width, room.gridPos.y + height, 1.0f);
	const r32 scale = glm::length(glm::vec2(gridToScreen[0][0], gridToScreen[1][0])) / VIEWPORT_WIDTH_METATILES;

	const ImVec2 nodeDrawMin = ImVec2(roomTopLeft.x, roomTopLeft.y);
	const ImVec2 nodeDrawMax = ImVec2(roomBtmRight.x, roomBtmRight.y);

	constexpr ImU32 outlineColor = IM_COL32(255, 255, 255, 255);
	constexpr ImU32 outlineHoveredColor = IM_COL32(255, 255, 0, 255);
	constexpr r32 outlineThickness = 1.0f;
	constexpr r32 outlineHoveredThickness = 2.0f;

	DrawTilemap(&room.pTemplate->tilemap, ImVec2(0,0), ImVec2(width * VIEWPORT_WIDTH_METATILES, height * VIEWPORT_HEIGHT_METATILES), nodeDrawMin, scale);

	drawList->AddRectFilled(nodeDrawMin, nodeDrawMax, IM_COL32(0, 0, 0, 0x80));

	drawList->AddText(ImVec2(nodeDrawMin.x + 10, nodeDrawMin.y + 10), IM_COL32(255, 255, 255, 255), room.pTemplate->name);

	ImU32 nodeOutlineColor = outlineColor;
	r32 nodeOutlineThickness = outlineThickness;
	// If node hovered
	if (io.MousePos.x >= nodeDrawMin.x &&
		io.MousePos.y >= nodeDrawMin.y &&
		io.MousePos.x < nodeDrawMax.x &&
		io.MousePos.y < nodeDrawMax.y) {
		nodeOutlineColor = outlineHoveredColor;
		nodeOutlineThickness = outlineHoveredThickness;
		outHovered = true;
	}

	drawList->AddRect(nodeDrawMin, nodeDrawMax, nodeOutlineColor, 0, 0, nodeOutlineThickness);
}

static bool DungeonPositionFree(u32 roomId, const glm::ivec2& pos, const glm::ivec2& dim, const EditorDungeon& dungeon) {
	const AABB targetRect(0, dim.x, 0, dim.y);
	
	for (auto& [otherId, otherRoom] : dungeon.rooms) {
		if (otherId == roomId) {
			continue;
		}

		const AABB rect(0, otherRoom.pTemplate->width, 0, otherRoom.pTemplate->height);

		if (Collision::BoxesOverlap(targetRect, pos, rect, otherRoom.gridPos)) {
			return false;
		}
	}

	return true;
}

static bool DrawDungeonDragDropPreview(u32 roomId, const glm::ivec2& pos, const glm::ivec2& dim, const glm::mat3& gridToScreen, const EditorDungeon& dungeon) {
	constexpr ImU32 freeColor = IM_COL32(0, 255, 0, 40);
	constexpr ImU32 occupiedColor = IM_COL32(255, 0, 0, 40);

	const bool posFree = DungeonPositionFree(roomId, pos, dim, dungeon);

	// Draw preview
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const glm::vec2 pMin = gridToScreen * glm::vec3(pos.x, pos.y, 1.0f);
	const glm::vec2 pMax = gridToScreen * glm::vec3(pos.x + dim.x, pos.y + dim.y, 1.0f);
	drawList->AddRectFilled(ImVec2(pMin.x, pMin.y), ImVec2(pMax.x, pMax.y), posFree ? freeColor : occupiedColor);

	return posFree;
}

static void DrawDungeonCanvas(EditorDungeon& dungeon) {
	// NOTE: ImGui examples -> custom rendering -> canvas
	ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
	ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
	if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
	if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
	ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(0x18, 0x18, 0x18, 255));
	drawList->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

	// Construct transformation matrices
	static glm::mat3 viewMat = glm::mat3(1.0f);
	const glm::mat3 canvasMat = glm::translate(glm::mat3(1.0f), glm::vec2(canvas_p0.x, canvas_p0.y));

	ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();

	// Canvas zooming
	if (hovered && io.MouseWheel != 0.0f) {
		const r32 scale = io.MouseWheel > 0.0f ? 1.1f : 0.9f;

		const glm::mat3 canvasToScreen = canvasMat * viewMat;
		const glm::mat3 screenToCanvas = glm::inverse(canvasToScreen);
		const glm::vec2 mouseInCanvas = screenToCanvas * glm::vec3(io.MousePos.x, io.MousePos.y, 1.0f);

		// Move to mouse pos
		viewMat = glm::translate(viewMat, mouseInCanvas);
		// Apply scale in mouse pos
		viewMat = glm::scale(viewMat, glm::vec2(scale, scale));
		// Move back to origin
		viewMat = glm::translate(viewMat, -mouseInCanvas);
	}

	const glm::mat3 canvasToScreen = canvasMat * viewMat;
	const glm::mat3 screenToCanvas = glm::inverse(canvasToScreen);

	// Canvas scrolling
	if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		glm::vec2 scaledDelta = screenToCanvas * glm::vec3(io.MouseDelta.x, io.MouseDelta.y, 0.0f);
		viewMat = glm::translate(viewMat, glm::vec2(scaledDelta.x, scaledDelta.y));
	}

	drawList->PushClipRect(canvas_p0, canvas_p1, true);

	// Draw grid
	const glm::mat3 gridToCanvas = glm::scale(glm::mat3(1.0f), glm::vec2(VIEWPORT_WIDTH_METATILES, VIEWPORT_HEIGHT_METATILES));
	const glm::mat3 canvasToGrid = glm::inverse(gridToCanvas);
	const glm::mat3 gridToScreen = canvasToScreen * gridToCanvas;
	const glm::mat3 screenToGrid = canvasToGrid * screenToCanvas;

	const glm::vec2 gridMin = gridToScreen * glm::vec3(0.0f, 0.0f, 1.0f);
	const glm::vec2 gridMax = gridToScreen * glm::vec3(DUNGEON_GRID_DIM, DUNGEON_GRID_DIM, 1.0f);
	drawList->AddRect(ImVec2(gridMin.x, gridMin.y), ImVec2(gridMax.x, gridMax.y), IM_COL32(255, 255, 255, 255));

	for (u32 x = 0; x < DUNGEON_GRID_DIM; x++) {
		const r32 xScreen = glm::mix(gridMin.x, gridMax.x, r32(x) / DUNGEON_GRID_DIM);
		drawList->AddLine(ImVec2(xScreen, gridMin.y), ImVec2(xScreen, gridMax.y), IM_COL32(200, 200, 200, 40));
	}

	for (u32 y = 0; y < DUNGEON_GRID_DIM; y++) {
		const r32 yScreen = glm::mix(gridMin.y, gridMax.y, r32(y) / DUNGEON_GRID_DIM);
		drawList->AddLine(ImVec2(gridMin.x, yScreen), ImVec2(gridMax.x, yScreen), IM_COL32(200, 200, 200, 40));
	}

	const glm::vec2 mousePosInGrid = screenToGrid * glm::vec3(io.MousePos.x, io.MousePos.y, 1.0f);
	const glm::ivec2 hoveredCellPos = glm::clamp(glm::ivec2(mousePosInGrid), glm::ivec2(0), glm::ivec2(DUNGEON_GRID_DIM - 1));

	static u32 draggedRoom = UUID_NULL;
	static glm::vec2 dragStartPos(0.0f);

	static u32 contextRoom = UUID_NULL;

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("dd_rooms", ImGuiDragDropFlags_AcceptBeforeDelivery))
		{
			s32 roomInd = *(const s32*)payload->Data;
			const RoomTemplate* pTemplate = Assets::GetRoomTemplate(roomInd);

			const glm::ivec2 roomTopLeft = hoveredCellPos;
			const glm::ivec2 roomDim = { pTemplate->width, pTemplate->height };

			const bool posFree = DrawDungeonDragDropPreview(UUID_NULL, roomTopLeft, roomDim, gridToScreen, dungeon);

			if (posFree && payload->IsDelivery()) {
				const u32 roomId = Random::GenerateUUID32();
				if (dungeon.rooms.size() < MAX_DUNGEON_ROOM_COUNT) {
					dungeon.rooms.emplace(roomId, EditorRoomInstance{
						.id = roomId,
						.pTemplate = pTemplate,
						.gridPos = roomTopLeft
						});
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (draggedRoom != UUID_NULL) {
		auto& room = dungeon.rooms[draggedRoom];
		const glm::ivec2 roomDim = { room.pTemplate->width, room.pTemplate->height };
		const glm::ivec2 dragTargetPos = glm::clamp(glm::ivec2(glm::roundEven(room.gridPos.x), glm::roundEven(room.gridPos.y)), glm::ivec2(0), glm::ivec2(DUNGEON_GRID_DIM - 1));
		const bool posFree = DrawDungeonDragDropPreview(room.id, dragTargetPos, roomDim, gridToScreen, dungeon);

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			glm::vec2 scaledDelta = screenToGrid * glm::vec3(io.MouseDelta.x, io.MouseDelta.y, 0.0f);

			room.gridPos += scaledDelta;
		}

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (posFree) {
				room.gridPos = dragTargetPos;
			}
			else {
				room.gridPos = dragStartPos;
			}

			draggedRoom = UUID_NULL;
		}
	}

	for (auto& [id, room] : dungeon.rooms) {
		bool nodeHovered;
		DrawRoom(room, gridToScreen, nodeHovered);

		if (active && nodeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			draggedRoom = id;
			dragStartPos = room.gridPos;
		}

		if (active && nodeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			contextRoom = id;
			ImGui::OpenPopup("NodeContextMenu");
		}
	}

	if (ImGui::BeginPopup("NodeContextMenu")) {
		if (ImGui::MenuItem("Delete Node") && contextRoom != UUID_NULL) {
			dungeon.rooms.erase(contextRoom);
			contextRoom = UUID_NULL;
		}
		ImGui::EndPopup();
	}

	drawList->PopClipRect();
}

static void DrawDungeonTools(EditorDungeon& dungeon) {
	//ImGui::SeparatorText(dungeon.name);

	//ImGui::InputText("Name", dungeon.name, DUNGEON_MAX_NAME_LENGTH);
	ImGui::Separator();

	ImGui::BeginChild("Room list", ImVec2(0, 0), ImGuiChildFlags_Border);
	{
		static s32 selection = 0;

		static constexpr u32 maxLabelNameLength = ROOM_MAX_NAME_LENGTH + 8;
		char label[maxLabelNameLength];

		for (u32 i = 0; i < MAX_ROOM_TEMPLATE_COUNT; i++) {
			const RoomTemplate& room = *Assets::GetRoomTemplate(i);

			ImGui::PushID(i);

			snprintf(label, maxLabelNameLength, "%#04x: %s", i, room.name);

			const bool selected = selection == i;
			if (ImGui::Selectable(label, selected)) {
				selection = i;
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				ImGui::SetDragDropPayload("dd_rooms", &i, sizeof(s32));
				ImGui::Text("%s", room.name);

				ImGui::EndDragDropSource();
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
}

static void DrawDungeonWindow() {
	ImGui::Begin("Dungeon editor", &pContext->dungeonWindowOpen, ImGuiWindowFlags_MenuBar);

	static u32 selectedDungeon = 0;
	static EditorDungeon editedDungeon = ConvertFromDungeon(selectedDungeon);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save")) {
				Dungeon* pEdited = Assets::GetDungeon(selectedDungeon);
				ConvertToDungeon(editedDungeon, pEdited);
				Assets::SaveDungeons("assets/test.dng");
			}
			if (ImGui::MenuItem("Revert changes")) {
				Assets::LoadDungeons("assets/test.dng");
				editedDungeon = ConvertFromDungeon(selectedDungeon);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::BeginChild("Dungeon list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	{
		static constexpr u32 maxLabelNameLength = DUNGEON_MAX_NAME_LENGTH + 8;
		char label[maxLabelNameLength];

		for (u32 i = 0; i < MAX_DUNGEON_COUNT; i++)
		{
			Dungeon* pDungeon = Assets::GetDungeon(i);
			ImGui::PushID(i);

			snprintf(label, maxLabelNameLength, "%#04x: %s", i, "Untitled");

			const bool selected = selectedDungeon == i;

			if (ImGui::Selectable(label, selected)) {
				Dungeon* pEdited = Assets::GetDungeon(selectedDungeon);
				ConvertToDungeon(editedDungeon, pEdited);
				selectedDungeon = i;
				editedDungeon = ConvertFromDungeon(selectedDungeon);
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}

			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	const r32 toolWindowWidth = 350.0f;
	ImGuiStyle& style = ImGui::GetStyle();
	const r32 canvasViewWidth = ImGui::GetContentRegionAvail().x - style.WindowPadding.x - toolWindowWidth;

	ImGui::BeginChild("Node graph", ImVec2(canvasViewWidth, 0));
	DrawDungeonCanvas(editedDungeon);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("Dungeon tools", ImVec2(toolWindowWidth, 0));
	DrawDungeonTools(editedDungeon);
	ImGui::EndChild();

	ImGui::End();
}
#pragma endregion

#pragma region Main Menu
static void DrawMainMenu() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Window"))
		{
			if (ImGui::MenuItem("Debug")) {
				pContext->debugWindowOpen = true;
			}
			if (ImGui::MenuItem("Metasprites")) {
				pContext->spriteWindowOpen = true;
			}
			if (ImGui::MenuItem("Tileset")) {
				pContext->tilesetWindowOpen = true;
			}
			if (ImGui::MenuItem("Room editor")) {
				pContext->roomWindowOpen = true;
			}
			if (ImGui::MenuItem("Dungeon editor")) {
				pContext->dungeonWindowOpen = true;
			}
			if (ImGui::MenuItem("Actor prototypes")) {
				pContext->actorWindowOpen = true;
			}
			if (ImGui::MenuItem("Audio")) {
				pContext->audioWindowOpen = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("ImGui Demo")) {
				pContext->demoWindowOpen = true;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}
#pragma endregion

#pragma region Public API
void Editor::CreateContext() {
	pContext = new EditorContext{};
	assert(pContext != nullptr);
}

void Editor::Init(SDL_Window *pWindow) {
	ImGui::CreateContext();
	Rendering::InitImGui(pWindow);

	Rendering::CreateImGuiChrTexture(&pContext->chrTexture);
	Rendering::CreateImGuiPaletteTexture(&pContext->paletteTexture);
	Rendering::CreateImGuiGameTexture(&pContext->gameViewTexture);
}

void Editor::Free() {
	Rendering::FreeImGuiChrTexture(&pContext->chrTexture);
	Rendering::FreeImGuiPaletteTexture(&pContext->paletteTexture);
	Rendering::FreeImGuiGameTexture(&pContext->gameViewTexture);

	Rendering::ShutdownImGui();
	ImGui::DestroyContext();
}

void Editor::DestroyContext() {
	delete pContext;
	pContext = nullptr;
}

void Editor::ConsoleLog(const char* fmt, va_list args) {
	if (pContext == nullptr) {
		return;
	}

	char s[1024];
	vsprintf_s(s, fmt, args);
	pContext->consoleLog.push_back(strdup(s));
}

void Editor::ClearLog() {
	for (char* msg : pContext->consoleLog) {
		free(msg);
	}
	pContext->consoleLog.clear();
}

void Editor::ProcessEvent(const SDL_Event* event) {
	ImGui_ImplSDL2_ProcessEvent(event);
}

void Editor::Render(r64 dt) {
	pContext->secondsElapsed += dt;

	Rendering::BeginImGuiFrame();
	ImGui::NewFrame();

	DrawMainMenu();

	if (pContext->demoWindowOpen) {
		ImGui::ShowDemoWindow(&pContext->demoWindowOpen);
	}

	if (pContext->debugWindowOpen) {
		DrawDebugWindow();
	}

	if (pContext->showDebugOverlay) {
		DrawDebugOverlay();
	}

	if (pContext->spriteWindowOpen) {
		DrawSpriteWindow();
	}

	if (pContext->tilesetWindowOpen) {
		DrawTilesetWindow();
	}

	if (pContext->roomWindowOpen) {
		DrawRoomWindow();
	}

	if (pContext->actorWindowOpen) {
		DrawActorWindow();
	}
;
	if (pContext->audioWindowOpen) {
		DrawAudioWindow();
	}

	if (pContext->dungeonWindowOpen) {
		DrawDungeonWindow();
	}

	ImGui::Render();
}
#pragma endregion