#include "editor.h"
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
#include "level.h"
#include "viewport.h"
#include "actors.h"
#include "audio.h"
#include "random.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/matrix_transform_2d.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <span>
#include <execution>

constexpr u32 CLIPBOARD_DIM_TILES = (VIEWPORT_WIDTH_TILES / 2) + 1;

struct LevelClipboard {
	u8 clipboard[CLIPBOARD_DIM_TILES * CLIPBOARD_DIM_TILES];
	ImVec2 size = { 1, 1 };
	ImVec2 offset = { 0, 0 };
};

struct LevelToolsState {
	bool propertiesOpen = true;
	bool tilemapOpen = true;
	bool actorsOpen = true;
};

enum LevelEditMode {
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

	// Editor state
	bool demoWindowOpen = false;
	bool debugWindowOpen = false;
	bool spriteWindowOpen = false;
	bool tilesetWindowOpen = false;
	bool gameWindowOpen = false;
	bool actorWindowOpen = false;
	bool audioWindowOpen = false;
	bool levelConnectionsOpen = false;
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

	drawList->AddImage(pContext->paletteTexture, topLeft, btmRight, ImVec2(0, 0), ImVec2(0.015625f, 1.0f));
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
static void GetMetatileVerticesAVX(const Metatile& metatile, s32 palette, const ImVec2& pos, r32 scale, ImVec2* outVertices, ImVec2* outUV) {
	constexpr r32 TILE_SIZE = 1.0f / METATILE_DIM_TILES;
	constexpr r32 INV_CHR_COUNT = 1.0f / CHR_COUNT;
	constexpr r32 INV_CHR_DIM_TILES = 1.0f / CHR_DIM_TILES;
	constexpr u32 CHR_DIM_TILES_BITS = 0xf;
	constexpr u32 CHR_DIM_TILES_LOG2 = 4;
	constexpr r32 INV_SHEET_PALETTE_COUNT = (1.0f / PALETTE_COUNT) * CHR_COUNT;

	const __m256 xMask = _mm256_setr_ps(0, 1, 0, 1, 1, 0, 1, 0);
	const __m256 yMask = _mm256_setr_ps(0, 0, 0, 0, 1, 1, 1, 1);

	const s32 i0 = metatile.tiles[0];
	const s32 i1 = metatile.tiles[1];
	const s32 i2 = metatile.tiles[2];
	const s32 i3 = metatile.tiles[3];

	const __m256i ti0 = _mm256_setr_epi32(i0, i0, i1, i1, i0, i0, i1, i1);
	const __m256i ti1 = _mm256_setr_epi32(i2, i2, i3, i3, i2, i2, i3, i3);
	const __m256 ci = _mm256_set1_ps(INV_CHR_COUNT);
	const __m256 cit = _mm256_set1_ps(INV_CHR_DIM_TILES);
	const __m256i cb = _mm256_set1_epi32(CHR_DIM_TILES_BITS);

	const __m256 p = _mm256_set1_ps(r32(palette));
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
	__m256 u0 = _mm256_cvtepi32_ps(tix0);
	u0 = _mm256_add_ps(u0, xMask);
	u0 = _mm256_mul_ps(u0, cit);

	u0 = _mm256_add_ps(u0, p);
	u0 = _mm256_mul_ps(u0, pi);

	__m256 v0 = _mm256_cvtepi32_ps(tiy0);
	v0 = _mm256_add_ps(v0, yMask);
	v0 = _mm256_mul_ps(v0, cit);

	v0 = _mm256_mul_ps(v0, ci);

	__m256 y1 = _mm256_setr_ps(1, 1, 1, 1, 2, 2, 2, 2);
	y1 = _mm256_mul_ps(y1, s);
	y1 = _mm256_add_ps(y1, ty);

	const __m256i tix1 = _mm256_and_si256(ti1, cb);
	const __m256i tiy1 = _mm256_srli_epi32(ti1, CHR_DIM_TILES_LOG2);
	__m256 u1 = _mm256_cvtepi32_ps(tix1);
	u1 = _mm256_add_ps(u1, xMask);
	u1 = _mm256_mul_ps(u1, cit);

	u1 = _mm256_add_ps(u1, p);
	u1 = _mm256_mul_ps(u1, pi);

	__m256 v1 = _mm256_cvtepi32_ps(tiy1);
	v1 = _mm256_add_ps(v1, yMask);
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

static void GetMetatileVertices(const Metatile& metatile, s32 palette, const ImVec2& pos, r32 scale, ImVec2* outVertices, ImVec2* outUV) {
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

		const glm::vec4 uvMinMax = ChrTileToTexCoord(metatile.tiles[i], 0, palette);

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

static void DrawMetatile(const Metatile& metatile, ImVec2 pos, r32 size, s32 palette, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	const r32 tileSize = size / METATILE_DIM_TILES;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushTextureID(pContext->chrTexture);
	drawList->PrimReserve(METATILE_TILE_COUNT * 6, METATILE_TILE_COUNT * 4);

	ImVec2 verts[METATILE_TILE_COUNT * 4];
	ImVec2 uv[METATILE_TILE_COUNT * 4];

	GetMetatileVerticesAVX(metatile, palette, pos, size, verts, uv);
	WriteMetatile(verts, uv, color);

	drawList->PopTextureID();
}

static void DrawNametable(ImVec2 size, const Nametable& nametable) {
	const r32 gridStep = size.x / NAMETABLE_WIDTH_TILES;
	const ImVec2 tablePos = DrawTileGrid(size, gridStep);

	const r32 scale = size.x / NAMETABLE_WIDTH_PIXELS;
	const r32 metatileDrawSize = METATILE_DIM_PIXELS * scale;

	Metatile metatile;
	s32 palette;
	for (u32 i = 0; i < NAMETABLE_SIZE_METATILES; i++) {
		u32 x = i % NAMETABLE_WIDTH_METATILES;
		u32 y = i / NAMETABLE_WIDTH_METATILES;

		ImVec2 pos = ImVec2(tablePos.x + (x * metatileDrawSize), tablePos.y + (y * metatileDrawSize));

		Rendering::Util::GetNametableMetatile(&nametable, i, metatile, palette);
		DrawMetatile(metatile, pos, metatileDrawSize, palette);
	}
}

static bool DrawPaletteButton(u8 palette) {
	return ImGui::ImageButton("", pContext->paletteTexture, ImVec2(80, 10), ImVec2(0.125 * palette, 0), ImVec2(0.125 * (palette + 1), 1));
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

static void DrawLevel(const Level* pLevel, ImVec2 pos, r32 scale) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const Tilemap* pTilemap = pLevel->pTilemap;
	const Tileset* pTileset = pTilemap->pTileset;

	const u32 screenCount = pTilemap->width * pTilemap->height;
	const u32 tileCount = screenCount * VIEWPORT_SIZE_TILES;

	drawList->PushTextureID(pContext->chrTexture);
	drawList->PrimReserve(tileCount * 6, tileCount * 4);

	ImVec2 verts[METATILE_TILE_COUNT * 4];
	ImVec2 uv[METATILE_TILE_COUNT * 4];

	struct ScreenDrawData {
		ImVec2 verts[VIEWPORT_SIZE_METATILES * METATILE_TILE_COUNT * 4];
		ImVec2 uv[VIEWPORT_SIZE_METATILES * METATILE_TILE_COUNT * 4];
	};
	static ScreenDrawData screenData[TILEMAP_MAX_SCREEN_COUNT];

	std::span<const TilemapScreen> screenSpan(pTilemap->screens, TILEMAP_MAX_SCREEN_COUNT);
	std::for_each(std::execution::par, screenSpan.begin(), screenSpan.end(), [&](const TilemapScreen& screen) {
		const u32 screenIndex = &screen - pTilemap->screens;
		const u32 xScreen = screenIndex % TILEMAP_MAX_DIM_SCREENS;
		const u32 yScreen = screenIndex / TILEMAP_MAX_DIM_SCREENS;

		if (xScreen >= pTilemap->width || yScreen >= pTilemap->height) {
			return;
		}

		const u32 outIndex = xScreen + pTilemap->width * yScreen;
		ScreenDrawData& data = screenData[outIndex];
		for (u32 i = 0; i < VIEWPORT_SIZE_METATILES; i++) {
			const u8 tilesetTileIndex = screen.tiles[i];
			const TilesetTile& tilesetTile = pTileset->tiles[tilesetTileIndex];
			const Metatile& metatile = tilesetTile.metatile;
			const s32 palette = Tiles::GetTilesetPalette(pTileset, tilesetTileIndex);

			const u32 xMetatile = xScreen * VIEWPORT_WIDTH_METATILES + i % VIEWPORT_WIDTH_METATILES;
			const u32 yMetatile = yScreen * VIEWPORT_HEIGHT_METATILES + i / VIEWPORT_WIDTH_METATILES;
			const ImVec2 metatileOffset = ImVec2(pos.x + xMetatile * scale, pos.y + yMetatile * scale);

			ImVec2* verts = data.verts + (METATILE_TILE_COUNT * 4) * i;
			ImVec2* uv = data.uv + (METATILE_TILE_COUNT * 4) * i;

			GetMetatileVerticesAVX(metatile, palette, metatileOffset, scale, verts, uv);
		}

	});

	for (u32 s = 0; s < screenCount; s++) {
		const auto& data = screenData[s];
		for (u32 i = 0; i < VIEWPORT_SIZE_METATILES; i++) {
			const ImVec2* verts = data.verts + (METATILE_TILE_COUNT * 4) * i;
			const ImVec2* uv = data.uv + (METATILE_TILE_COUNT * 4) * i;
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
		const s32 palette = Tiles::GetTilesetPalette(pTileset, i);
		GetMetatileVerticesAVX(metatile, palette, metatileOffset, renderScale * TILESET_DIM, verts, uv);
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
		const ImVec2 pos = ImVec2(origin.x + renderScale * Rendering::Util::SignExtendSpritePos(sprite.x), origin.y + renderScale * Rendering::Util::SignExtendSpritePos(sprite.y));
		DrawSprite(sprite, pos, renderScale, color);
	}
}

static AABB GetActorBoundingBox(const Actor* pActor) {
	AABB result{};
	if (pActor == nullptr) {
		return result;
	}

	constexpr r32 tileWorldDim = 1.0f / METATILE_DIM_TILES;
	
	// TODO: What if animation changes bounds?
	const Animation& anim = pActor->pPrototype->animations[0];
	switch (anim.type) {
	case ANIMATION_TYPE_SPRITES: {
		const Metasprite* pMetasprite = Metasprites::GetMetasprite(anim.metaspriteIndex);
		Sprite& sprite = pMetasprite->spritesRelativePos[0];
		result.min = { (r32)Rendering::Util::SignExtendSpritePos(sprite.x) / METATILE_DIM_PIXELS, (r32)Rendering::Util::SignExtendSpritePos(sprite.y) / METATILE_DIM_PIXELS };
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
			const glm::vec2 spriteMin = { (r32)Rendering::Util::SignExtendSpritePos(sprite.x) / METATILE_DIM_PIXELS, (r32)Rendering::Util::SignExtendSpritePos(sprite.y) / METATILE_DIM_PIXELS };
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
		ImVec2 pos = ImVec2(origin.x + renderScale * Rendering::Util::SignExtendSpritePos(sprite.x), origin.y + renderScale * Rendering::Util::SignExtendSpritePos(sprite.y));
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

static void swap(Level& a, Level& b) {
	Tilemap* levelATilemap = a.pTilemap;
	char* levelAName = a.name;
	Tilemap* levelBTilemap = b.pTilemap;
	char* levelBName = b.name;

	// Copy A to temp
	Level temp = a;

	Tilemap* tempTilemap = new Tilemap{};
	char tempName[LEVEL_MAX_NAME_LENGTH];

	memcpy(tempTilemap, levelATilemap, sizeof(Tilemap));
	memcpy(tempName, levelAName, LEVEL_MAX_NAME_LENGTH);

	// Copy B to A (But keep pointers pointing in original location)
	a = b;
	a.pTilemap = levelATilemap;
	a.name = levelAName;
	memcpy(levelATilemap, levelBTilemap, sizeof(Tilemap));
	memcpy(levelAName, levelBName, LEVEL_MAX_NAME_LENGTH);

	// Copy Temp to B
	b = temp;
	b.pTilemap = levelBTilemap;
	b.name = levelBName;
	memcpy(levelBTilemap, tempTilemap, sizeof(Tilemap));
	memcpy(levelBName, tempName, LEVEL_MAX_NAME_LENGTH);

	delete tempTilemap;
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

	const char* selectedLabel = typeCount > 0 ? typeNames[selection] : "NO ITEMS";

	if (ImGui::BeginCombo(label, selectedLabel)) {
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
static void DrawDebugWindow() {
	ImGui::Begin("Debug", &pContext->debugWindowOpen);

	if (ImGui::BeginTabBar("Debug tabs")) {
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
			const ImVec2 nametableSizePx = ImVec2(NAMETABLE_WIDTH_PIXELS, NAMETABLE_HEIGHT_PIXELS);

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
				ImGui::PushID(i);
				if (DrawPaletteButton(i)) {
					if (i < 4) {
						selectedPalettes[0] = i;
					}
					else selectedPalettes[1] = i - 4;
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
		ImVec2 pos = ImVec2(origin.x + renderScale * Rendering::Util::SignExtendSpritePos(sprite.x), origin.y + renderScale * Rendering::Util::SignExtendSpritePos(sprite.y));

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
			for (int i = 0; i < 4; i++) {
				ImGui::PushID(i);
				if (DrawPaletteButton(i+4)) {
					sprite.palette = i;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		ImGui::Text("Position: (%d, %d)", Rendering::Util::SignExtendSpritePos(sprite.x), Rendering::Util::SignExtendSpritePos(sprite.y));

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
		s32 palette = Tiles::GetTilesetPalette(pTileset, selectedMetatileIndex);
		s32 tileId = metatile.tiles[selectedTileIndex];

		r32 chrSheetSize = 256;
		ImGui::PushID(0);
		DrawCHRSheet(chrSheetSize, 0, palette, &tileId);
		ImGui::PopID();
		if (tileId != metatile.tiles[selectedTileIndex]) {
			metatile.tiles[selectedTileIndex] = tileId;
		}

		ImGui::Text("0x%02x", selectedMetatileIndex);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 gridSize = ImVec2(tilePreviewSize, tilePreviewSize);
		ImGui::PushID(1);
		const ImVec2 tilePos = DrawTileGrid(gridSize, gridStepPixels, &selectedTileIndex);
		ImGui::PopID();
		DrawMetatile(metatile, tilePos, tilePreviewSize, palette);
		DrawTileGridSelection(tilePos, gridSize, gridStepPixels, selectedTileIndex);

		s32& type = pTileset->tiles[selectedMetatileIndex].type;
		ImGui::SliderInt("Type", &type, 0, TILE_TYPE_COUNT - 1, METATILE_TYPE_NAMES[type]);

		if (ImGui::SliderInt("Palette", &palette, 0, 3)) {
			Tiles::SetTilesetPalette(pTileset, selectedMetatileIndex, palette);
		}
	}
	ImGui::EndChild();

	ImGui::End();
}
#pragma endregion

#pragma region Level editor
static void DrawScreenBorders(u32 index, ImVec2 pMin, ImVec2 pMax, r32 renderScale) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	
	static char screenLabelText[16];
	snprintf(screenLabelText, 16, "%#04x", index);

	drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255));

	const ImVec2 textPos = ImVec2(pMin.x + TILE_DIM_PIXELS * renderScale, pMin.y + TILE_DIM_PIXELS * renderScale);
	drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), screenLabelText);
}

static void DrawScreenCollisionCells(ImVec2 pMin, ImVec2 pMax, ImVec2 viewportDrawSize) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	for (s32 y = 0; y < 4; y++) {
		r32 yPos = pMin.y + (viewportDrawSize.y / 4) * y;
		for (s32 x = 0; x < 4; x++) {
			r32 xPos = pMin.x + (viewportDrawSize.x / 4) * x;

			ImVec2 pMin2 = ImVec2(xPos, yPos);
			ImVec2 pMax2 = ImVec2(pMin2.x + viewportDrawSize.x / 4, pMin2.y + viewportDrawSize.y / 4);

			drawList->AddRectFilled(pMin2, pMax2, IM_COL32(255, 0, 0, 63));
			drawList->AddRect(pMin2, pMax2, IM_COL32(255, 255, 255, 63));
		}
	}
}

static void DrawActorColliders(const Viewport* pViewport, const ImVec2 topLeft, const r32 renderScale) {
	const DynamicActorPool* actors = Game::GetActors();

	const glm::vec2 viewportPixelPos = { pViewport->x * METATILE_DIM_PIXELS, pViewport->y * METATILE_DIM_PIXELS };
	for (u32 i = 0; i < actors->Count(); i++)
	{
		PoolHandle<Actor> handle = actors->GetHandle(i);
		const Actor* pActor = actors->Get(handle);

		const glm::vec2 actorPixelPos = pActor->position * (r32)METATILE_DIM_PIXELS;
		const glm::vec2 pixelOffset = actorPixelPos - viewportPixelPos;
		const ImVec2 drawPos = ImVec2(topLeft.x + pixelOffset.x * renderScale, topLeft.y + pixelOffset.y * renderScale);

		DrawHitbox(&pActor->pPrototype->hitbox, drawPos, renderScale);
	}
}

static void DrawGameViewOverlay(const Level* pLevel, const Viewport* pViewport, const ImVec2 topLeft, const ImVec2 btmRight, const r32 renderScale, bool drawBorders, bool drawCollisionCells, bool drawHitboxes) {
	const glm::vec2 viewportPixelPos = { pViewport->x * METATILE_DIM_PIXELS, pViewport->y * METATILE_DIM_PIXELS };
	const ImVec2 viewportDrawSize = ImVec2(VIEWPORT_WIDTH_PIXELS * renderScale, VIEWPORT_HEIGHT_PIXELS * renderScale);

	const s32 screenStartX = pViewport->x / VIEWPORT_WIDTH_METATILES;
	const s32 screenStartY = pViewport->y / VIEWPORT_HEIGHT_METATILES;

	const s32 screenEndX = (pViewport->x + VIEWPORT_WIDTH_METATILES) / VIEWPORT_WIDTH_METATILES;
	const s32 screenEndY = (pViewport->y + VIEWPORT_HEIGHT_METATILES) / VIEWPORT_HEIGHT_METATILES;

	const Tilemap* pTilemap = pLevel->pTilemap;

	for (s32 y = screenStartY; y <= screenEndY; y++) {
		for (s32 x = screenStartX; x <= screenEndX; x++) {
			const glm::vec2 screenPixelPos = { x * VIEWPORT_WIDTH_PIXELS, y * VIEWPORT_HEIGHT_PIXELS };
			const ImVec2 pMin = ImVec2((screenPixelPos.x - viewportPixelPos.x) * renderScale + topLeft.x, (screenPixelPos.y - viewportPixelPos.y) * renderScale + topLeft.y);
			const ImVec2 pMax = ImVec2(pMin.x + viewportDrawSize.x, pMin.y + viewportDrawSize.y);

			const s32 i = x + y * TILEMAP_MAX_DIM_SCREENS;

			if (drawBorders) {
				DrawScreenBorders(i, pMin, pMax, renderScale);
			}

			if (drawCollisionCells) {
				DrawScreenCollisionCells(pMin, pMax, viewportDrawSize);
			}

			if (drawHitboxes) {
				DrawActorColliders(pViewport, topLeft, renderScale);
			}
		}
	}
}

static void DrawGameView(Level* pLevel, bool editing, u32 editMode, LevelClipboard& clipboard, u32& selectedLevel, PoolHandle<Actor>& selectedActorHandle) {
	Viewport* pViewport = Game::GetViewport();
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
	const ImVec2 mousePosInWorldCoords = ImVec2(mousePosInViewportCoords.x + pViewport->x, mousePosInViewportCoords.y + pViewport->y);

	// Invisible button to prevent dragging window
	ImGui::InvisibleButton("##canvas", ImVec2(contentWidth, contentHeight), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

	// Context menu handling
	if (editing && editMode == EDIT_MODE_ACTORS) {
		if (ImGui::BeginPopupContextItem("actor_popup"))
		{
			if (ImGui::Selectable("Add actor")) {
				PoolHandle<Actor> handle = pLevel->actors.Add();
				Actor* pNewActor = pLevel->actors.Get(handle);
				pNewActor->pPrototype = Actors::GetPrototype(0);
				pNewActor->id = Random::GenerateUUID();
				pNewActor->position = { mousePosInWorldCoords.x, mousePosInWorldCoords.y };
			}
			ImGui::EndPopup();
		}
	}

	const bool hovered = ImGui::IsItemHovered(); // Hovered
	const bool active = ImGui::IsItemActive();   // Held

	drawList->PushClipRect(topLeft, btmRight, true);

	drawList->AddImage(pContext->gameViewTexture, topLeft, btmRight);

	static bool drawCollisionCells = false;
	static bool drawHitboxes = false;

	DrawGameViewOverlay(pLevel, pViewport, topLeft, btmRight, renderScale, editing, drawCollisionCells, drawHitboxes);

	if (editing) {
		// Draw actors
		const glm::vec2 viewportPixelPos = { pViewport->x * METATILE_DIM_PIXELS, pViewport->y * METATILE_DIM_PIXELS };
		for (u32 i = 0; i < pLevel->actors.Count(); i++)
		{
			PoolHandle<Actor> handle = pLevel->actors.GetHandle(i);
			const Actor* pActor = pLevel->actors.Get(handle);

			const glm::vec2 actorPixelPos = pActor->position * (r32)METATILE_DIM_PIXELS;
			const glm::vec2 pixelOffset = actorPixelPos - viewportPixelPos;
			const ImVec2 drawPos = ImVec2(topLeft.x + pixelOffset.x * renderScale, topLeft.y + pixelOffset.y * renderScale);

			const u8 opacity = editMode == EDIT_MODE_ACTORS ? 255 : 80;
			DrawActor(pActor->pPrototype, drawPos, renderScale, 0, 0, IM_COL32(255, 255, 255, opacity));
		}

		// View scrolling
		static ImVec2 dragStartPos = ImVec2(0, 0);
		static ImVec2 dragDelta = ImVec2(0, 0);
		bool scrolling = false;

		if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
			dragStartPos = io.MousePos;
			selectedActorHandle = PoolHandle<Actor>::Null();
		}

		if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
			const ImVec2 newDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
			const r32 dx = -(newDelta.x - dragDelta.x) / renderScale / METATILE_DIM_PIXELS;
			const r32 dy = -(newDelta.y - dragDelta.y) / renderScale / METATILE_DIM_PIXELS;
			dragDelta = newDelta;

			MoveViewport(pViewport, pNametables, pLevel->pTilemap, dx, dy);
			scrolling = true;
		}

		// Reset drag delta when mouse released
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
			dragDelta = ImVec2(0, 0);
		}

		const ImVec2 hoveredTileWorldPos = ImVec2(glm::floor(mousePosInWorldCoords.x), glm::floor(mousePosInWorldCoords.y));

		switch (editMode) {
		case EDIT_MODE_ACTORS:
		{
			Actor* pActor = pLevel->actors.Get(selectedActorHandle);
			const AABB actorBounds = GetActorBoundingBox(pActor);

			// Selection
			if (!scrolling) {
				static glm::vec2 selectionStartPos{};

				if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					selectedActorHandle = PoolHandle<Actor>::Null();
					// TODO: Some quadtree action needed desperately
					for (u32 i = 0; i < pLevel->actors.Count(); i++) {
						PoolHandle<Actor> handle = pLevel->actors.GetHandle(i);
						const Actor* pActor = pLevel->actors.Get(handle);

						const AABB bounds = GetActorBoundingBox(pActor);
						if (Collision::PointInsideBox({ mousePosInWorldCoords.x, mousePosInWorldCoords.y }, bounds, pActor->position)) {
							selectedActorHandle = handle;
							selectionStartPos = { mousePosInWorldCoords.x, mousePosInWorldCoords.y };
							break;
						}
					}
				}

				if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && pActor != nullptr) {
					const ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
					const glm::vec2 deltaInWorldCoords = { dragDelta.x / tileDrawSize, dragDelta.y / tileDrawSize };

					pActor->position = selectionStartPos + deltaInWorldCoords;
				}
			}

			if (pActor != nullptr) {
				const AABB boundsAbs(actorBounds.min + pActor->position, actorBounds.max + pActor->position);
				const ImVec2 pMin = ImVec2((boundsAbs.min.x - pViewport->x) * tileDrawSize + topLeft.x, (boundsAbs.min.y - pViewport->y) * tileDrawSize + topLeft.y);
				const ImVec2 pMax = ImVec2((boundsAbs.max.x - pViewport->x) * tileDrawSize + topLeft.x, (boundsAbs.max.y - pViewport->y) * tileDrawSize + topLeft.y);

				drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255));
			}

			break;
		}
		case EDIT_MODE_TILES:
		{
			Tilemap* pTilemap = pLevel->pTilemap;
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

					const ImVec2 selectionTopLeftInPixelCoords = ImVec2((selectionTopLeft.x - pViewport->x) * tileDrawSize + topLeft.x, (selectionTopLeft.y - pViewport->y) * tileDrawSize + topLeft.y);
					const ImVec2 selectionBtmRightInPixelCoords = ImVec2((selectionBtmRight.x - pViewport->x) * tileDrawSize + topLeft.x, (selectionBtmRight.y - pViewport->y) * tileDrawSize + topLeft.y);

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
							const s32 tilesetIndex = Tiles::GetTilesetIndex(pTilemap, metatileWorldPos);
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
					const ImVec2 metatileInViewportCoords = ImVec2(metatileWorldPos.x - pViewport->x, metatileWorldPos.y - pViewport->y);
					const ImVec2 metatileInPixelCoords = ImVec2(metatileInViewportCoords.x * tileDrawSize + topLeft.x, metatileInViewportCoords.y * tileDrawSize + topLeft.y);
					const u8 metatileIndex = clipboard.clipboard[clipboardIndex];
							
					const Metatile& metatile = pTilemap->pTileset->tiles[metatileIndex].metatile;
					const s32 palette = Tiles::GetTilesetPalette(pTilemap->pTileset, metatileIndex);
					DrawMetatile(metatile, metatileInPixelCoords, tileDrawSize, palette, IM_COL32(255, 255, 255, 127));

					// Paint metatiles
					if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && active) {
						Tiles::SetTilesetTile(pTilemap, metatileWorldPos, metatileIndex);

						const u32 nametableIndex = Tiles::GetNametableIndex(metatileWorldPos);
						const glm::ivec2 nametablePos = Tiles::GetNametableOffset(metatileWorldPos);
						Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametablePos.x, nametablePos.y, metatile, palette);
					}
				}
			}

			const ImVec2 clipboardTopLeftInPixelCoords = ImVec2((clipboardTopLeft.x - pViewport->x) * tileDrawSize + topLeft.x, (clipboardTopLeft.y - pViewport->y) * tileDrawSize + topLeft.y);
			const ImVec2 clipboardBtmRightInPixelCoords = ImVec2((clipboardBtmRight.x - pViewport->x) * tileDrawSize + topLeft.x, (clipboardBtmRight.y - pViewport->y) * tileDrawSize + topLeft.y);
			drawList->AddRect(clipboardTopLeftInPixelCoords, clipboardBtmRightInPixelCoords, IM_COL32(255, 255, 255, 255));
			break;
		}
		default:
			break;
		}
	}
	drawList->PopClipRect();

	ImGui::BeginDisabled(pLevel == nullptr);
	if (ImGui::Button(editing ? "Play mode" : "Edit mode")) {
		if (!editing) {
			// This is a little bit cursed
			u32 loadedLevelIndex = pLevel - Levels::GetLevelsPtr();
			selectedLevel = loadedLevelIndex;

			Game::UnloadLevel();
		}
		else {
			Game::ReloadLevel();
		}

		Game::SetPaused(!editing);
	}
	ImGui::EndDisabled();
	if (editing) {
		ImGui::SameLine();
		if (ImGui::Button("Refresh viewport")) {
			RefreshViewport(pViewport, pNametables, pLevel->pTilemap);
		}
	}

	ImGui::Text("Currently loaded level: %s", pLevel->name);
	ImGui::Text("Viewport pos = (%f, %f)", pViewport->x, pViewport->y);

	ImGui::SeparatorText("Debug");
	ImGui::Checkbox("Draw collision cells", &drawCollisionCells);
	ImGui::Checkbox("Draw actor hitboxes", &drawHitboxes);
}

static void DrawLevelTools(u32& selectedLevel, bool editing, u32& editMode, LevelToolsState& state, LevelClipboard& clipboard, PoolHandle<Actor>& selectedActorHandle) {
	const ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs;
	
	Level* pLevels = Levels::GetLevelsPtr();
	Level& level = pLevels[selectedLevel];

	if (ImGui::BeginTabBar("Level tool tabs")) {
		if (state.propertiesOpen && ImGui::BeginTabItem("Properties", &state.propertiesOpen)) {
			editMode = EDIT_MODE_NONE;

			ImGui::SeparatorText(level.name);

			ImGui::InputText("Name", level.name, LEVEL_MAX_NAME_LENGTH);

			u8 type = level.flags.type % LEVEL_TYPE_COUNT;
			DrawTypeSelectionCombo("Type", LEVEL_TYPE_NAMES, LEVEL_TYPE_COUNT, type);
			level.flags.type = type;

			s32 size[2] = { level.pTilemap->width, level.pTilemap->height };
			if (ImGui::InputInt2("Size", size)) {
				level.pTilemap->width = glm::clamp(size[0], 1, s32(TILEMAP_MAX_DIM_SCREENS));
				level.pTilemap->height = glm::clamp(size[1], 1, s32(TILEMAP_MAX_DIM_SCREENS));
			}

			// Screens
			/*if (ImGui::TreeNode("Screens")) {
				// TODO: Lay these out nicer
				u32 screenCount = level.width * level.height;
				for (u32 i = 0; i < screenCount; i++) {
					TilemapScreen& screen = level.screens[i];

					if (ImGui::TreeNode(&screen, "%#04x", i)) {

						const Level& exitTargetLevel = pLevels[screen.exitTargetLevel];

						if (ImGui::BeginCombo("Exit target level", exitTargetLevel.name)) {
							for (u32 i = 0; i < maxLevelCount; i++)
							{
								ImGui::PushID(i);
								const bool selected = screen.exitTargetLevel == i;
								if (ImGui::Selectable(pLevels[i].name, selected)) {
									screen.exitTargetLevel = i;
								}

								if (selected) {
									ImGui::SetItemDefaultFocus();
								}
								ImGui::PopID();
							}
							ImGui::EndCombo();
						}

						s32 exitScreen = screen.exitTargetScreen;
						if (ImGui::InputInt("Exit target screen", &exitScreen)) {
							screen.exitTargetScreen = (u32)glm::max(glm::Fmin((s32)levelMaxScreenCount - 1, exitScreen), 0);
						}

						ImGui::TreePop();
					}
				}

				ImGui::TreePop();
			}*/

			ImGui::EndTabItem();
		}

		if (state.tilemapOpen && ImGui::BeginTabItem("Tilemap", &state.tilemapOpen)) {
			editMode = EDIT_MODE_TILES;
			ImGuiStyle& style = ImGui::GetStyle();

			const s32 currentSelection = (clipboard.size.x == 1 && clipboard.size.y == 1) ? clipboard.clipboard[0] : -1;
			s32 newSelection = currentSelection;

			DrawTileset(level.pTilemap->pTileset, ImGui::GetContentRegionAvail().x - style.WindowPadding.x, &newSelection);

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

			Actor* pActor = level.actors.Get(selectedActorHandle);
			if (pActor == nullptr) {
				ImGui::Text("No actor selected");
			}
			else {
				ImGui::PushID(selectedActorHandle.Raw());
				ImGui::BeginDisabled(!editing);

				ImGui::Text("UUID: %llu", pActor->id);

				if (ImGui::BeginCombo("Prototype", Actors::GetPrototypeName(pActor->pPrototype))) {
					for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
						ImGui::PushID(i);

						const ActorPrototype* pPrototype = Actors::GetPrototype(i);
						const bool selected = pActor->pPrototype == pPrototype;

						if (ImGui::Selectable(Actors::GetPrototypeName(pPrototype), selected)) {
							pActor->pPrototype = pPrototype;
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

				/*if (ImGui::TreeNode("Screens")) {
					// TODO: Lay these out nicer
					u32 screenCount = level.width * level.height;
					for (u32 i = 0; i < screenCount; i++) {
						TilemapScreen& screen = level.screens[i];

						if (ImGui::TreeNode(&screen, "%#04x", i)) {

							const Level& exitTargetLevel = pLevels[screen.exitTargetLevel];

							if (ImGui::BeginCombo("Exit target level", exitTargetLevel.name)) {
								for (u32 i = 0; i < maxLevelCount; i++)
								{
									ImGui::PushID(i);
									const bool selected = screen.exitTargetLevel == i;
									if (ImGui::Selectable(pLevels[i].name, selected)) {
										screen.exitTargetLevel = i;
									}

									if (selected) {
										ImGui::SetItemDefaultFocus();
									}
									ImGui::PopID();
								}
								ImGui::EndCombo();
							}

							s32 exitScreen = screen.exitTargetScreen;
							if (ImGui::InputInt("Exit target screen", &exitScreen)) {
								screen.exitTargetScreen = (u32)glm::clamp(exitScreen, 0, (s32)levelMaxScreenCount - 1);
							}

							ImGui::TreePop();
						}
					}

					ImGui::TreePop();
				}*/
				ImGui::EndDisabled();
				ImGui::PopID();
			}

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

static void DrawGameWindow() {
	ImGui::Begin("Level editor", &pContext->gameWindowOpen, ImGuiWindowFlags_MenuBar);

	Level* pCurrentLevel = Game::GetLevel();
	Viewport* pViewport = Game::GetViewport();
	Nametable* pNametables = Rendering::GetNametablePtr(0);

	static u32 selectedLevel = 0;
	static PoolHandle<Actor> selectedActorHandle = PoolHandle<Actor>::Null();

	static u32 editMode = EDIT_MODE_NONE;
	static LevelClipboard clipboard{};
	static LevelToolsState toolsState{};

	const bool noLevelLoaded = pCurrentLevel == nullptr;
	bool editing = Game::IsPaused() && !noLevelLoaded;

	Level* pLevels = Levels::GetLevelsPtr();
	Level& editedLevel = pLevels[selectedLevel];

	const bool editingCurrentLevel = pCurrentLevel == &editedLevel;

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save")) {
				Levels::SaveLevels("assets/levels.lev");
			}
			if (ImGui::MenuItem("Revert changes")) {
				Levels::LoadLevels("assets/levels.lev");
				RefreshViewport(pViewport, pNametables, pCurrentLevel->pTilemap);
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

	ImGui::BeginChild("Level list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	{
		static constexpr u32 maxLabelNameLength = LEVEL_MAX_NAME_LENGTH + 8;
		char label[maxLabelNameLength];

		for (u32 i = 0; i < MAX_LEVEL_COUNT; i++)
		{
			ImGui::PushID(i);

			snprintf(label, maxLabelNameLength, "%#04x: %s", i, pLevels[i].name);

			const bool selected = selectedLevel == i;
			const bool isCurrent = pCurrentLevel == pLevels + i;

			if (isCurrent) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,1,0,1));
			}

			if (ImGui::Selectable(label, selected)) {
				selectedLevel = i;
			}

			if (isCurrent) {
				ImGui::PopStyleColor();
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				ImGui::SetDragDropPayload("swap_levels", &i, sizeof(u32));
				ImGui::Text("%s", pLevels[i].name);

				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("swap_levels"))
				{
					int sourceIndex = *(const u32*)payload->Data;

					s32 step = i - sourceIndex;

					ImVector<s32> vec;
					vec.push_back(sourceIndex);

					const bool canMove = CanMoveElements(MAX_LEVEL_COUNT, vec, step);

					if (canMove) {
						MoveElements<Level>(pLevels, vec, step);

						Game::ReloadLevel();
						selectedLevel = editing ? (pCurrentLevel - pLevels) : vec[0];
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PopID();

			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::Selectable("Load level")) {
					Game::LoadLevel(i);
					Game::SetPaused(true);
					selectedActorHandle = PoolHandle<Actor>::Null();
				}
				ImGui::EndPopup();
			}
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
	DrawGameView(pCurrentLevel, editing, editMode, clipboard, selectedLevel, selectedActorHandle);
	ImGui::EndChild();

	ImGui::SameLine();

	// Reset edit mode, it will be set by the tools window
	editMode = EDIT_MODE_NONE;
	if (showToolsWindow) {
		ImGui::BeginChild("Level tools", ImVec2(toolWindowWidth,0));
		DrawLevelTools(selectedLevel, editing, editMode, toolsState, clipboard, selectedActorHandle);
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

	ActorPrototype* pPrototypes = Actors::GetPrototype(0);
	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++)
	{
		ImGui::PushID(i);

		snprintf(label, maxLabelNameLength, "0x%02x: %s", i, Actors::GetPrototypeName(i));

		const bool selected = selection == i;
		if (ImGui::Selectable(label, selected)) {
			selection = i;
		}

		if (selected) {
			ImGui::SetItemDefaultFocus();
		}

		/*if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			ImGui::SetDragDropPayload("dd_actors", &i, sizeof(u32));
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
		}*/
		ImGui::PopID();
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
				Actors::SavePrototypes("assets/actors.prt");
			}
			if (ImGui::MenuItem("Revert changes")) {
				Actors::LoadPrototypes("assets/actors.prt");
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
		ActorPrototype* pPrototype = Actors::GetPrototype(selection);
		char* prototypeName = Actors::GetPrototypeName(selection);

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

				DrawTypeSelectionCombo("Type", ACTOR_TYPE_NAMES, ACTOR_TYPE_COUNT, pPrototype->type);

				const char* const* subtypeNames = nullptr;
				u32 subtypeCount = 0;
				switch (pPrototype->type) {
				case ACTOR_TYPE_PLAYER: {
					subtypeNames = PLAYER_SUBTYPE_NAMES;
					subtypeCount = PLAYER_SUBTYPE_COUNT;
					break;
				}
				case ACTOR_TYPE_NPC: {
					subtypeNames = NPC_SUBTYPE_NAMES;
					subtypeCount = NPC_SUBTYPE_COUNT;
					break;
				}
				case ACTOR_TYPE_BULLET: {
					subtypeNames = BULLET_SUBTYPE_NAMES;
					subtypeCount = BULLET_SUBTYPE_COUNT;
					break;
				}
				case ACTOR_TYPE_PICKUP: {
					subtypeNames = PICKUP_SUBTYPE_NAMES;
					subtypeCount = PICKUP_SUBTYPE_COUNT;
					break;
				}
				case ACTOR_TYPE_EFFECT: {
					subtypeNames = EFFECT_SUBTYPE_NAMES;
					subtypeCount = EFFECT_SUBTYPE_COUNT;
					break;
				}
				default:
					break;
				}

				pPrototype->subtype = glm::clamp(pPrototype->subtype, u16(0), u16(subtypeCount - 1));
				DrawTypeSelectionCombo("Subtype", subtypeNames, subtypeCount, pPrototype->subtype);
				DrawTypeSelectionCombo("Alignment", ACTOR_ALIGNMENT_NAMES, ACTOR_ALIGNMENT_COUNT, pPrototype->alignment);

				ImGui::SeparatorText("Type data");

				const char* prototypeNames[MAX_ACTOR_PROTOTYPE_COUNT]{};
				Actors::GetPrototypeNames(prototypeNames);

				// TODO: Split into many functions?
				switch (pPrototype->type) {
				case ACTOR_TYPE_PLAYER: {
					
					break;
				}
				case ACTOR_TYPE_NPC: {
					ImGui::InputScalar("Health", ImGuiDataType_U16, &pPrototype->npcData.health);
					ImGui::InputScalar("Exp value", ImGuiDataType_U16, &pPrototype->npcData.expValue);
					// TODO: loot type
					DrawTypeSelectionCombo("Spawn on death", prototypeNames, MAX_ACTOR_PROTOTYPE_COUNT, pPrototype->npcData.spawnOnDeath);

					break;
				}
				case ACTOR_TYPE_BULLET: {
					ImGui::InputScalar("Lifetime", ImGuiDataType_U16, &pPrototype->bulletData.lifetime);
					DrawTypeSelectionCombo("Spawn on death", prototypeNames, MAX_ACTOR_PROTOTYPE_COUNT, pPrototype->bulletData.spawnOnDeath);
					break;
				}
				case ACTOR_TYPE_PICKUP: {
					ImGui::InputScalar("Value", ImGuiDataType_U16, &pPrototype->pickupData.value);
					break;
				}
				case ACTOR_TYPE_EFFECT: {
					ImGui::InputScalar("Lifetime", ImGuiDataType_U16, &pPrototype->effectData.lifetime);
					break;
				}
				default:
					break;
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

#pragma region Level Connections
constexpr r32 LEVEL_SCREEN_ASPECT = (r32)VIEWPORT_WIDTH_METATILES / VIEWPORT_HEIGHT_METATILES;
constexpr r32 LEVEL_NODE_SCREEN_HEIGHT = 100.0f;
constexpr r32 LEVEL_NODE_OVERWORLD_TILE_DIM = 25.0f;
constexpr glm::vec2 LEVEL_NODE_SCREEN_SIZE = glm::vec2(LEVEL_NODE_SCREEN_HEIGHT * LEVEL_SCREEN_ASPECT, LEVEL_NODE_SCREEN_HEIGHT);

struct LevelNode {
	s32 levelIndex;
	glm::vec2 position;
};

enum LevelNodeExitPinTypeFlags : u8 {
	NODE_EXIT_NONE = 0,
	NODE_EXIT_LEFT = 1,
	NODE_EXIT_RIGHT = 2,
	NODE_EXIT_TOP = 4,
	NODE_EXIT_BOTTOM = 8,

	NODE_EXIT_HORIZONTAL = 3,
	NODE_EXIT_VERTICAL = 12,
	NODE_EXIT_OMNI = 15
};

struct LevelNodePin {
	glm::vec2 offset;
	glm::vec2 direction;

	s32 levelIndex = -1;
	s8 screenIndex = -1;
	s16 tileIndex = -1;
	u8 exit = 0;

	bool Valid() const {
		return levelIndex != -1 && screenIndex != -1;
	}
	bool operator==(const LevelNodePin& a) const {
		return Hash() == a.Hash();
	}
	u64 Hash() const {
		return (u64(u32(levelIndex)) << 32 |
			u64(u8(screenIndex)) << 24 |
			u64(u16(tileIndex)) << 8 |
			u64(exit)
			);
	}
};

struct LevelNodePinHasher {
	u64 operator()(const LevelNodePin& pin) const {
		return pin.Hash();
	}
};

typedef std::unordered_map<s32, LevelNode> LevelNodeMap;

struct LevelNodeConnection {
	LevelNodePin from, to;
};

static r32 GetLevelNodeDrawScale(const LevelNode& node, const glm::mat3& canvasToScreen) {
	const Level& level = Levels::GetLevelsPtr()[node.levelIndex];

	const r32 zoomScale = glm::length(glm::vec2(canvasToScreen[0][0], canvasToScreen[1][0]));

	if (level.flags.type == LEVEL_TYPE_OVERWORLD) {
		return zoomScale * LEVEL_NODE_OVERWORLD_TILE_DIM;
	}

	return zoomScale * (LEVEL_NODE_SCREEN_HEIGHT / VIEWPORT_HEIGHT_METATILES);

}

static glm::vec2 GetLevelNodeCanvasSize(const LevelNode& node, r32 scale) {
	const Level& level = Levels::GetLevelsPtr()[node.levelIndex];

	return glm::vec2(VIEWPORT_WIDTH_METATILES * level.pTilemap->width, VIEWPORT_HEIGHT_METATILES * level.pTilemap->height) * scale;
}

static void GetNodePins(const LevelNode& node, std::vector<LevelNodePin>& outPins) {
	const Level& level = Levels::GetLevelsPtr()[node.levelIndex];

	for (u32 y = 0; y < level.pTilemap->height; y++) {
		for (u32 x = 0; x < level.pTilemap->width; x++) {
			const s8 screenIndex = x + y * TILEMAP_MAX_DIM_SCREENS;

			if (level.flags.type == LEVEL_TYPE_OVERWORLD) {
				for (u16 i = 0; i < VIEWPORT_SIZE_METATILES; i++) {

					const glm::vec2 screenSize = LEVEL_NODE_OVERWORLD_TILE_DIM * glm::vec2(VIEWPORT_WIDTH_METATILES, VIEWPORT_HEIGHT_METATILES);
					const glm::vec2 screenTopLeft = glm::vec2(x, y) * screenSize;

					const u32 xTile = i % VIEWPORT_WIDTH_METATILES;
					const u32 yTile = i / VIEWPORT_WIDTH_METATILES;

					const glm::vec2 offset = screenTopLeft + glm::vec2(xTile + 0.5f, yTile + 0.5f) * LEVEL_NODE_OVERWORLD_TILE_DIM;

					outPins.emplace_back(offset, glm::vec2(0), node.levelIndex, screenIndex, i, NODE_EXIT_HORIZONTAL);
				}
			}
			else {
				const bool hasLeftExit = (x == 0);
				const bool hasRightExit = (x == level.pTilemap->width - 1);
				const bool hasTopExit = (y == 0);
				const bool hasBottomExit = (y == level.pTilemap->height - 1);
				u8 missingExits = NODE_EXIT_NONE;

				const glm::vec2 screenTopLeft = glm::vec2(x, y) * LEVEL_NODE_SCREEN_SIZE;
				const glm::vec2 screenBtmRight = screenTopLeft + LEVEL_NODE_SCREEN_SIZE;
				const glm::vec2 screenMid = screenTopLeft + LEVEL_NODE_SCREEN_SIZE / 2.0f;

				// Top connection pin
				if (hasTopExit) {
					outPins.emplace_back(glm::vec2(screenMid.x, screenTopLeft.y), glm::vec2(0, -1), node.levelIndex, screenIndex, -1, NODE_EXIT_TOP);
				}
				else missingExits |= NODE_EXIT_TOP;
				// Left connection pin
				if (hasLeftExit) {
					outPins.emplace_back(glm::vec2(screenTopLeft.x, screenMid.y), glm::vec2(-1, 0), node.levelIndex, screenIndex, -1, NODE_EXIT_LEFT);
				}
				else missingExits |= NODE_EXIT_LEFT;
				// Right connection pin
				if (hasRightExit) {
					outPins.emplace_back(glm::vec2(screenBtmRight.x, screenMid.y), glm::vec2(1, 0), node.levelIndex, screenIndex, -1, NODE_EXIT_RIGHT);
				}
				else missingExits |= NODE_EXIT_RIGHT;
				// Bottom connection pin
				if (hasBottomExit) {
					outPins.emplace_back(glm::vec2(screenMid.x, screenBtmRight.y), glm::vec2(0, 1), node.levelIndex, screenIndex, -1, NODE_EXIT_BOTTOM);
				}
				else missingExits |= NODE_EXIT_BOTTOM;

				// Middle connection pin
				if (missingExits != NODE_EXIT_NONE) {
					outPins.emplace_back(screenMid, glm::vec2(0), node.levelIndex, screenIndex, -1, missingExits);
				}
			}

		}
	}
}

static bool MousePosWithinNodeBounds(const LevelNode& node, const glm::vec3& mousePosInCanvas) {
	const glm::vec2 nodeSize = GetLevelNodeCanvasSize(node, 1.0f / LEVEL_NODE_SCREEN_HEIGHT);

	return (mousePosInCanvas.x >= node.position.x && 
		mousePosInCanvas.y >= node.position.y &&
		mousePosInCanvas.x < node.position.x + nodeSize.x &&
		mousePosInCanvas.y < node.position.y + nodeSize.y);
}

static bool IsPinConnected(const LevelNodePin& pin, const std::unordered_set<LevelNodePin, LevelNodePinHasher>& connectedPins) {
	return connectedPins.find(pin) != connectedPins.end();
}

static void ClearPinConnections(const LevelNodePin& pin, std::vector<LevelNodeConnection>& connections) {
	std::erase_if(connections, [pin](const LevelNodeConnection& connection) {
		return connection.from == pin || connection.to == pin;
		});
}

static void ClearNodeConnections(s32 levelIndex, std::vector<LevelNodeConnection>& connections) {
	std::erase_if(connections, [levelIndex](const LevelNodeConnection& connection) {
		return connection.from.levelIndex == levelIndex || connection.to.levelIndex == levelIndex;
		});
}

static void DrawLevelNode(const LevelNode& node, const glm::mat3& canvasToScreen, const std::unordered_set<LevelNodePin, LevelNodePinHasher>& connectedPins, bool& outHovered, LevelNodePin& outHoveredPin) {
	outHovered = false;
	outHoveredPin = LevelNodePin();

	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	constexpr r32 pinRadius = 6.f;
	static std::vector<LevelNodePin> pins{};
	pins.clear();
	GetNodePins(node, pins);

	std::vector<glm::vec2> pinPositions(pins.size());

	// Check mouse hover over pins
	for (u32 i = 0; i < pins.size(); i++) {
		const LevelNodePin& pin = pins[i];
		glm::vec2& pinPos = pinPositions[i];
		pinPos = node.position + pin.offset;
		pinPos = canvasToScreen * glm::vec3(pinPos.x, pinPos.y, 1.0f);

		if (glm::distance(glm::vec2(io.MousePos.x, io.MousePos.y), pinPos) <= pinRadius) {
			outHoveredPin = pin;
		}
	}

	// Node body
	const r32 levelDrawScale = GetLevelNodeDrawScale(node, canvasToScreen);
	const glm::vec2 nodeScreenTopLeft = canvasToScreen * glm::vec3(node.position.x, node.position.y, 1.0f);

	const glm::vec2 nodeCanvasSize = GetLevelNodeCanvasSize(node, levelDrawScale);
	const glm::vec2 nodeScreenBtmRight = nodeScreenTopLeft + nodeCanvasSize;

	const ImVec2 nodeDrawMin = ImVec2(nodeScreenTopLeft.x, nodeScreenTopLeft.y);
	const ImVec2 nodeDrawMax = ImVec2(nodeScreenBtmRight.x, nodeScreenBtmRight.y);

	constexpr ImU32 outlineColor = IM_COL32(255, 255, 255, 255);
	constexpr ImU32 outlineHoveredColor = IM_COL32(255, 255, 0, 255);
	constexpr r32 outlineThickness = 1.0f;
	constexpr r32 outlineHoveredThickness = 2.0f;

	// Draw background color
	drawList->AddImage(pContext->paletteTexture, nodeDrawMin, nodeDrawMax, ImVec2(0, 0), ImVec2(0.015625f, 1.0f));

	const Level& level = Levels::GetLevelsPtr()[node.levelIndex];
	DrawLevel(&level, nodeDrawMin, levelDrawScale);

	drawList->AddRectFilled(nodeDrawMin, nodeDrawMax, IM_COL32(0, 0, 0, 0x80));

	drawList->AddText(ImVec2(nodeDrawMin.x + 10, nodeDrawMin.y + 10), IM_COL32(255, 255, 255, 255), level.name);

	ImU32 nodeOutlineColor = outlineColor;
	r32 nodeOutlineThickness = outlineThickness;
	// If node hovered
	if (!outHoveredPin.Valid() &&
		io.MousePos.x >= nodeDrawMin.x &&
		io.MousePos.y >= nodeDrawMin.y &&
		io.MousePos.x < nodeDrawMax.x &&
		io.MousePos.y < nodeDrawMax.y) {
		nodeOutlineColor = outlineHoveredColor;
		nodeOutlineThickness = outlineHoveredThickness;
		outHovered = true;
	}

	drawList->AddRect(nodeDrawMin, nodeDrawMax, nodeOutlineColor, 0, 0, nodeOutlineThickness);

	// z = 0 for only scaling, no translation
	const glm::vec2 screenDrawSize = canvasToScreen * glm::vec3(LEVEL_NODE_SCREEN_SIZE.x, LEVEL_NODE_SCREEN_SIZE.y, 0.0f);
	const glm::vec2 screenDrawSizeHalf = screenDrawSize / 2.0f;

	// Draw connection pins
	for (u32 i = 0; i < pins.size(); i++) {
		constexpr ImU32 pinDisconnectedColor = IM_COL32(0xd6, 0x45, 0x45, 255);
		constexpr ImU32 pinConnectedColor = IM_COL32(0x27, 0xae, 0x60, 255);

		const LevelNodePin& pin = pins[i];
		const bool connected = IsPinConnected(pin, connectedPins);
		const bool hovered = outHoveredPin == pin;

		// There are so many possible pins in the overworld that I don't want to draw them all
		if (level.flags.type == LEVEL_TYPE_OVERWORLD && !connected && !hovered) {
			continue;
		}

		const glm::vec2& pinPos = pinPositions[i];
		const ImU32 pinDrawColor = connected ? pinConnectedColor : pinDisconnectedColor;
		ImU32 pinOutlineColor = outlineColor;
		r32 pinOutlineThickness = outlineThickness;
		if (hovered) {
			pinOutlineColor = outlineHoveredColor;
			pinOutlineThickness = outlineHoveredThickness;
		}

		drawList->AddCircleFilled(ImVec2(pinPos.x, pinPos.y), pinRadius, pinDrawColor);
		drawList->AddCircle(ImVec2(pinPos.x, pinPos.y), pinRadius, pinOutlineColor, 0, pinOutlineThickness);
	}
}

static bool CanPinsConnect(const LevelNodePin& a, const LevelNodePin& b) {
	if (!a.Valid() || !b.Valid()) {
		return false;
	}

	return (
		(a.exit & NODE_EXIT_RIGHT) && (b.exit & NODE_EXIT_LEFT) ||
		(a.exit & NODE_EXIT_LEFT) && (b.exit & NODE_EXIT_RIGHT) ||
		(a.exit & NODE_EXIT_BOTTOM) && (b.exit & NODE_EXIT_TOP) ||
		(a.exit & NODE_EXIT_TOP) && (b.exit & NODE_EXIT_BOTTOM)
		);
}

static void GetScreenExitDirs(u8 aFlags, u8 bFlags, u8& aOutDir, u8& bOutDir) {
	if ((aFlags & NODE_EXIT_LEFT) && (bFlags & NODE_EXIT_RIGHT)) {
		aOutDir = SCREEN_EXIT_DIR_LEFT;
		bOutDir = SCREEN_EXIT_DIR_RIGHT;
		return;
	}
	if ((aFlags & NODE_EXIT_RIGHT) && (bFlags & NODE_EXIT_LEFT)) {
		aOutDir = SCREEN_EXIT_DIR_RIGHT;
		bOutDir = SCREEN_EXIT_DIR_LEFT;
		return;
	}
	if ((aFlags & NODE_EXIT_TOP) && (bFlags & NODE_EXIT_BOTTOM)) {
		aOutDir = SCREEN_EXIT_DIR_TOP;
		bOutDir = SCREEN_EXIT_DIR_BOTTOM;
		return;
	}
	if ((aFlags & NODE_EXIT_BOTTOM) && (bFlags & NODE_EXIT_TOP)) {
		aOutDir = SCREEN_EXIT_DIR_BOTTOM;
		bOutDir = SCREEN_EXIT_DIR_TOP;
		return;
	}

}

static void ConnectLevels(const LevelNodePin& a, const LevelNodePin& b) {
	// Write level screen metadata with new connection
	const Level& aLevel = Levels::GetLevelsPtr()[a.levelIndex];
	const Level& bLevel = Levels::GetLevelsPtr()[b.levelIndex];

	const TilemapScreen& aScreen = aLevel.pTilemap->screens[a.screenIndex];
	const TilemapScreen& bScreen = bLevel.pTilemap->screens[b.screenIndex];

	u8 aDir, bDir;
	GetScreenExitDirs(a.exit, b.exit, aDir, bDir);

	LevelExit* aExits = (LevelExit*)&aScreen.screenMetadata;
	LevelExit* bExits = (LevelExit*)&bScreen.screenMetadata;

	aExits[aDir] = {
		.targetLevel = u16(b.levelIndex),
		.targetScreen = u16(b.screenIndex)
	};

	bExits[bDir] = {
		.targetLevel = u16(a.levelIndex),
		.targetScreen = u16(a.screenIndex)
	};
}

static void DrawLevelNodeGraph(LevelNodeMap& nodes) {
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
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("dd_levels"))
		{
			s32 levelInd = *(const s32*)payload->Data;

			const glm::mat3 canvasToScreen = canvasMat * viewMat;
			const glm::mat3 screenToCanvas = glm::inverse(canvasToScreen);
			const glm::vec2 mouseInCanvas = screenToCanvas * glm::vec3(io.MousePos.x, io.MousePos.y, 1.0f);

			nodes.emplace(levelInd, LevelNode{
				.levelIndex = levelInd,
				.position = mouseInCanvas,
				});
		}
		ImGui::EndDragDropTarget();
	}

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
	const float gridStep = 64.0f;

	const glm::vec2 topLeftCanvas = screenToCanvas * glm::vec3(canvas_p0.x, canvas_p0.y, 1.0f);
	const glm::vec2 btmRightCanvas = screenToCanvas * glm::vec3(canvas_p1.x, canvas_p1.y, 1.0f);

	const float xStart = glm::floor(topLeftCanvas.x / gridStep) * gridStep;
	const float yStart = glm::floor(topLeftCanvas.y / gridStep) * gridStep;
	const float xEnd = glm::ceil(btmRightCanvas.x / gridStep) * gridStep;
	const float yEnd = glm::ceil(btmRightCanvas.y / gridStep) * gridStep;

	for (float x = xStart; x < xEnd; x += gridStep) {
		glm::vec2 screenStart = canvasToScreen * glm::vec3(x, topLeftCanvas.y, 1.0f);
		glm::vec2 screenEnd = canvasToScreen * glm::vec3(x, btmRightCanvas.y, 1.0f);
		drawList->AddLine(ImVec2(screenStart.x, screenStart.y), ImVec2(screenEnd.x, screenEnd.y), IM_COL32(200, 200, 200, 40));
	}
	for (float y = yStart; y < yEnd; y += gridStep) {
		glm::vec2 screenStart = canvasToScreen * glm::vec3(topLeftCanvas.x, y, 1.0f);
		glm::vec2 screenEnd = canvasToScreen * glm::vec3(btmRightCanvas.x, y, 1.0f);
		drawList->AddLine(ImVec2(screenStart.x, screenStart.y), ImVec2(screenEnd.x, screenEnd.y), IM_COL32(200, 200, 200, 40));
	}

	const glm::vec3 mousePosInCanvas = screenToCanvas * glm::vec3(io.MousePos.x, io.MousePos.y, 1.0f);

	// Draw nodes
	static s32 draggedNode = -1;
	static s32 contextNode = -1;

	static LevelNodePin contextPin{};

	static std::vector<LevelNodeConnection> connections{};
	static LevelNodeConnection pendingConnection = {
		.from = LevelNodePin(),
		.to = LevelNodePin()
	};

	// Precompute connected pin set
	static std::unordered_set<LevelNodePin, LevelNodePinHasher> connectedPins;
	connectedPins.clear();
	connectedPins.reserve(connections.size() * 2);

	for (const auto& connection : connections) {
		connectedPins.insert(connection.from);
		connectedPins.insert(connection.to);
	}

	for (const auto& [id, node] : nodes) {
		bool nodeHovered = false;
		LevelNodePin pin{};
		DrawLevelNode(node, canvasToScreen, connectedPins, nodeHovered, pin);

		const bool pinHovered = pin.Valid();
		const bool hoveredExitConnected = pinHovered && IsPinConnected(pin, connectedPins);
		// If hovering over an exit
		if (hovered && pinHovered) {
			// Try to set hovered exit as connection target
			if (pendingConnection.from.Valid() && // There is a pending connection from some node
				!hoveredExitConnected && // The hovered exit is not already connected to something
				pendingConnection.from != pin // We're not trying to connect an exit to itself
			) {
				pendingConnection.to = pin;
			}
		}
		else if (pendingConnection.to == pin)
		{
			pendingConnection.to = LevelNodePin{};
		}

		if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			if (nodeHovered) {
				draggedNode = id;
			}
			else if (pinHovered && !hoveredExitConnected) {
				pendingConnection.from = pin;
			}
		}

		if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			if (nodeHovered) {
				contextNode = id;
				ImGui::OpenPopup("NodeContextMenu");
			}
			else if (pinHovered) {
				contextPin = pin;
				ImGui::OpenPopup("ExitContextMenu");
			}
		}
	}

	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && draggedNode != -1) {
		glm::vec2 scaledDelta = screenToCanvas * glm::vec3(io.MouseDelta.x, io.MouseDelta.y, 0.0f);

		LevelNode& node = nodes[draggedNode];
		node.position.x += scaledDelta.x;
		node.position.y += scaledDelta.y;
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		draggedNode = -1;

		if (CanPinsConnect(pendingConnection.from, pendingConnection.to)) {
			ConnectLevels(pendingConnection.from, pendingConnection.to);
			connections.push_back(pendingConnection);
		}
		pendingConnection = LevelNodeConnection{};
	}

	if (ImGui::BeginPopup("NodeContextMenu")) {
		if (ImGui::MenuItem("Delete Node") && contextNode != -1) {
			ClearNodeConnections(contextNode, connections);
			nodes.erase(contextNode);
			contextNode = -1;
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("ExitContextMenu")) {
		if (ImGui::MenuItem("Sever connection") && contextPin.Valid()) {
			ClearPinConnections(contextPin, connections);
			contextPin = LevelNodePin();
		}
		ImGui::EndPopup();
	}

	// Draw connections
	constexpr r32 bezierOffset = 64.f;
	if (pendingConnection.from.Valid()) {
		const glm::vec2 pCanvas = nodes[pendingConnection.from.levelIndex].position + pendingConnection.from.offset;
		const glm::vec2 pScreen = canvasToScreen * glm::vec3(pCanvas.x, pCanvas.y, 1.0f);
		const glm::vec2 pTangent = pendingConnection.from.direction;

		const ImVec2 p0 = ImVec2(pScreen.x, pScreen.y);
		const ImVec2 p1 = ImVec2(pScreen.x + pTangent.x * bezierOffset, pScreen.y + pTangent.y * bezierOffset);
		const ImVec2 p2 = io.MousePos;
		const ImVec2 p3 = io.MousePos;

		drawList->AddBezierCubic(p0, p1, p2, p3, IM_COL32(255, 255, 255, 255), 1.0f);
	}

	for (auto& connection : connections) {
		const glm::vec2 p0Canvas = nodes[connection.from.levelIndex].position + connection.from.offset;
		const glm::vec2 p0Screen = canvasToScreen * glm::vec3(p0Canvas.x, p0Canvas.y, 1.0f);
		const glm::vec2 p0Tangent = connection.from.direction;

		const glm::vec2 p1Canvas = nodes[connection.to.levelIndex].position + connection.to.offset;
		const glm::vec2 p1Screen = canvasToScreen * glm::vec3(p1Canvas.x, p1Canvas.y, 1.0f);
		const glm::vec2 p1Tangent = connection.to.direction;

		const ImVec2 p0 = ImVec2(p0Screen.x, p0Screen.y);
		const ImVec2 p1 = ImVec2(p0Screen.x + p0Tangent.x * bezierOffset, p0Screen.y + p0Tangent.y * bezierOffset);
		const ImVec2 p2 = ImVec2(p1Screen.x + p1Tangent.x * bezierOffset, p1Screen.y + p1Tangent.y * bezierOffset);
		const ImVec2 p3 = ImVec2(p1Screen.x, p1Screen.y);

		drawList->AddBezierCubic(p0, p1, p2, p3, IM_COL32(255, 255, 255, 255), 1.0f);
	}

	drawList->PopClipRect();
}

static void DrawLevelConnectionsWindow() {
	ImGui::Begin("Level connections", &pContext->levelConnectionsOpen);

	static LevelNodeMap nodes{};

	ImGui::BeginChild("Level list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	{
		const Level* pLevels = Levels::GetLevelsPtr();
		static s32 selection = 0;

		// Construct available levels list
		std::vector<s32> availableLevels{};
		availableLevels.reserve(MAX_LEVEL_COUNT);

		for (u32 i = 0; i < MAX_LEVEL_COUNT; i++) {
			auto it = std::find_if(nodes.begin(), nodes.end(), [i](const auto& kvp) {
				return kvp.second.levelIndex == i;
			});

			if (it == nodes.end()) {
				availableLevels.push_back(i);
			}
		}

		static constexpr u32 maxLabelNameLength = LEVEL_MAX_NAME_LENGTH + 8;
		char label[maxLabelNameLength];

		for (auto& levelInd : availableLevels) {
			const Level& level = pLevels[levelInd];

			ImGui::PushID(levelInd);

			snprintf(label, maxLabelNameLength, "%#04x: %s", levelInd, level.name);

			const bool selected = selection == levelInd;
			if (ImGui::Selectable(label, selected)) {
				selection = levelInd;
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				ImGui::SetDragDropPayload("dd_levels", &levelInd, sizeof(s32));
				ImGui::Text("%s", level.name);

				ImGui::EndDragDropSource();
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("Node graph");
	DrawLevelNodeGraph(nodes);
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
			if (ImGui::MenuItem("Level editor")) {
				pContext->gameWindowOpen = true;
			}
			if (ImGui::MenuItem("Level connections editor")) {
				pContext->levelConnectionsOpen = true;
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

	if (pContext->spriteWindowOpen) {
		DrawSpriteWindow();
	}

	if (pContext->tilesetWindowOpen) {
		DrawTilesetWindow();
	}

	if (pContext->gameWindowOpen) {
		DrawGameWindow();
	}

	if (pContext->actorWindowOpen) {
		DrawActorWindow();
	}
;
	if (pContext->audioWindowOpen) {
		DrawAudioWindow();
	}

	if (pContext->levelConnectionsOpen) {
		DrawLevelConnectionsWindow();
	}

	ImGui::Render();
}
#pragma endregion