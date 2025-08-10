#include "editor.h"
#include "editor_assets.h"
#include "rendering.h"
#include "debug.h"
#include <cassert>
#include <limits>
#include <type_traits>
#include <SDL.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imfilebrowser.h>
#include <immintrin.h>
#include "tilemap.h"
#include "rendering_util.h"
#include "game.h"
#include "game_rendering.h"
#include "game_state.h"
#include "actors.h"
#include "audio.h"
#include "collision.h"
#include "random.h"
#include "asset_manager.h"
#include "asset_serialization.h"
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
	ROOM_EDIT_MODE_NONE = 0,
	ROOM_EDIT_MODE_TILES = 1,
	ROOM_EDIT_MODE_ACTORS = 2
};

enum OverworldEditMode {
	OW_EDIT_MODE_NONE = 0,
	OW_EDIT_MODE_TILES,
	OW_EDIT_MODE_AREAS
};

struct EditedAssetRenderData {
	EditorRenderTexture* pTexture;
	EditorRenderBuffer* pBuffer;
};

struct EditorContext {
	EditorRenderTexture* pChrTexture; // Legacy - will be removed
	EditorRenderTexture* pIndexedChrTextures[CHR_COUNT]; // New indexed CHR textures
	EditorRenderBuffer* pChrBuffers[CHR_COUNT]; // Buffers for each CHR sheet
	EditorRenderTexture* pPaletteTexture;
	EditorRenderTexture* pColorTexture;

	std::unordered_map<u64, EditorRenderBuffer*> assetRenderBuffers;
	std::vector<u64> bufferEraseList;
	std::unordered_set<EditedAssetRenderData*> tempRenderData;
	std::vector<EditedAssetRenderData*> tempEraseList;

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
	bool assetBrowserOpen = false;
	bool dungeonWindowOpen = false;
	bool overworldWindowOpen = false;
	bool animationWindowOpen = false;
	bool soundWindowOpen = false;
	bool chrWindowOpen = false;
	bool paletteWindowOpen = false;

	// Debug overlay
	bool showDebugOverlay = true;
	bool drawActorHitboxes = false;
	bool drawActorPositions = false;
};

static EditorContext* pContext;

#pragma region Utils
static ImTextureID GetTextureID(const EditorRenderTexture* pTexture) {
	return (ImTextureID)Rendering::GetEditorTextureData(pTexture);
}

static ImTextureID GetIndexedChrTextureID(u32 chrIndex) {
	if (chrIndex >= CHR_COUNT) {
		return nullptr;
	}
	return GetTextureID(pContext->pIndexedChrTextures[chrIndex]);
}

// Simplified function to add CHR image using indexed textures
// For now, this will use the legacy texture but with correct UV mapping for a single palette
static void AddChrImage(ImDrawList* drawList, u8 tileIndex, u8 chrIndex, u8 palette, const ImVec2& pos, const ImVec2& size, bool flipX = false, bool flipY = false, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	// For now, fall back to the legacy system but prepare for the indexed transition
	const glm::vec4 uvMinMax = ChrTileToTexCoord(tileIndex, chrIndex, palette);
	drawList->AddImage(GetTextureID(pContext->pChrTexture), pos, ImVec2(pos.x + size.x, pos.y + size.y), 
		ImVec2(flipX ? uvMinMax.z : uvMinMax.x, flipY ? uvMinMax.w : uvMinMax.y), 
		ImVec2(!flipX ? uvMinMax.z : uvMinMax.x, !flipY ? uvMinMax.w : uvMinMax.y), color);
}

static inline glm::vec4 NormalizedToChrTexCoord(const glm::vec4& normalized, u8 chrIndex, u8 palette) {
	constexpr r32 INV_CHR_COUNT = 1.0f / CHR_COUNT;
	constexpr r32 INV_SHEET_PALETTE_COUNT = (1.0f / (PALETTE_COUNT / 2));
	
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
	drawList->AddImage(GetTextureID(pContext->pPaletteTexture), topLeft, btmRight, ImVec2(0, 0), ImVec2(invBgColorIndex, 1.0f));
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

static ImVec2 DrawColorGrid(ImVec2 size, s32* selection = nullptr, bool* focused = nullptr) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const ImVec2 gridPos = DrawTileGrid(size, size.x / 16, selection, focused);

	ImTextureID textureId = GetTextureID(pContext->pColorTexture);
	drawList->AddImage(textureId, gridPos, ImVec2(gridPos.x + size.x, gridPos.y + size.y), ImVec2(0, 0), ImVec2(1, 1));

	return gridPos;
}

// See GetMetatileVertices for human-readable version
static void GetMetatileVerticesAVX(const Metatile& metatile, const ImVec2& pos, r32 scale, ImVec2* outVertices, ImVec2* outUV) {
	constexpr r32 TILE_SIZE = 1.0f / METATILE_DIM_TILES;
	constexpr r32 INV_CHR_COUNT = 1.0f / CHR_COUNT;
	constexpr r32 INV_CHR_DIM_TILES = 1.0f / CHR_DIM_TILES;
	constexpr u32 CHR_DIM_TILES_BITS = 0xf;
	constexpr u32 CHR_DIM_TILES_LOG2 = 4;
	constexpr r32 INV_SHEET_PALETTE_COUNT = (1.0f / (PALETTE_COUNT / 2));

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

		const u16 tileId = metatile.tiles[i].tileId;
		const u8 index = (u8)tileId;
		const u8 pageIndex = (tileId >> 8) & 3;
		const glm::vec4 uvMinMax = ChrTileToTexCoord(index, pageIndex, metatile.tiles[i].palette);

		// Handle flipping by modifying UV coordinate assignment
		const bool flipH = metatile.tiles[i].flipHorizontal;
		const bool flipV = metatile.tiles[i].flipVertical;

		// Base UV coordinates: top-left, top-right, bottom-right, bottom-left
		r32 u0 = flipH ? uvMinMax.z : uvMinMax.x; // top-left U
		r32 v0 = flipV ? uvMinMax.w : uvMinMax.y; // top-left V
		r32 u1 = flipH ? uvMinMax.x : uvMinMax.z; // top-right U
		r32 v1 = flipV ? uvMinMax.w : uvMinMax.y; // top-right V
		r32 u2 = flipH ? uvMinMax.x : uvMinMax.z; // bottom-right U
		r32 v2 = flipV ? uvMinMax.y : uvMinMax.w; // bottom-right V
		r32 u3 = flipH ? uvMinMax.z : uvMinMax.x; // bottom-left U
		r32 v3 = flipV ? uvMinMax.y : uvMinMax.w; // bottom-left V

		outUV[i * 4] = ImVec2(u0, v0);
		outUV[i * 4 + 1] = ImVec2(u1, v1);
		outUV[i * 4 + 2] = ImVec2(u2, v2);
		outUV[i * 4 + 3] = ImVec2(u3, v3);
	}
}

static inline void GetMetatileVerticesImpl(const Metatile& metatile, const ImVec2& pos, r32 scale, ImVec2* outVertices, ImVec2* outUV) {
	// TODO: Check CPU capabilities and use AVX if available
	GetMetatileVerticesAVX(metatile, pos, scale, outVertices, outUV);
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
	drawList->PushTextureID(GetTextureID(pContext->pChrTexture));
	drawList->PrimReserve(METATILE_TILE_COUNT * 6, METATILE_TILE_COUNT * 4);

	ImVec2 verts[METATILE_TILE_COUNT * 4];
	ImVec2 uv[METATILE_TILE_COUNT * 4];

	GetMetatileVerticesImpl(metatile, pos, size, verts, uv);
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
	return ImGui::ImageButton("", GetTextureID(pContext->pPaletteTexture), ImVec2(80, 10), ImVec2(invPaletteCount * palette, 0), ImVec2(invPaletteCount * (palette + 1), 1));
}

static void DrawCHRSheet(r32 size, u32 index, u8 palette, s32* selectedTile) {
	constexpr s32 gridSizeTiles = CHR_DIM_TILES;

	const r32 renderScale = size / (gridSizeTiles * TILE_DIM_PIXELS);
	const r32 gridStepPixels = TILE_DIM_PIXELS * renderScale;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 gridSize = ImVec2(size, size);
	const ImVec2 chrPos = DrawTileGrid(gridSize, gridStepPixels, selectedTile);
	const glm::vec4 uvMinMax = NormalizedToChrTexCoord({ 0,0,1,1, }, index, palette);

	drawList->AddImage(GetTextureID(pContext->pChrTexture), chrPos, ImVec2(chrPos.x + size, chrPos.y + size), ImVec2(uvMinMax.x, uvMinMax.y), ImVec2(uvMinMax.z, uvMinMax.w));
	if (selectedTile != nullptr && *selectedTile >= 0) {
		DrawTileGridSelection(chrPos, gridSize, gridStepPixels, *selectedTile);
	}
}

static bool DrawCHRPageSelector(r32 chrSize, u32 index, u8 palette, s32* selectedTile) {
	const s32 prevSelection = selectedTile ? *selectedTile : -1;
	const s32 selectedPage = selectedTile ? *selectedTile >> 8 : -1;

	if (ImGui::BeginTabBar("CHR Pages")) {
		for (u32 i = 0; i < CHR_PAGE_COUNT; i++) {
			char label[8];
			snprintf(label, 8, "Page #%d", i);
			if (ImGui::BeginTabItem(label)) {
				s32 selectedPageTile = -1;
				if (selectedPage == i) {
					selectedPageTile = *selectedTile & 0xff;
				}

				DrawCHRSheet(chrSize, i + index*CHR_PAGE_COUNT, palette, &selectedPageTile);

				if (selectedPageTile >= 0) {
					const s32 newSelection = selectedPageTile | (i << 8);
					*selectedTile = newSelection;
				}

				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	return selectedTile ? (*selectedTile != prevSelection) : false;
}

static void DrawTilemap(const Tilemap* pTilemap, const ImVec2& metatileOffset, const ImVec2& metatileSize, const ImVec2& pos, r32 scale) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Draw background color
	constexpr r32 invBgColorIndex = 1.0f / (PALETTE_COUNT * PALETTE_COLOR_COUNT);
	drawList->AddImage(GetTextureID(pContext->pPaletteTexture), pos, ImVec2(pos.x + metatileSize.x * scale, pos.y + metatileSize.y * scale), ImVec2(0, 0), ImVec2(invBgColorIndex, 1.0f));

	const Tileset* pTileset = AssetManager::GetAsset(pTilemap->tilesetHandle);
	if (!pTileset) {
		return;
	}

	const u32 tileCount = metatileSize.x * metatileSize.y * METATILE_TILE_COUNT;

	drawList->PushTextureID(GetTextureID(pContext->pChrTexture));
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
			const u8 tilesetTileIndex = pTilemap->GetTileData()[i];
			const TilesetTile& tilesetTile = pTileset->tiles[tilesetTileIndex];
			const Metatile& metatile = tilesetTile.metatile;

			const ImVec2 drawPos = ImVec2(pos.x + (x - xOffsetRemainder) * scale, pos.y + (y - yOffsetRemainder) * scale);
			GetMetatileVerticesImpl(metatile, drawPos, scale, verts, uv);
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

	drawList->PushTextureID(GetTextureID(pContext->pChrTexture));
	drawList->PrimReserve(tilesetTileCount * 6, tilesetTileCount * 4);

	ImVec2 verts[METATILE_TILE_COUNT * 4];
	ImVec2 uv[METATILE_TILE_COUNT * 4];

	for (s32 i = 0; i < TILESET_SIZE; i++) {
		ImVec2 metatileCoord = ImVec2(i % TILESET_DIM, i / TILESET_DIM);
		ImVec2 metatileOffset = ImVec2(chrPos.x + metatileCoord.x * gridStepPixels, chrPos.y + metatileCoord.y * gridStepPixels);

		const Metatile& metatile = pTileset->tiles[i].metatile;
		GetMetatileVerticesImpl(metatile, metatileOffset, renderScale * TILESET_DIM, verts, uv);
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
	const u8 pageIndex = (sprite.tileId >> 8) & 3;

	glm::vec4 uvMinMax = ChrTileToTexCoord(index, pageIndex + CHR_PAGE_COUNT, palette);

	drawList->AddImage(GetTextureID(pContext->pChrTexture), pos, ImVec2(pos.x + tileDrawSize, pos.y + tileDrawSize), ImVec2(flipX ? uvMinMax.z : uvMinMax.x, flipY ? uvMinMax.w : uvMinMax.y), ImVec2(!flipX ? uvMinMax.z : uvMinMax.x, !flipY ? uvMinMax.w : uvMinMax.y), color);
}

static void DrawMetasprite(const Metasprite* pMetasprite, const ImVec2& origin, r32 renderScale, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const r32 tileDrawSize = TILE_DIM_PIXELS * renderScale;
	for (s32 i = pMetasprite->spriteCount - 1; i >= 0; i--) {
		const Sprite& sprite = pMetasprite->GetSprites()[i];
		const ImVec2 pos = ImVec2(origin.x + renderScale * sprite.x, origin.y + renderScale * sprite.y);
		DrawSprite(sprite, pos, renderScale, color);
	}
}

static AABB GetActorBoundingBox(const RoomActor* pActor) {
	AABB result(-0.5f, 0.5f, -0.5f, 0.5f);
	if (pActor == nullptr) {
		return result;
	}
	
	const ActorPrototype* pPrototype = AssetManager::GetAsset(pActor->prototypeHandle);
	if (!pPrototype) {
		return result;
	}

	// TODO: What if animation changes bounds?
	const AnimationHandle& animHandle = pPrototype->GetAnimations()[0];
	const Animation* pAnimation = AssetManager::GetAsset(animHandle);
	if (!pAnimation) {
		return result;
	}

	const AnimationFrame& frame = pAnimation->GetFrames()[0];
	const Metasprite* pMetasprite = AssetManager::GetAsset(frame.metaspriteId);

	result.x1 = std::numeric_limits<r32>::max();
	result.x2 = std::numeric_limits<r32>::min();
	result.y1 = std::numeric_limits<r32>::max();
	result.y2 = std::numeric_limits<r32>::min();

	constexpr r32 tileWorldDim = 1.0f / METATILE_DIM_TILES;

	for (u32 i = 0; i < pMetasprite->spriteCount; i++) {
		const Sprite& sprite = pMetasprite->GetSprites()[i];
		const glm::vec2 spriteMin = { (r32)sprite.x / METATILE_DIM_PIXELS, (r32)sprite.y / METATILE_DIM_PIXELS };
		const glm::vec2 spriteMax = { spriteMin.x + tileWorldDim, spriteMin.y + tileWorldDim };
		result.x1 = glm::min(result.x1, spriteMin.x);
		result.x2 = glm::max(result.x2, spriteMax.x);
		result.y1 = glm::min(result.y1, spriteMin.y);
		result.y2 = glm::max(result.y2, spriteMax.y);
	}

	return result;
}

static void DrawAnimFrame(const AnimationHandle animHandle, const ImVec2& origin, r32 renderScale, s32 frameIndex = 0, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	const Animation* pAnimation = AssetManager::GetAsset(animHandle);
	if (!pAnimation) {
		return;
	}

	const AnimationFrame& frame = pAnimation->GetFrames()[frameIndex];
	const Metasprite* pMetasprite = AssetManager::GetAsset(frame.metaspriteId);
	if (!pMetasprite) {
		return;
	}

	DrawMetasprite(pMetasprite, origin, renderScale, color);
}

static void DrawActor(const ActorPrototype* pPrototype, const ImVec2& origin, r32 renderScale, s32 animIndex = 0, s32 frameIndex = 0, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	const AnimationHandle& animHandle = pPrototype->GetAnimations()[animIndex];
	DrawAnimFrame(animHandle, origin, renderScale, frameIndex, color);
}

static void DrawHitbox(const AABB* pHitbox, const ImVec2 origin, const r32 renderScale, ImU32 color = IM_COL32(0, 255, 0, 80)) {
	const r32 colliderDrawScale = METATILE_DIM_PIXELS * renderScale;

	const ImVec2 pMin = ImVec2(origin.x + colliderDrawScale * pHitbox->x1, origin.y + colliderDrawScale * pHitbox->y1);
	const ImVec2 pMax = ImVec2(origin.x + colliderDrawScale * pHitbox->x2, origin.y + colliderDrawScale * pHitbox->y2);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(pMin, pMax, color);
}

// Callback for displaying waveform in audio window
static r32 GetAudioSample(void* data, s32 idx) {
	return ((r32)((u8*)data)[idx]);
}

template <typename T>
static bool TrySwapElements(ImVector<T>& elements, ImVector<s32>& elementIndices, s32 i, s32 dir) {
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
static void MoveElements(ImVector<T>& elements, ImVector<s32>& elementIndices, s32 step) {
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
static void MoveElementsRange(ImVector<T>& elements, s32 begin, s32 end, s32 step, ImVector<s32>* selection) {
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
static u32 PushElement(ImVector<T>& elements, const T& element) {
	elements.push_back(element);

	return elements.size() - 1;
}

template <typename T>
static u32 PushElement(ImVector<T>& elements) {
	return PushElement(elements, T{});
}

template <typename T>
static void PopElement(ImVector<T>& elements) {
	elements.pop_back();
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

// TODO: This will cause awfulness if elements are added but there's no room in the array. Need to fix
template <typename T>
static void DrawGenericEditableList(ImVector<T>& elements, ImVector<s32>& selection, const char* labelPrefix, bool selectionLocked = false, void (*drawExtraStuff)(const T&) = nullptr) {
	ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
	for (u32 i = 0; i < elements.size(); i++) {
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
				MoveElementsRange<T>(elements, i + 1, elements.size(), -1, &selection);
				PopElement<T>(elements);
			}
			if (ImGui::Selectable("Insert above")) {
				u32 newInd = PushElement<T>(elements);
				MoveElementsRange<T>(elements, i, newInd, 1, &selection);
				if (!selectionLocked) {
					selection.clear();
					selection.push_back(i);
				}
			}
			if (ImGui::Selectable("Insert below")) {
				u32 newInd = PushElement<T>(elements);
				MoveElementsRange<T>(elements, i + 1, newInd, 1, &selection);
				if (!selectionLocked) {
					selection.clear();
					selection.push_back(i + 1);
				}
			}
			if (ImGui::Selectable("Duplicate")) { // And insert below
				u32 newInd = PushElement<T>(elements, element);
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
				const bool canMove = CanMoveElements(elements.size(), selection, step);

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
static bool DrawTypeSelectionCombo(const char* label, const char* const* const typeNames, size_t typeCount, T& selection, bool allowNone = true) {
	static_assert(std::is_integral<T>::value);
	assert(!allowNone || std::is_signed<T>::value);
	const T oldSelection = selection;

	const char* noSelectionLabel = "NONE";
	const char* noItemsLabel = "NO ITEMS";
	const char* selectedLabel = typeCount > 0 ? (selection >= 0 ? typeNames[selection] : noSelectionLabel) : noItemsLabel;

	if (ImGui::BeginCombo(label, selectedLabel)) {
		if (allowNone && ImGui::Selectable(noSelectionLabel, selection < 0)) {
			selection = T(-1);
		}

		for (size_t i = 0; i < typeCount; i++) {
			ImGui::PushID((s32)i);

			const bool selected = selection == i;
			if (ImGui::Selectable(typeNames[i], selected)) {
				selection = T(i);
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	return oldSelection != selection;
}
#pragma endregion

#pragma region Assets
struct EditedAsset {
	u64 id;
	AssetType type;
	char name[MAX_ASSET_NAME_LENGTH];
	char relativePath[MAX_ASSET_PATH_LENGTH];
	size_t size;
	void* data;

	bool dirty;
	void* userData;
};

struct AssetEditorState {
	std::unordered_map<u64, EditedAsset> editedAssets;
	u64 currentAsset = UUID_NULL;
};

typedef void (*AssetEditorFn)(EditedAsset&);
typedef void (*PopulateAssetEditorDataFn)(EditedAsset&);
typedef void (*ApplyAssetEditorDataFn)(EditedAsset&);
typedef void (*DeleteAssetEditorDataFn)(EditedAsset&);

static EditorRenderBuffer* CreateRenderBuffer(u64 id, size_t size, const void* data) {
	EditorRenderBuffer* pBuffer = Rendering::CreateEditorBuffer(size, data);
	pContext->assetRenderBuffers.emplace(id, pBuffer);
	return pBuffer;
}

static EditorRenderBuffer* GetRenderBuffer(u64 id, AssetType type, size_t dataSize) {
	if (id == UUID_NULL) {
		return nullptr;
	}

	EditorRenderBuffer* pBuffer = nullptr;
	if (pContext->assetRenderBuffers.contains(id)) {
		pBuffer = pContext->assetRenderBuffers.at(id);
	}
	else {
		const void* pData = AssetManager::GetAsset(id, type);
		if (!pData) {
			return nullptr;
		}
		pBuffer = CreateRenderBuffer(id, CHR_SIZE_BYTES, pData);
	}
	return pBuffer;
}

static void UpdateRenderBuffer(u64 id, AssetType type, size_t dataSize, const void* data) {
	if (type != ASSET_TYPE_CHR_BANK && type != ASSET_TYPE_PALETTE) {
		return;
	}

	EditorRenderBuffer* pBuffer = GetRenderBuffer(id, type, dataSize);
	Rendering::UpdateEditorBuffer(pBuffer, data);
}

static EditedAsset CopyAssetForEditing(u64 id, PopulateAssetEditorDataFn populateFn) {
	EditedAsset result{};
	result.id = id;

	const AssetEntry* pAssetInfo = AssetManager::GetAssetInfo(id);
	result.type = pAssetInfo->flags.type;
	strcpy(result.name, pAssetInfo->name);
	strcpy(result.relativePath, pAssetInfo->relativePath);
	result.size = pAssetInfo->size;
	result.data = malloc(pAssetInfo->size);
	memcpy(result.data, AssetManager::GetAsset(id, pAssetInfo->flags.type), pAssetInfo->size);
	result.dirty = false;

	if (populateFn) {
		populateFn(result);
	}
	else result.userData = nullptr;

	return result;
}

static bool ResizeEditedAsset(EditedAsset& asset, size_t newSize) {
	if (newSize > asset.size) {
		void* newData = malloc(newSize);
		if (!newData) {
			return false;
		}
		memcpy(newData, asset.data, asset.size);
		free(asset.data);
		asset.data = newData;
	}
	asset.size = newSize;
	return true;
}

static bool SaveAssetToFile(AssetType type, const std::filesystem::path& relativePath, const char* name, const void* pData, u64 id = UUID_NULL) {
	std::filesystem::path fullPath = std::filesystem::path(ASSETS_SRC_DIR) / relativePath;

	nlohmann::json metadata;
	if (!AssetSerialization::HasMetadata(fullPath)) {
		DEBUG_LOG("Creating metadata file for asset: %s", fullPath.string().c_str());
		if (id == UUID_NULL) {
			id = Random::GenerateUUID();
		}

		if (AssetSerialization::CreateAssetMetadataFile(fullPath, id, metadata) != SERIALIZATION_SUCCESS) {
			DEBUG_ERROR("Failed to create metadata file for asset: %s", fullPath.string().c_str());
			return false;
		}
	}
	else {
		DEBUG_LOG("Updating metadata file for asset: %s", fullPath.string().c_str());
		if (AssetSerialization::LoadAssetMetadataFromFile(fullPath, metadata) != SERIALIZATION_SUCCESS) {
			DEBUG_ERROR("Failed to load metadata file for asset: %s", fullPath.string().c_str());
			return false;
		}
	}

	if (AssetSerialization::SaveAssetToFile(fullPath, name, type, metadata, pData) != SERIALIZATION_SUCCESS) {
		DEBUG_ERROR("Failed to save asset to file: %s", fullPath.string().c_str());
		return false;
	}

	if (AssetSerialization::SaveAssetMetadataToFile(fullPath, metadata) != SERIALIZATION_SUCCESS) {
		DEBUG_ERROR("Failed to save asset metadata to file: %s", fullPath.string().c_str());
		return false;
	}

	return true;
}

static bool SaveEditedAsset(EditedAsset& asset, ApplyAssetEditorDataFn applyFn) {
	if (applyFn) {
		applyFn(asset);
	}
	
	if (!SaveAssetToFile(asset.type, asset.relativePath, asset.name, asset.data, asset.id)) {
		return false;
	}

	AssetEntry* pAssetInfo = AssetManager::GetAssetInfo(asset.id);
	if (pAssetInfo->size != asset.size) {
		if (!AssetManager::ResizeAsset(asset.id, asset.size)) {
			return false;
		}
	}

	memcpy(pAssetInfo->name, asset.name, MAX_ASSET_NAME_LENGTH);
	void* data = AssetManager::GetAsset(asset.id, pAssetInfo->flags.type);
	memcpy(data, asset.data, asset.size);

	UpdateRenderBuffer(pAssetInfo->id, pAssetInfo->flags.type, pAssetInfo->size, data);

	asset.dirty = false;
	return true;
}

static bool RevertEditedAsset(EditedAsset& asset, PopulateAssetEditorDataFn populateFn) {
	const AssetEntry* pAssetInfo = AssetManager::GetAssetInfo(asset.id);
	if (pAssetInfo->size != asset.size) {
		if (!ResizeEditedAsset(asset, pAssetInfo->size)) {
			return false;
		}
	}

	memcpy(asset.name, pAssetInfo->name, MAX_ASSET_NAME_LENGTH);
	const void* data = AssetManager::GetAsset(asset.id, pAssetInfo->flags.type);
	memcpy(asset.data, data, asset.size);
	asset.dirty = false;

	UpdateRenderBuffer(pAssetInfo->id, pAssetInfo->flags.type, pAssetInfo->size, data);

	if (populateFn) {
		populateFn(asset);
	}
	else asset.userData = nullptr;
	return true;
}

static void FreeEditedAsset(EditedAsset& asset, DeleteAssetEditorDataFn deleteFn) {
	free(asset.data);
	asset.data = nullptr;

	if (deleteFn) {
		deleteFn(asset);
	}
	asset.userData = nullptr;
}

static bool DuplicateAsset(u64 id, const std::filesystem::path& path) {
	const AssetEntry* pAssetInfo = AssetManager::GetAssetInfo(id);
	if (!pAssetInfo) {
		return false;
	}

	const void* assetData = AssetManager::GetAsset(id, pAssetInfo->flags.type);
	if (!assetData) {
		return false;
	}

	char newName[MAX_ASSET_NAME_LENGTH];
	snprintf(newName, MAX_ASSET_NAME_LENGTH, "%s (Copy)", pAssetInfo->name);
	const std::filesystem::path relativePath = std::filesystem::relative(path, ASSETS_SRC_DIR);

	const u64 newId = AssetManager::CreateAsset(pAssetInfo->flags.type, pAssetInfo->size, relativePath.string().c_str(), newName);
	if (newId == UUID_NULL) {
		return false;
	}
	void* newData = AssetManager::GetAsset(newId, pAssetInfo->flags.type);
	memcpy(newData, assetData, pAssetInfo->size);

	if (!SaveAssetToFile(pAssetInfo->flags.type, relativePath, newName, assetData, newId)) {
		AssetManager::RemoveAsset(newId);
		return false;
	}

	return true;
}

static bool DrawAssetField(const char* label, AssetType type, u64& selectedId) {
	const u64 oldId = selectedId;

	std::vector<u64> ids;
	std::vector<const char*> assetNames;
	s32 selectedIndex = -1;

	const AssetIndex& assetIndex = AssetManager::GetIndex();
	for (u32 i = 0; i < assetIndex.Count(); i++) {
		PoolHandle<AssetEntry> handle = assetIndex.GetHandle(i);
		const AssetEntry* asset = assetIndex.Get(handle);
		if (!asset) continue;

		if (asset->flags.type != type || asset->flags.deleted) {
			continue;
		}

		if (asset->id == selectedId) {
			selectedIndex = (s32)ids.size();
		}

		ids.push_back(asset->id);
		assetNames.push_back(asset->name);
	}

	if (selectedId != UUID_NULL && selectedIndex < 0) {
		// If the selected ID is not in the list, reset it
		selectedId = UUID_NULL;
		selectedIndex = -1;
	}

	if (DrawTypeSelectionCombo(label, assetNames.data(), assetNames.size(), selectedIndex)) {
		selectedId = selectedIndex >= 0 ? ids[selectedIndex] : UUID_NULL;
	}

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("dd_asset", ImGuiDragDropFlags_AcceptBeforeDelivery))
		{
			const u64 assetId = *(const u64*)payload->Data;

			const AssetEntry* pAssetInfo = AssetManager::GetAssetInfo(assetId);
			const bool isValid = pAssetInfo && pAssetInfo->flags.type == type;

			if (isValid && payload->IsDelivery()) {
				selectedId = assetId;
			}
		}
		ImGui::EndDragDropTarget();
	}

	return oldId != selectedId;
}

static bool DrawAssetFieldN(const char* label, AssetType type, u64* pSelectedIds, s32 components) {
	u64* pCurrentId = pSelectedIds;
	bool valueChanged = false;

	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::BeginGroup();
	ImGui::PushID(label);
	ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());
	for (s32 i = 0; i < components; i++) {
		ImGui::PushID(i);
		if (i > 0)
			ImGui::SameLine(0, style.ItemInnerSpacing.x);
		valueChanged |= DrawAssetField("", type, *pCurrentId);
		ImGui::PopID();
		ImGui::PopItemWidth();
		
		pCurrentId++;
	}
	ImGui::PopID();

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	if (label != label_end)
	{
		ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
		ImGui::TextEx(label, label_end);
	}

	ImGui::EndGroup();
	return valueChanged;
}

static u64 DrawAssetList(AssetType type) {
	u64 result = UUID_NULL;
	ImGui::BeginChild("Asset list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

	const AssetIndex& assetIndex = AssetManager::GetIndex();
	for (u32 i = 0; i < assetIndex.Count(); i++) {
		PoolHandle<AssetEntry> handle = assetIndex.GetHandle(i);
		const AssetEntry* asset = assetIndex.Get(handle);
		if (!asset) continue;

		if (asset->flags.type != type || asset->flags.deleted) {
			continue;
		}

		ImGui::PushID(asset->id);

		ImGui::Selectable(asset->name);

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			result = asset->id;
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			ImGui::SetDragDropPayload("dd_asset", &asset->id, sizeof(u64));
			ImGui::Text("%s", asset->name);

			ImGui::EndDragDropSource();
		}

		ImGui::PopID();
	}

	ImGui::EndChild();
	return result;
}

// TODO: Figure out how to deal with copypasta
static u64 DrawAssetListWithFunctionality(AssetType type, ImGui::FileBrowser& fileBrowser, u64& duplicateAssetId) {
	u64 result = UUID_NULL;
	ImGui::BeginChild("Asset list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

	const AssetIndex& assetIndex = AssetManager::GetIndex();
	for (u32 i = 0; i < assetIndex.Count(); i++) {
		PoolHandle<AssetEntry> handle = assetIndex.GetHandle(i);
		const AssetEntry* asset = assetIndex.Get(handle);
		if (!asset) continue;

		if (asset->flags.type != type || asset->flags.deleted) {
			continue;
		}

		ImGui::PushID(asset->id);

		ImGui::Selectable(asset->name);

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			result = asset->id;
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			ImGui::OpenPopup("AssetPopup");
		}
		if (ImGui::BeginPopup("AssetPopup")) {
			if (ImGui::MenuItem("Delete")) {
				AssetManager::RemoveAsset(asset->id);
				if (pContext->assetRenderBuffers.contains(asset->id)) {
					pContext->bufferEraseList.push_back(asset->id);
				}
			}
			if (ImGui::MenuItem("Duplicate")) {
				duplicateAssetId = asset->id;
				fileBrowser.SetDirectory(ASSETS_SRC_DIR);
				fileBrowser.SetTitle("Create new asset");
				fileBrowser.SetTypeFilters({ ASSET_TYPE_FILE_EXTENSIONS[type] });
				fileBrowser.Open();
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			ImGui::SetDragDropPayload("dd_asset", &asset->id, sizeof(u64));
			ImGui::Text("%s", asset->name);

			ImGui::EndDragDropSource();
		}

		ImGui::PopID();
	}

	ImGui::EndChild();
	return result;
}

static void DrawAssetEditor(const char* title, bool& open, AssetType type, const char* newName, AssetEditorFn drawEditor, AssetEditorState& state, 
	PopulateAssetEditorDataFn populateFn = nullptr, 
	ApplyAssetEditorDataFn applyFn = nullptr, 
	DeleteAssetEditorDataFn deleteFn = nullptr) {
	ImGui::Begin(title, &open, ImGuiWindowFlags_MenuBar);

	static ImGui::FileBrowser fileBrowser(ImGuiFileBrowserFlags_EnterNewFilename);
	static u64 duplicateAssetId = UUID_NULL;

	fileBrowser.Display();
	if (fileBrowser.HasSelected()) {
		if (duplicateAssetId != UUID_NULL) {
			DuplicateAsset(duplicateAssetId, fileBrowser.GetSelected());
		}
		else {
			const std::filesystem::path relativePath = std::filesystem::relative(fileBrowser.GetSelected(), ASSETS_SRC_DIR);
			const u32 newSize = Editor::Assets::GetAssetSize(type, nullptr);
			const u64 id = AssetManager::CreateAsset(type, newSize, relativePath.string().c_str(), newName);
			void* data = AssetManager::GetAsset(id, type);
			Editor::Assets::InitializeAsset(type, data);

			if (!SaveAssetToFile(type, relativePath, newName, data, id)) {
				// Failed to save asset, remove it
				AssetManager::RemoveAsset(id);
			}
			else {
				state.editedAssets.try_emplace(id, CopyAssetForEditing(id, populateFn));
			}
		}
		
		fileBrowser.ClearSelected();
		duplicateAssetId = UUID_NULL;
	}

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Asset"))
		{
			if (ImGui::MenuItem("New")) {
				duplicateAssetId = UUID_NULL;
				fileBrowser.SetDirectory(ASSETS_SRC_DIR);
				fileBrowser.SetTitle("Create new asset");
				fileBrowser.SetTypeFilters({ ASSET_TYPE_FILE_EXTENSIONS[type] });
				fileBrowser.Open();
			}
			ImGui::Separator();
			ImGui::BeginDisabled(!state.editedAssets.contains(state.currentAsset));
			if (ImGui::MenuItem("Save")) {
				EditedAsset& asset = state.editedAssets.at(state.currentAsset);
				SaveEditedAsset(asset, applyFn);
			}
			if (ImGui::MenuItem("Save all")) {
				for (auto& kvp : state.editedAssets) {
					EditedAsset& asset = kvp.second;
					SaveEditedAsset(asset, applyFn);
				}
			}
			if (ImGui::MenuItem("Revert changes")) {
				EditedAsset& asset = state.editedAssets.at(state.currentAsset);
				RevertEditedAsset(asset, populateFn);
			}
			if (ImGui::MenuItem("Revert all")) {
				for (auto& kvp : state.editedAssets) {
					EditedAsset& asset = kvp.second;
					RevertEditedAsset(asset, populateFn);
				}
			}
			ImGui::EndDisabled();
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	const u64 openedAsset = DrawAssetListWithFunctionality(type, fileBrowser, duplicateAssetId);
	if (openedAsset != UUID_NULL && !state.editedAssets.contains(openedAsset)) {
		state.editedAssets.emplace(openedAsset, CopyAssetForEditing(openedAsset, populateFn));
	}
	ImGui::SameLine();

	ImGui::BeginChild("Editor Area");
	if (ImGui::BeginTabBar("Edited assets")) {
		std::vector<u64> eraseList;
		for (auto& kvp : state.editedAssets) {
			auto& asset = kvp.second;

			// Check if deleted
			const AssetEntry* pAssetInfo = AssetManager::GetAssetInfo(kvp.first);
			if (!pAssetInfo || pAssetInfo->flags.deleted) {
				FreeEditedAsset(asset, deleteFn);
				eraseList.push_back(kvp.first);
				continue;
			}

			bool assetOpen = true;

			char label[256];
			if (asset.dirty) {
				sprintf(label, "%s*###%lld", asset.name, kvp.first);
			}
			else {
				sprintf(label, "%s###%lld", asset.name, kvp.first);
			}

			if (ImGui::BeginTabItem(label, &assetOpen)) {
				state.currentAsset = kvp.first;
				drawEditor(asset);
				ImGui::EndTabItem();
			}

			if (!assetOpen) {
				// TODO: Popup that asks if the user is sure they want to close this
				FreeEditedAsset(asset, deleteFn);
				eraseList.push_back(kvp.first);
			}
		}
		for (u64 id : eraseList) {
			state.editedAssets.erase(id);
		}
		eraseList.clear();
		ImGui::EndTabBar();
	}
	ImGui::EndChild();

	ImGui::End();
}
#pragma endregion

#pragma region Tilemap
static bool DrawTilemapTools(TilemapClipboard& clipboard, TilesetHandle& tilesetHandle) {
	const bool result = DrawAssetField("Tileset", ASSET_TYPE_TILESET, tilesetHandle.id);

	ImGuiStyle& style = ImGui::GetStyle();

	const s32 currentSelection = (clipboard.size.x == 1 && clipboard.size.y == 1) ? clipboard.clipboard[0] : -1;
	s32 newSelection = currentSelection;

	const Tileset* pTileset = AssetManager::GetAsset(tilesetHandle);
	if (!pTileset) {
		ImGui::TextUnformatted("Select a tileset to paint tiles.");
		return result;
	}

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

	return result;
}

// Returns true if scrolling view
static bool DrawTilemapEditor(Tilemap* pTilemap, ImVec2 topLeft, r32 renderScale, glm::vec2& viewportPos, TilemapClipboard& clipboard, bool allowEditing) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 size(VIEWPORT_WIDTH_PIXELS * renderScale, VIEWPORT_HEIGHT_PIXELS * renderScale);
	const ImVec2 btmRight(topLeft.x + size.x, topLeft.y + size.y);
	const r32 tileDrawSize = METATILE_DIM_PIXELS * renderScale;

	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 mousePosInViewportCoords = ImVec2((io.MousePos.x - topLeft.x) / tileDrawSize, (io.MousePos.y - topLeft.y) / tileDrawSize);
	const ImVec2 mousePosInWorldCoords = ImVec2(mousePosInViewportCoords.x + viewportPos.x, mousePosInViewportCoords.y + viewportPos.y);
	const ImVec2 hoveredTileWorldPos = ImVec2(glm::floor(mousePosInWorldCoords.x), glm::floor(mousePosInWorldCoords.y));

	// Invisible button to prevent dragging window
	ImGui::InvisibleButton("##canvas", ImVec2(size.x, size.y), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();

	drawList->PushClipRect(topLeft, btmRight, true);

	DrawTilemap(pTilemap, ImVec2(viewportPos.x, viewportPos.y), ImVec2(VIEWPORT_WIDTH_METATILES + 1, VIEWPORT_HEIGHT_METATILES + 1), topLeft, renderScale * METATILE_DIM_PIXELS);

	// View scrolling
	bool scrolling = false;

	static ImVec2 dragStartPos = ImVec2(0, 0);
	static ImVec2 dragDelta = ImVec2(0, 0);

	if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
		dragStartPos = io.MousePos;
	}

	if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		const ImVec2 newDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
		glm::vec2 newViewportPos = viewportPos;
		newViewportPos.x -= (newDelta.x - dragDelta.x) / renderScale / METATILE_DIM_PIXELS;
		newViewportPos.y -= (newDelta.y - dragDelta.y) / renderScale / METATILE_DIM_PIXELS;
		dragDelta = newDelta;

		const glm::vec2 max = {
			pTilemap->width,
			pTilemap->height
		};

		viewportPos = glm::clamp(newViewportPos, glm::vec2(0.0f), max);
		scrolling = true;
	}

	// Reset drag delta when mouse released
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
		dragDelta = ImVec2(0, 0);
	}

	// Editing
	if (allowEditing) {
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
						const s32 tilesetIndex = Tiles::GetTilesetTileIndex(pTilemap, metatileWorldPos);
						clipboard.clipboard[clipboardIndex] = tilesetIndex;
					}
				}
			}
		}

		if (!selecting) {
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

					const Tileset* pTileset = AssetManager::GetAsset(pTilemap->tilesetHandle);
					if (pTileset) {
						const Metatile& metatile = pTileset->tiles[metatileIndex].metatile;
						DrawMetatile(metatile, metatileInPixelCoords, tileDrawSize, IM_COL32(255, 255, 255, 127));

						// Paint metatiles
						if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && active) {
							Tiles::SetTilesetTile(pTilemap, metatileWorldPos, metatileIndex);
						}
					}
				}
			}

			const ImVec2 clipboardTopLeftInPixelCoords = ImVec2((clipboardTopLeft.x - viewportPos.x) * tileDrawSize + topLeft.x, (clipboardTopLeft.y - viewportPos.y) * tileDrawSize + topLeft.y);
			const ImVec2 clipboardBtmRightInPixelCoords = ImVec2((clipboardBtmRight.x - viewportPos.x) * tileDrawSize + topLeft.x, (clipboardBtmRight.y - viewportPos.y) * tileDrawSize + topLeft.y);
			drawList->AddRect(clipboardTopLeftInPixelCoords, clipboardBtmRightInPixelCoords, IM_COL32(255, 255, 255, 255));
		}
	}

	drawList->PopClipRect();
	return scrolling;
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
			const ActorPrototype* pPrototype = Game::GetActorPrototype(pActor);
			if (pPrototype) {
				DrawHitbox(&pPrototype->hitbox, drawPos, renderScale);
			}
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
			else if (strstr(msg, "[warn]")) {
				color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
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
			static u32 selectedPages[2]{};

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

			ImGui::BeginChild("BG page selector", ImVec2(chrWidth, 0));
			DrawCHRPageSelector(chrWidth, 0, selectedPalettes[0], nullptr);
			ImGui::EndChild();
			ImGui::SameLine();
			ImGui::BeginChild("FG page selector", ImVec2(chrWidth, 0));
			DrawCHRPageSelector(chrWidth, 1, selectedPalettes[1], nullptr);
			ImGui::EndChild();
			
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Palette")) {
			constexpr ImVec2 size = ImVec2(512, 256);
			DrawColorGrid(size);

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
struct MetaspriteEditorData {
	ImVector<Sprite> sprites;
};

static void PopulateMetaspriteEditorData(EditedAsset& editedAsset) {
	if (editedAsset.userData) {
		delete editedAsset.userData;
	}

	editedAsset.userData = new MetaspriteEditorData();
	MetaspriteEditorData* pEditorData = (MetaspriteEditorData*)editedAsset.userData;

	const Metasprite* pMetasprite = (Metasprite*)editedAsset.data;
	const Sprite* pSprites = pMetasprite->GetSprites();
	for (u32 i = 0; i < pMetasprite->spriteCount; i++) {
		pEditorData->sprites.push_back(pSprites[i]);
	}
}

static void ApplyMetaspriteEditorData(EditedAsset& editedAsset) {
	const MetaspriteEditorData* pEditorData = (MetaspriteEditorData*)editedAsset.userData;
	Metasprite* pMetasprite = (Metasprite*)editedAsset.data;
	pMetasprite->spriteCount = pEditorData->sprites.size();
	const u32 newSize = Editor::Assets::GetAssetSize(editedAsset.type, pMetasprite);
	ResizeEditedAsset(editedAsset, newSize);

	// Reassign in case the asset was resized
	pMetasprite = (Metasprite*)editedAsset.data;
	Sprite* pSprites = pMetasprite->GetSprites();
	for (u32 i = 0; i < pMetasprite->spriteCount; i++) {
		pSprites[i] = pEditorData->sprites[i];
	}
}

static void DeleteMetaspriteEditorData(EditedAsset& editedAsset) {
	delete editedAsset.userData;
}


static void DrawMetaspritePreview(MetaspriteEditorData* pEditorData, ImVector<s32>& spriteSelection, bool selectionLocked, r32 size) {
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

	for (s32 i = pEditorData->sprites.size() - 1; i >= 0; i--) {
		Sprite& sprite = pEditorData->sprites[i];

		const u8 index = (u8)sprite.tileId;
		const bool flipX = sprite.flipHorizontal;
		const bool flipY = sprite.flipVertical;
		const u8 palette = sprite.palette;
		const u8 pageIndex = (sprite.tileId >> 8) & 3;

		glm::vec4 uvMinMax = ChrTileToTexCoord(index, pageIndex + CHR_PAGE_COUNT, palette);
		ImVec2 pos = ImVec2(origin.x + renderScale * sprite.x, origin.y + renderScale * sprite.y);

		// Select sprite by clicking (Topmost sprite gets selected)
		bool spriteClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x >= pos.x && io.MousePos.x < pos.x + gridStepPixels && io.MousePos.y >= pos.y && io.MousePos.y < pos.y + gridStepPixels;
		if (spriteClicked) {
			trySelect = i;
		}

		bool selected = spriteSelection.contains(i);
		// Move sprite if dragged
		ImVec2 posWithDrag = selected ? ImVec2(pos.x + dragDelta.x, pos.y + dragDelta.y) : pos;

		drawList->AddImage(GetTextureID(pContext->pChrTexture), posWithDrag, ImVec2(posWithDrag.x + gridStepPixels, posWithDrag.y + gridStepPixels), ImVec2(flipX ? uvMinMax.z : uvMinMax.x, flipY ? uvMinMax.w : uvMinMax.y), ImVec2(!flipX ? uvMinMax.z : uvMinMax.x, !flipY ? uvMinMax.w : uvMinMax.y));


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

static void DrawSpriteEditor(MetaspriteEditorData* pEditorData, ImVector<s32>& spriteSelection, bool& dirty) {
	ImGui::SeparatorText("Sprite editor");
	
	if (spriteSelection.empty()) {
		ImGui::TextUnformatted("No sprite selected");
	}
	else if (spriteSelection.size() > 1) {
		ImGui::TextUnformatted("Multiple sprites selected");
	}
	else {
		s32& spriteIndex = spriteSelection[0];
		Sprite& sprite = pEditorData->sprites[spriteIndex];
		s32 index = (s32)sprite.tileId;

		bool flipX = sprite.flipHorizontal;
		bool flipY = sprite.flipVertical;

		s32 newId = (s32)sprite.tileId;
		r32 chrSheetSize = 256;

		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::BeginChild("Tile selector", ImVec2(chrSheetSize, chrSheetSize + ImGui::GetItemRectSize().y + style.ItemSpacing.y));
		DrawCHRPageSelector(chrSheetSize, 1, sprite.palette, &newId);
		ImGui::EndChild();

		if (newId != sprite.tileId) {
			sprite.tileId = (u16)newId;
			dirty = true;
		}

		ImGui::SameLine();
		ImGui::BeginChild("sprite palette", ImVec2(0, chrSheetSize));
		{
			for (int i = 0; i < FG_PALETTE_COUNT; i++) {
				ImGui::PushID(i);
				if (DrawPaletteButton(i + BG_PALETTE_COUNT)) {
					sprite.palette = i;
					dirty = true;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		ImGui::Text("Position: (%d, %d)", sprite.x, sprite.y);

		if (ImGui::Checkbox("Flip horizontal", &flipX)) {
			sprite.flipHorizontal = flipX;
			dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Flip vertical", &flipY)) {
			sprite.flipVertical = flipY;
			dirty = true;
		}
	}
}

static void DrawSpriteListPreview(const Sprite& sprite) {
	// Draw a nice little preview of the sprite
	u8 index = (u8)sprite.tileId;
	const bool flipX = sprite.flipHorizontal;
	const bool flipY = sprite.flipVertical;
	const u8 pageIndex = (sprite.tileId >> 8) & 3;
	glm::vec4 uvMinMax = ChrTileToTexCoord(index, pageIndex + CHR_PAGE_COUNT, sprite.palette);
	ImGuiStyle& style = ImGui::GetStyle();
	r32 itemHeight = ImGui::GetItemRectSize().y - style.FramePadding.y;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	const ImVec2 topLeft = ImVec2(ImGui::GetItemRectMin().x + 88, ImGui::GetItemRectMin().y + style.FramePadding.y);
	const ImVec2 btmRight = ImVec2(topLeft.x + itemHeight, topLeft.y + itemHeight);
	drawList->AddImage(GetTextureID(pContext->pChrTexture), topLeft, btmRight, ImVec2(flipX ? uvMinMax.z : uvMinMax.x, flipY ? uvMinMax.w : uvMinMax.y), ImVec2(!flipX ? uvMinMax.z : uvMinMax.x, !flipY ? uvMinMax.w : uvMinMax.y));
}

static void DrawMetaspriteEditor(EditedAsset& asset) {
	ImGui::BeginChild("Metasprite editor");

	Metasprite* pMetasprite = (Metasprite*)asset.data;
	MetaspriteEditorData* pEditorData = (MetaspriteEditorData*)asset.userData;

	static ImVector<s32> spriteSelection;
	static bool selectionLocked = false;

	static bool showColliderPreview = false;

	ImGui::SeparatorText("Properties");
	if (ImGui::InputText("Name", asset.name, MAX_ASSET_NAME_LENGTH)) {
		asset.dirty = true;
	}

	ImGui::Separator();

	constexpr r32 previewSize = 256;
	ImGui::BeginChild("Metasprite preview", ImVec2(previewSize, previewSize));
	DrawMetaspritePreview(pEditorData, spriteSelection, selectionLocked, previewSize);
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::BeginChild("Metasprite properties");
	ImGui::Checkbox("Lock selection", &selectionLocked);

	if (ImGui::Button("+")) {
		pEditorData->sprites.push_back(Sprite{});
		asset.dirty = true;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(pEditorData->sprites.size() == 0);
	if (ImGui::Button("-")) {
		pEditorData->sprites.pop_back();
		asset.dirty = true;
	}
	ImGui::EndDisabled();

	ImGui::BeginChild("Sprite list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);

	DrawGenericEditableList<Sprite>(pEditorData->sprites, spriteSelection, "Sprite", selectionLocked, DrawSpriteListPreview);

	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("Sprite editor");
	DrawSpriteEditor(pEditorData, spriteSelection, asset.dirty);
	ImGui::EndChild();
	ImGui::EndChild();
	ImGui::EndChild();
}

static void DrawSpriteWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Metasprite editor", pContext->spriteWindowOpen, ASSET_TYPE_METASPRITE, "New Metasprite", DrawMetaspriteEditor, state, PopulateMetaspriteEditorData, ApplyMetaspriteEditorData, DeleteMetaspriteEditorData);
}
#pragma endregion

#pragma region Tileset
static void DrawTilesetEditor(EditedAsset& asset) {
	ImGui::BeginChild("Tileset editor");

	static s32 selectedMetatileIndex = 0;
	static s32 selectedTileIndex = 0;

	constexpr s32 renderScale = 2;
	constexpr s32 gridSizeTiles = 16;
	constexpr s32 gridStepPixels = METATILE_DIM_PIXELS * renderScale;
	constexpr s32 gridSizePixels = gridSizeTiles * gridStepPixels;

	Tileset* pTileset = (Tileset*)asset.data;

	DrawTileset(pTileset, gridSizePixels, &selectedMetatileIndex);

	ImGui::SameLine();

	ImGui::BeginChild("Editor tools");
	{
		ImGui::SeparatorText("Properties");
		if (ImGui::InputText("Name", asset.name, MAX_ASSET_NAME_LENGTH)) {
			asset.dirty = true;
		}

		ImGui::SeparatorText("Metatile editor");

		constexpr r32 tilePreviewSize = 64;
		constexpr r32 pixelSize = tilePreviewSize / TILE_DIM_PIXELS;

		Metatile& metatile = pTileset->tiles[selectedMetatileIndex].metatile;
		s32 tileId = metatile.tiles[selectedTileIndex].tileId;

		s32 palette = metatile.tiles[selectedTileIndex].palette;
		r32 chrSheetSize = 256;
		DrawCHRPageSelector(chrSheetSize, 0, palette, &tileId);
		if (tileId != metatile.tiles[selectedTileIndex].tileId) {
			metatile.tiles[selectedTileIndex].tileId = tileId;
			asset.dirty = true;
		}

		ImGui::Text("0x%02x", selectedMetatileIndex);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 gridSize = ImVec2(tilePreviewSize, tilePreviewSize);
		const ImVec2 tilePos = DrawTileGrid(gridSize, gridStepPixels, &selectedTileIndex);
		DrawMetatile(metatile, tilePos, tilePreviewSize);
		DrawTileGridSelection(tilePos, gridSize, gridStepPixels, selectedTileIndex);

		s32& type = pTileset->tiles[selectedMetatileIndex].type;
		ImGui::SliderInt("Type", &type, 0, TILE_TYPE_COUNT - 1, METATILE_TYPE_NAMES[type]);

		ImGui::SeparatorText("Tile Settings");

		ImGui::Text("Tile ID: 0x%02x", tileId);

		if (ImGui::SliderInt("Palette", &palette, 0, BG_PALETTE_COUNT - 1)) {
			metatile.tiles[selectedTileIndex].palette = palette;
			asset.dirty = true;
		}

		bool flipHorizontal = metatile.tiles[selectedTileIndex].flipHorizontal;
		if (ImGui::Checkbox("Flip Horizontal", &flipHorizontal)) {
			metatile.tiles[selectedTileIndex].flipHorizontal = flipHorizontal;
			asset.dirty = true;
		}

		bool flipVertical = metatile.tiles[selectedTileIndex].flipVertical;
		if (ImGui::Checkbox("Flip Vertical", &flipVertical)) {
			metatile.tiles[selectedTileIndex].flipVertical = flipVertical;
			asset.dirty = true;
		}
	}
	ImGui::EndChild();

	ImGui::EndChild();
}

static void DrawTilesetWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Tileset editor", pContext->tilesetWindowOpen, ASSET_TYPE_TILESET, "New Tileset", DrawTilesetEditor, state);
}
#pragma endregion

#pragma region Room editor
struct RoomEditorData {
	Pool<RoomActor, 1024> actors;
	PoolHandle<RoomActor> selectedActorHandle = PoolHandle<RoomActor>::Null();

	u32 editMode = ROOM_EDIT_MODE_NONE;
	TilemapClipboard clipboard{};
	RoomToolsState toolsState{};
	glm::vec2 viewportPos = glm::vec2(0.0f);
};

static void PopulateRoomEditorData(EditedAsset& editedAsset) {
	if (editedAsset.userData) {
		delete editedAsset.userData;
	}

	editedAsset.userData = new RoomEditorData();
	RoomEditorData* pEditorData = (RoomEditorData*)editedAsset.userData;

	const RoomTemplate* pTemplate = (RoomTemplate*)editedAsset.data;
	const RoomActor* pActors = pTemplate->GetActors();
	for (u32 i = 0; i < pTemplate->actorCount; i++) {
		pEditorData->actors.Add(pActors[i]);
	}
}

static void ApplyRoomEditorData(EditedAsset& editedAsset) {
	const RoomEditorData* pEditorData = (RoomEditorData*)editedAsset.userData;
	RoomTemplate* pTemplate = (RoomTemplate*)editedAsset.data;
	pTemplate->actorCount = pEditorData->actors.Count();
	const u32 newSize = Editor::Assets::GetAssetSize(editedAsset.type, pTemplate);
	ResizeEditedAsset(editedAsset, newSize);

	// Reassign in case the asset was resized
	pTemplate = (RoomTemplate*)editedAsset.data;
	RoomActor* pActors = pTemplate->GetActors();
	for (u32 i = 0; i < pTemplate->actorCount; i++) {
		pActors[i] = *pEditorData->actors.Get(pEditorData->actors.GetHandle(i));
	}
}

static void DeleteRoomEditorData(EditedAsset& editedAsset) {
	delete editedAsset.userData;
}

static void DrawScreenBorders(u32 index, ImVec2 pMin, ImVec2 pMax, r32 renderScale) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	
	static char screenLabelText[16];
	snprintf(screenLabelText, 16, "%#04x", index);

	drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255));

	const ImVec2 textPos = ImVec2(pMin.x + TILE_DIM_PIXELS * renderScale, pMin.y + TILE_DIM_PIXELS * renderScale);
	drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), screenLabelText);
}

static void DrawRoomOverlay(const glm::vec2& viewportPos, const ImVec2 topLeft, const ImVec2 btmRight, const r32 renderScale) {
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

static PoolHandle<RoomActor> GetHoveredActorHandle(const RoomEditorData* pEditorData, const ImVec2& mousePosInWorldCoords) {
	auto result = PoolHandle<RoomActor>::Null();
	// TODO: Some quadtree action needed desperately
	for (u32 i = 0; i < pEditorData->actors.Count(); i++) {
		PoolHandle<RoomActor> handle = pEditorData->actors.GetHandle(i);
		const RoomActor* pActor = pEditorData->actors.Get(handle);

		const AABB bounds = GetActorBoundingBox(pActor);
		if (Collision::PointInsideBox({ mousePosInWorldCoords.x, mousePosInWorldCoords.y }, bounds, pActor->position)) {
			result = handle;
			break;
		}
	}
	return result;
}

static void DrawRoomView(EditedAsset& asset) {
	RoomTemplate* pHeader = (RoomTemplate*)asset.data;
	RoomEditorData* pEditorData = (RoomEditorData*)asset.userData;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 topLeft = ImGui::GetCursorScreenPos();
	static const r32 aspectRatio = (r32)VIEWPORT_WIDTH_PIXELS / VIEWPORT_HEIGHT_PIXELS;
	const r32 contentWidth = ImGui::GetContentRegionAvail().x;
	const r32 contentHeight = contentWidth / aspectRatio;
	const r32 renderScale = contentWidth / VIEWPORT_WIDTH_PIXELS;
	ImVec2 btmRight = ImVec2(topLeft.x + contentWidth, topLeft.y + contentHeight);
	const r32 tileDrawSize = METATILE_DIM_PIXELS * renderScale;
	
	const bool scrolling = DrawTilemapEditor(&pHeader->tilemap, topLeft, renderScale, pEditorData->viewportPos, pEditorData->clipboard, pEditorData->editMode == ROOM_EDIT_MODE_TILES);

	// Clamp scrolling to room size
	const glm::vec2 scrollMax = {
			(pHeader->width - 1) * VIEWPORT_WIDTH_METATILES,
			(pHeader->height - 1) * VIEWPORT_HEIGHT_METATILES
	};

	pEditorData->viewportPos = glm::clamp(pEditorData->viewportPos, glm::vec2(0.0f), scrollMax);

	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();

	// Context menu handling
	if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		if (pEditorData->editMode == ROOM_EDIT_MODE_ACTORS) {
			ImGui::OpenPopup("ActorContextMenu");
		}
	}

	drawList->PushClipRect(topLeft, btmRight, true);
	DrawRoomOverlay(pEditorData->viewportPos, topLeft, btmRight, renderScale);

	// Draw actors
	const glm::vec2 viewportPixelPos = pEditorData->viewportPos * r32(METATILE_DIM_PIXELS);
	for (u32 i = 0; i < pEditorData->actors.Count(); i++)
	{
		PoolHandle<RoomActor> handle = pEditorData->actors.GetHandle(i);
		const RoomActor* pActor = pEditorData->actors.Get(handle);

		const glm::vec2 actorPixelPos = pActor->position * (r32)METATILE_DIM_PIXELS;
		const glm::vec2 pixelOffset = actorPixelPos - viewportPixelPos;
		const ImVec2 drawPos = ImVec2(topLeft.x + pixelOffset.x * renderScale, topLeft.y + pixelOffset.y * renderScale);

		const u8 opacity = pEditorData->editMode == ROOM_EDIT_MODE_ACTORS ? 255 : 80;

		const ActorPrototype* pPrototype = AssetManager::GetAsset(pActor->prototypeHandle);
		if (!pPrototype) {
			// Draw a placeholder thingy
			drawList->AddText(drawPos, IM_COL32(255, 0, 0, opacity), "ERROR");
			continue;
		}

		DrawActor(pPrototype, drawPos, renderScale, 0, 0, IM_COL32(255, 255, 255, opacity));
	}

	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 mousePosInViewportCoords = ImVec2((io.MousePos.x - topLeft.x) / tileDrawSize, (io.MousePos.y - topLeft.y) / tileDrawSize);
	const ImVec2 mousePosInWorldCoords = ImVec2(mousePosInViewportCoords.x + pEditorData->viewportPos.x, mousePosInViewportCoords.y + pEditorData->viewportPos.y);

	if (pEditorData->editMode == ROOM_EDIT_MODE_ACTORS) {
		RoomActor* pActor = pEditorData->actors.Get(pEditorData->selectedActorHandle);
		const AABB actorBounds = GetActorBoundingBox(pActor);
		PoolHandle<RoomActor> hoveredActorHandle = GetHoveredActorHandle(pEditorData, mousePosInWorldCoords);

		// Selection
		if (!scrolling) {
			static glm::vec2 selectionStartPos{};

			if (active && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
				pEditorData->selectedActorHandle = hoveredActorHandle;
				selectionStartPos = { mousePosInWorldCoords.x, mousePosInWorldCoords.y };
			}

			if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && pActor != nullptr) {
				const ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
				const glm::vec2 deltaInWorldCoords = { dragDelta.x / tileDrawSize, dragDelta.y / tileDrawSize };

				pActor->position = selectionStartPos + deltaInWorldCoords;
			}
		}

		if (ImGui::BeginPopup("ActorContextMenu")) {
			if (pEditorData->selectedActorHandle != PoolHandle<RoomActor>::Null()) {
				if (ImGui::MenuItem("Remove actor")) {
					pEditorData->actors.Remove(pEditorData->selectedActorHandle);

					pHeader->actorCount--;
					asset.dirty = true;
				}
			}
			else if (ImGui::MenuItem("Add actor")) {
				PoolHandle<RoomActor> handle = pEditorData->actors.Add();
				RoomActor* pNewActor = pEditorData->actors.Get(handle);
				pNewActor->prototypeHandle = ActorPrototypeHandle::Null();
				pNewActor->id = Random::GenerateUUID32();
				pNewActor->position = { mousePosInWorldCoords.x, mousePosInWorldCoords.y };

				pHeader->actorCount++;
				asset.dirty = true;
			}
			ImGui::EndPopup();
		}

		if (pActor != nullptr) {
			const AABB boundsAbs(actorBounds.min + pActor->position, actorBounds.max + pActor->position);
			const ImVec2 pMin = ImVec2((boundsAbs.min.x - pEditorData->viewportPos.x) * tileDrawSize + topLeft.x, (boundsAbs.min.y - pEditorData->viewportPos.y) * tileDrawSize + topLeft.y);
			const ImVec2 pMax = ImVec2((boundsAbs.max.x - pEditorData->viewportPos.x) * tileDrawSize + topLeft.x, (boundsAbs.max.y - pEditorData->viewportPos.y) * tileDrawSize + topLeft.y);

			drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255));
		}
	}

	drawList->PopClipRect();
}

static void DrawRoomTools(EditedAsset& asset) {
	RoomTemplate* pTemplate = (RoomTemplate*)asset.data;
	RoomEditorData* pEditorData = (RoomEditorData*)asset.userData;

	// Reset edit mode, it will be set by the tools window
	pEditorData->editMode = ROOM_EDIT_MODE_NONE;

	const ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs;

	if (ImGui::BeginTabBar("Room tool tabs")) {
		if (ImGui::BeginTabItem("Properties")) {
			pEditorData->editMode = ROOM_EDIT_MODE_NONE;

			if (ImGui::InputText("Name", asset.name, MAX_ASSET_NAME_LENGTH)) {
				asset.dirty = true;
			}

			s32 size[2] = { pTemplate->width, pTemplate->height };
			if (ImGui::InputInt2("Size", size)) {
				pTemplate->width = glm::clamp(size[0], 1, s32(ROOM_MAX_DIM_SCREENS));
				pTemplate->height = glm::clamp(size[1], 1, s32(ROOM_MAX_DIM_SCREENS));
				asset.dirty = true;
			}

			ImGui::SeparatorText("Map visuals");

			static s32 selectedTile = 0;
			if (selectedTile >= pTemplate->width * 2 * pTemplate->height) {
				selectedTile = 0;
			}
			const s32 xTile = selectedTile % (pTemplate->width * 2);
			const s32 yTile = selectedTile / (pTemplate->width * 2);
			const s32 roomTileIndex = xTile + yTile * ROOM_MAX_DIM_SCREENS * 2;

			BgTile* mapTiles = pTemplate->GetMapTiles();

			s32 tileId = mapTiles[roomTileIndex].tileId;
			s32 palette = mapTiles[roomTileIndex].palette;

			constexpr r32 chrSheetSize = 256;
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::BeginChild("Tile selector", ImVec2(chrSheetSize, chrSheetSize + ImGui::GetItemRectSize().y + style.ItemSpacing.y));
			if (DrawCHRPageSelector(chrSheetSize, 0, palette, &tileId)) {
				mapTiles[roomTileIndex].tileId = tileId;
				asset.dirty = true;
			}
			ImGui::EndChild();

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			constexpr r32 previewTileSize = 32;
			const ImVec2 gridSize(previewTileSize * pTemplate->width * 2, previewTileSize * pTemplate->height);

			static bool gridFocused = false;
			const ImVec2 gridPos = DrawTileGrid(gridSize, previewTileSize, &selectedTile, &gridFocused);

			for (u32 y = 0; y < pTemplate->height; y++) {
				for (u32 x = 0; x < pTemplate->width * 2; x++) {
					const s32 tileIndex = x + y * ROOM_MAX_DIM_SCREENS * 2;
					const BgTile& tile = mapTiles[tileIndex];

					const u8 index = (u8)tile.tileId;
					const bool flipX = tile.flipHorizontal;
					const bool flipY = tile.flipVertical;
					const u8 pageIndex = (tile.tileId >> 8) & 3;

					glm::vec4 uvMinMax = ChrTileToTexCoord(index, pageIndex, tile.palette);

					const ImVec2 pos(gridPos.x + x * previewTileSize, gridPos.y + y * previewTileSize);
					drawList->AddImage(GetTextureID(pContext->pChrTexture), pos, ImVec2(pos.x + previewTileSize, pos.y + previewTileSize), ImVec2(flipX ? uvMinMax.z : uvMinMax.x, flipY ? uvMinMax.w : uvMinMax.y), ImVec2(!flipX ? uvMinMax.z : uvMinMax.x, !flipY ? uvMinMax.w : uvMinMax.y));
				}
			}

			DrawTileGridSelection(gridPos, gridSize, previewTileSize, selectedTile);

			ImGui::SeparatorText("Tile Settings");

			ImGui::Text("Tile ID: 0x%02x", tileId);

			if (ImGui::SliderInt("Palette", &palette, 0, BG_PALETTE_COUNT - 1)) {
				mapTiles[roomTileIndex].palette = palette;
				asset.dirty = true;
			}

			bool flipHorizontal = mapTiles[roomTileIndex].flipHorizontal;
			if (ImGui::Checkbox("Flip Horizontal", &flipHorizontal)) {
				mapTiles[roomTileIndex].flipHorizontal = flipHorizontal;
				asset.dirty = true;
			}

			bool flipVertical = mapTiles[roomTileIndex].flipVertical;
			if (ImGui::Checkbox("Flip Vertical", &flipVertical)) {
				mapTiles[roomTileIndex].flipVertical = flipVertical;
				asset.dirty = true;
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Tilemap")) {
			pEditorData->editMode = ROOM_EDIT_MODE_TILES;
			if (DrawTilemapTools(pEditorData->clipboard, pTemplate->tilemap.tilesetHandle)) {
				asset.dirty = true;
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Actors")) {
			pEditorData->editMode = ROOM_EDIT_MODE_ACTORS;

			RoomActor* pActor = pEditorData->actors.Get(pEditorData->selectedActorHandle);
			if (pActor == nullptr) {
				ImGui::Text("No actor selected");
			}
			else {
				ImGui::PushID(pEditorData->selectedActorHandle.Raw());

				ImGui::Text("UUID: %lu", pActor->id);

				if (DrawAssetField("Prototype", ASSET_TYPE_ACTOR_PROTOTYPE, pActor->prototypeHandle.id)) {
					asset.dirty = true;
				}

				ImGui::Separator();

				if (ImGui::InputFloat2("Position", (r32*)&pActor->position)) {
					asset.dirty = true;
				}

				ImGui::PopID();
			}

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

static void DrawRoomEditor(EditedAsset& asset) {
	ImGui::BeginChild("Room editor");

	const r32 toolWindowWidth = 350.0f;
	ImGuiStyle& style = ImGui::GetStyle();
	const r32 gameViewWidth = ImGui::GetContentRegionAvail().x - style.WindowPadding.x - toolWindowWidth;

	ImGui::BeginChild("Game view", ImVec2(gameViewWidth, 0));
	ImGui::NewLine();
	DrawRoomView(asset);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("Room tools", ImVec2(toolWindowWidth, 0));
	DrawRoomTools(asset);
	ImGui::EndChild();

	ImGui::EndChild();
}

static void DrawRoomWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Room editor", pContext->roomWindowOpen, ASSET_TYPE_ROOM_TEMPLATE, "New Room", DrawRoomEditor, state, PopulateRoomEditorData, ApplyRoomEditorData, DeleteRoomEditorData);
}
#pragma endregion

#pragma region Actor prototypes
struct ActorPrototypeEditorData {
	ImVector<AnimationHandle> animations;
	ImVector<s32> selectedAnims;
	s32 currentAnim;
};

static void PopulateActorEditorData(EditedAsset& editedAsset) {
	if (editedAsset.userData) {
		delete editedAsset.userData;
	}

	editedAsset.userData = new ActorPrototypeEditorData();
	ActorPrototypeEditorData* pEditorData = (ActorPrototypeEditorData*)editedAsset.userData;
	pEditorData->currentAnim = -1;

	ActorPrototype* pPrototype = (ActorPrototype*)editedAsset.data;
	for (u32 i = 0; i < pPrototype->animCount; i++) {
		pEditorData->animations.push_back(pPrototype->GetAnimations()[i]);
	}
}

static void ApplyActorEditorData(EditedAsset& editedAsset) {
	ActorPrototypeEditorData* pEditorData = (ActorPrototypeEditorData*)editedAsset.userData;
	ActorPrototype* pPrototype = (ActorPrototype*)editedAsset.data;

	pPrototype->animCount = pEditorData->animations.size();
	const u32 newSize = Editor::Assets::GetAssetSize(editedAsset.type, pPrototype);
	ResizeEditedAsset(editedAsset, newSize);

	// Reassign in case the asset was resized
	pPrototype = (ActorPrototype*)editedAsset.data;
	for (u32 i = 0; i < pPrototype->animCount; i++) {
		pPrototype->GetAnimations()[i] = pEditorData->animations[i];
	}
}

static void DeleteActorEditorData(EditedAsset& editedAsset) {
	delete editedAsset.userData;
}

static ImVec2 DrawActorPreview(const ActorPrototypeEditorData* pEditorData, s32 animIndex, s32 frameIndex, r32 size) {
	constexpr s32 gridSizeTiles = 8;

	const r32 renderScale = size / (gridSizeTiles * TILE_DIM_PIXELS);
	const r32 gridStepPixels = TILE_DIM_PIXELS * renderScale;
	ImVec2 gridPos = DrawTileGrid(ImVec2(size, size), gridStepPixels);
	ImVec2 origin = ImVec2(gridPos.x + size / 2, gridPos.y + size / 2);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddLine(ImVec2(origin.x - 10, origin.y), ImVec2(origin.x + 10, origin.y), IM_COL32(200, 200, 200, 255));
	drawList->AddLine(ImVec2(origin.x, origin.y - 10), ImVec2(origin.x, origin.y + 10), IM_COL32(200, 200, 200, 255));

	if (animIndex < 0 || animIndex >= pEditorData->animations.size()) {
		return gridPos; // No animations to draw
	}

	const AnimationHandle& animHandle = pEditorData->animations[animIndex];
	DrawAnimFrame(animHandle, origin, renderScale, frameIndex);

	return gridPos;
}

static ImGuiDataType DataTypeToImGui(DataType dataType) {
	switch (dataType) {
	case DATA_TYPE_S8:     return ImGuiDataType_S8;
	case DATA_TYPE_U8:     return ImGuiDataType_U8;
	case DATA_TYPE_S16:    return ImGuiDataType_S16;
	case DATA_TYPE_U16:    return ImGuiDataType_U16;
	case DATA_TYPE_S32:    return ImGuiDataType_S32;
	case DATA_TYPE_U32:    return ImGuiDataType_U32;
	case DATA_TYPE_S64:    return ImGuiDataType_S64;
	case DATA_TYPE_U64:    return ImGuiDataType_U64;
	case DATA_TYPE_R32:    return ImGuiDataType_Float;
	case DATA_TYPE_R64:    return ImGuiDataType_Double;
	case DATA_TYPE_BOOL:   return ImGuiDataType_Bool;
	default:               return ImGuiDataType_COUNT; // Invalid type
	}
}

static DataType ImGuiToDataType(ImGuiDataType imguiType) {
	switch (imguiType) {
	case ImGuiDataType_S8:     return DATA_TYPE_S8;
	case ImGuiDataType_U8:     return DATA_TYPE_U8;
	case ImGuiDataType_S16:    return DATA_TYPE_S16;
	case ImGuiDataType_U16:    return DATA_TYPE_U16;
	case ImGuiDataType_S32:    return DATA_TYPE_S32;
	case ImGuiDataType_U32:    return DATA_TYPE_U32;
	case ImGuiDataType_S64:    return DATA_TYPE_S64;
	case ImGuiDataType_U64:    return DATA_TYPE_U64;
	case ImGuiDataType_Float:  return DATA_TYPE_R32;
	case ImGuiDataType_Double: return DATA_TYPE_R64;
	case ImGuiDataType_Bool:   return DATA_TYPE_BOOL;
	default:                   return DATA_TYPE_COUNT; // Invalid type
	}
}

static bool DrawActorPrototypeProperty(const ActorProperty& property, ActorPrototypeData& data) {
	void* propertyData = (u8*)&data + property.offset;
	const ImGuiDataType imguiDataType = DataTypeToImGui(property.dataType);
	bool result = false;

	switch (property.type) {
	case ACTOR_PROPERTY_SCALAR: {
		result = ImGui::InputScalarN(property.name, imguiDataType, propertyData, property.components);
		break;
	}
	case ACTOR_PROPERTY_ASSET: {
		u64* assetIds = (u64*)propertyData;
		result = DrawAssetFieldN(property.name, property.assetType, assetIds, property.components);
		break;
	}
	default: {
		break;
	}
	}

	return result;
}

static void DrawActorEditor(EditedAsset& asset) {
	ImGui::BeginChild("Prototype editor");
	{
		static bool showHitboxPreview = false;

		ActorPrototype* pPrototype = (ActorPrototype*)asset.data;
		ActorPrototypeEditorData* pEditorData = (ActorPrototypeEditorData*)asset.userData;

		ImGui::SeparatorText("Properties");

		if (ImGui::InputText("Name", asset.name, MAX_ASSET_NAME_LENGTH)) {
			asset.dirty = true;
		}

		ImGui::Separator();

		constexpr r32 previewSize = 256;
		ImGui::BeginChild("Actor preview", ImVec2(previewSize, previewSize));

		// TODO: Play back anim
		const s32 currentFrame = 0;

		ImVec2 metaspriteGridPos = DrawActorPreview(pEditorData, pEditorData->currentAnim, currentFrame, previewSize);

		if (showHitboxPreview) {
			AABB& hitbox = pPrototype->hitbox;

			const r32 gridSizeTiles = 8;
			const r32 renderScale = previewSize / (gridSizeTiles * TILE_DIM_PIXELS);

			ImVec2 origin = ImVec2(metaspriteGridPos.x + previewSize / 2, metaspriteGridPos.y + previewSize / 2);

			DrawHitbox(&hitbox, origin, renderScale);
		}
		ImGui::EndChild();

		if (ImGui::BeginTabBar("Actor editor tabs")) {
			if (ImGui::BeginTabItem("Behaviour")) {
				if (DrawTypeSelectionCombo("Type", ACTOR_TYPE_NAMES, ACTOR_TYPE_COUNT, pPrototype->type)) {
					asset.dirty = true;
				}
				const auto& editorData = ACTOR_REFLECTION_DATA[pPrototype->type];

				size_t subtypeCount = editorData.subtypeCount;
				pPrototype->subtype = glm::clamp<TActorSubtype>(pPrototype->subtype, u16(0), u16(subtypeCount - 1));
				if (DrawTypeSelectionCombo("Subtype", editorData.subtypeNames, subtypeCount, pPrototype->subtype)) {
					asset.dirty = true;
				}

				ImGui::SeparatorText("Type data");

				size_t propCount = editorData.propertyCounts[pPrototype->subtype];
				for (size_t i = 0; i < propCount; i++) {
					if (DrawActorPrototypeProperty(editorData.subtypeProperties[pPrototype->subtype][i], pPrototype->data)) {
						asset.dirty = true;
					}
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Animations")) {
				if (ImGui::Button("+")) {
					pEditorData->animations.push_back(AnimationHandle{});
				}
				ImGui::SameLine();
				ImGui::BeginDisabled(pEditorData->animations.size() == 0);
				if (ImGui::Button("-")) {
					pEditorData->animations.pop_back();
				}
				ImGui::EndDisabled();

				ImGui::BeginChild("Anim list", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
				DrawGenericEditableList<AnimationHandle>(pEditorData->animations, pEditorData->selectedAnims, "Animation");

				ImGui::EndChild();

				ImGui::SameLine();

				ImGui::BeginChild("Animation editor");
				{
					ImGui::SeparatorText("Animation editor");
					if (pEditorData->selectedAnims.empty()) {
						ImGui::TextUnformatted("No animation selected");
					}
					else if (pEditorData->selectedAnims.size() > 1) {
						ImGui::TextUnformatted("Multiple animations selected");
					}
					else {
						pEditorData->currentAnim = pEditorData->selectedAnims[0];
						AnimationHandle& animHandle = pEditorData->animations[pEditorData->currentAnim];

						if (DrawAssetField("Animation", ASSET_TYPE_ANIMATION, animHandle.id)) {
							asset.dirty = true;
						}

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
}

static void DrawActorWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Actor editor", pContext->actorWindowOpen, ASSET_TYPE_ACTOR_PROTOTYPE, "New actor prototype", DrawActorEditor, state, PopulateActorEditorData, ApplyActorEditorData, DeleteActorEditorData);
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
enum DungeonNodeType {
	DUNGEON_NODE_ROOM,
	DUNGEON_NODE_EXIT,
};

struct DungeonRoomData {
	const RoomTemplateHandle templateId;
	
	DungeonRoomData() : templateId(0) {}
	DungeonRoomData(RoomTemplateHandle id) : templateId(id) {}
};

struct DungeonExitData {
	u8 direction;
	u8 keyArea;
};

struct DungeonNode {
	u32 id;
	u32 type;
	union {
		DungeonRoomData roomData;
		DungeonExitData exitData;
	};

	glm::vec2 gridPos;
	
	DungeonNode() : id(0), type(0), roomData(), gridPos(0.0f, 0.0f) {}
	
	// Constructor for room nodes
	DungeonNode(u32 nodeId, u32 nodeType, RoomTemplateHandle templateId, glm::vec2 pos) 
		: id(nodeId), type(nodeType), roomData(templateId), gridPos(pos) {}
	
	// Constructor for exit nodes
	DungeonNode(u32 nodeId, u32 nodeType, u8 direction, u8 keyArea, glm::vec2 pos) 
		: id(nodeId), type(nodeType), exitData{direction, keyArea}, gridPos(pos) {}
};

struct EditorDungeon {
	std::unordered_map<u32, DungeonNode> nodes;
};

struct DungeonEditorData {
	EditorDungeon dungeon;
	u32 selectedNodeId = UUID_NULL;
};

static EditorDungeon ConvertFromDungeon(const Dungeon* pDungeon) {
	EditorDungeon outDungeon{};

	for (u32 i = 0; i < DUNGEON_GRID_SIZE; i++) {
		const DungeonCell& cell = pDungeon->grid[i];
		const glm::vec2 pos(i % DUNGEON_GRID_DIM, i / DUNGEON_GRID_DIM);

		if (cell.roomIndex >= 0 && cell.screenIndex == 0) {
			const RoomInstance& room = pDungeon->rooms[cell.roomIndex];

			outDungeon.nodes.emplace(room.id, DungeonNode(room.id, DUNGEON_NODE_ROOM, room.templateId, pos));
		}
		else if (cell.roomIndex < -1) {
			const u32 nodeId = Random::GenerateUUID32();

			const u8 dir = ~(cell.roomIndex) - 1;
			outDungeon.nodes.emplace(nodeId, DungeonNode(nodeId, DUNGEON_NODE_EXIT, dir, cell.screenIndex, pos));
		}
	}

	return outDungeon;
}

static void ConvertToDungeon(const EditorDungeon& dungeon, Dungeon* pOutDungeon) {

	s8 roomIndex = 0;
	for (auto& [id, node] : dungeon.nodes) {
		if (node.type == DUNGEON_NODE_ROOM) {
			pOutDungeon->rooms[roomIndex] = RoomInstance{
				.id = node.id,
				.templateId = node.roomData.templateId
			};

			RoomTemplate* pRoomHeader = AssetManager::GetAsset(node.roomData.templateId);

			const u32 width = pRoomHeader ? pRoomHeader->width : 1;
			const u32 height = pRoomHeader ? pRoomHeader->height : 1;

			for (u32 y = 0; y < height; y++) {
				for (u32 x = 0; x < width; x++) {
					const u8 screenIndex = x + y * ROOM_MAX_DIM_SCREENS;
					const u32 gridIndex = (node.gridPos.x + x) + (node.gridPos.y + y) * DUNGEON_GRID_DIM;
					pOutDungeon->grid[gridIndex] = {
						.roomIndex = roomIndex,
						.screenIndex = screenIndex,
					};
				}
			}

			++roomIndex;
		}
		else {
			const u32 gridIndex = node.gridPos.x + node.gridPos.y * DUNGEON_GRID_DIM;

			// Convert direction to negative room index value
			const s8 ind = ~(node.exitData.direction + 1);
			pOutDungeon->grid[gridIndex] = {
				.roomIndex = ind,
				.screenIndex = node.exitData.keyArea,
			};
		}
	}
	pOutDungeon->roomCount = roomIndex;
}

static void PopulateDungeonEditorData(EditedAsset& editedAsset) {
	if (editedAsset.userData) {
		delete editedAsset.userData;
	}

	editedAsset.userData = new DungeonEditorData();
	DungeonEditorData* pEditorData = (DungeonEditorData*)editedAsset.userData;
	Dungeon* pDungeon = (Dungeon*)editedAsset.data;
	pEditorData->dungeon = ConvertFromDungeon(pDungeon);
}

static void ApplyDungeonEditorData(EditedAsset& editedAsset) {
	const DungeonEditorData* pEditorData = (DungeonEditorData*)editedAsset.userData;
	Dungeon* pDungeon = (Dungeon*)editedAsset.data;

	ConvertToDungeon(pEditorData->dungeon, pDungeon);
}

static void DeleteDungeonEditorData(EditedAsset& editedAsset) {
	delete editedAsset.userData;
}

static u32 GetRoomCount(const EditorDungeon& dungeon) {
	u32 result = 0;
	for (auto& [id, node] : dungeon.nodes) {
		if (node.type != DUNGEON_NODE_ROOM) {
			continue;
		}

		result++;
	}
	return result;
}

static glm::ivec2 GetDungeonNodeSize(const DungeonNode& node) {
	glm::ivec2 result(1, 1);

	if (node.type == DUNGEON_NODE_ROOM) {
		RoomTemplate* pRoomHeader = AssetManager::GetAsset(node.roomData.templateId);
		if (pRoomHeader) {
			result.x = pRoomHeader->width;
			result.y = pRoomHeader->height;
		}
	}
	
	return result;
}

static void DrawDungeonNode(const DungeonNode& node, const glm::mat3& gridToScreen, bool selected, bool& outHovered) {
	outHovered = false;

	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Node body
	const glm::ivec2 nodeSize = GetDungeonNodeSize(node);

	const glm::vec2 nodeTopLeft = gridToScreen * glm::vec3(node.gridPos.x, node.gridPos.y, 1.0f);
	const glm::vec2 nodeBtmRight = gridToScreen * glm::vec3(node.gridPos.x + nodeSize.x, node.gridPos.y + nodeSize.y, 1.0f);
	const r32 scale = glm::length(glm::vec2(gridToScreen[0][0], gridToScreen[1][0])) / VIEWPORT_WIDTH_METATILES;

	const ImVec2 nodeDrawMin = ImVec2(nodeTopLeft.x, nodeTopLeft.y);
	const ImVec2 nodeDrawMax = ImVec2(nodeBtmRight.x, nodeBtmRight.y);

	constexpr ImU32 outlineColor = IM_COL32(255, 255, 255, 255);
	constexpr ImU32 outlineSelectedColor = IM_COL32(0, 255, 0, 255);
	constexpr ImU32 outlineHoveredColor = IM_COL32(255, 255, 0, 255);
	constexpr r32 outlineThickness = 1.0f;
	constexpr r32 outlineHoveredThickness = 2.0f;

	if (node.type == DUNGEON_NODE_ROOM) {
		RoomTemplate* pRoomTemplate = AssetManager::GetAsset(node.roomData.templateId);
		if (pRoomTemplate) {
			DrawTilemap(&pRoomTemplate->tilemap, ImVec2(0,0), ImVec2(nodeSize.x * VIEWPORT_WIDTH_METATILES, nodeSize.y * VIEWPORT_HEIGHT_METATILES), nodeDrawMin, scale);
			drawList->AddRectFilled(nodeDrawMin, nodeDrawMax, IM_COL32(0, 0, 0, 0x80));
			drawList->AddText(ImVec2(nodeDrawMin.x + 10, nodeDrawMin.y + 10), IM_COL32(255, 255, 255, 255), AssetManager::GetAssetName(node.roomData.templateId.id));
		}
		else {
			drawList->AddRectFilled(nodeDrawMin, nodeDrawMax, IM_COL32(0xc, 0xc, 0xc, 255));
			drawList->AddText(ImVec2(nodeDrawMin.x + 10, nodeDrawMin.y + 10), IM_COL32(255, 0, 0, 255), "ERROR");
		}

	}
	else {
		drawList->AddRectFilled(nodeDrawMin, nodeDrawMax, IM_COL32(0xc, 0xc, 0xc, 255));
		drawList->AddText(ImVec2(nodeDrawMin.x + 10, nodeDrawMin.y + 10), IM_COL32(255, 255, 255, 255), "EXIT");
	}

	ImU32 nodeOutlineColor = selected ? outlineSelectedColor : outlineColor;
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
	
	for (auto& [otherId, otherRoom] : dungeon.nodes) {
		if (otherId == roomId) {
			continue;
		}

		const glm::ivec2 otherSize = GetDungeonNodeSize(otherRoom);
		const AABB rect(0, otherSize.x, 0, otherSize.y);

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

static void DrawDungeonCanvas(EditedAsset& asset) {
	DungeonEditorData* pEditorData = (DungeonEditorData*)asset.userData;
	EditorDungeon& dungeon = pEditorData->dungeon;

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

	static u32 draggedNode = UUID_NULL;
	static glm::vec2 dragStartPos(0.0f);

	static u32 contextNode = UUID_NULL;

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("dd_asset", ImGuiDragDropFlags_AcceptBeforeDelivery))
		{
			const u64 assetId = *(const u64*)payload->Data;

			const AssetEntry* pAssetInfo = AssetManager::GetAssetInfo(assetId);
			const bool isValidRoomTemplate = pAssetInfo && pAssetInfo->flags.type == ASSET_TYPE_ROOM_TEMPLATE;

			if (isValidRoomTemplate) {
				RoomTemplateHandle handle(assetId);
				RoomTemplate* pRoomHeader = AssetManager::GetAsset(handle);

				const glm::ivec2 roomTopLeft = hoveredCellPos;
				const glm::ivec2 roomDim = { pRoomHeader->width, pRoomHeader->height };

				const bool posFree = DrawDungeonDragDropPreview(UUID_NULL, roomTopLeft, roomDim, gridToScreen, dungeon);

				if (posFree && payload->IsDelivery()) {
					const u32 nodeId = Random::GenerateUUID32();
					if (GetRoomCount(dungeon) < MAX_DUNGEON_ROOM_COUNT) {
						dungeon.nodes.emplace(nodeId, DungeonNode(nodeId, DUNGEON_NODE_ROOM, RoomTemplateHandle(assetId), roomTopLeft));
					}
				}
			}

		}
		ImGui::EndDragDropTarget();
	}

	if (draggedNode != UUID_NULL) {
		auto& node = dungeon.nodes[draggedNode];
		const glm::ivec2 nodeSize = GetDungeonNodeSize(node);
		const glm::ivec2 dragTargetPos = glm::clamp(glm::ivec2(glm::roundEven(node.gridPos.x), glm::roundEven(node.gridPos.y)), glm::ivec2(0), glm::ivec2(DUNGEON_GRID_DIM - 1));
		const bool posFree = DrawDungeonDragDropPreview(node.id, dragTargetPos, nodeSize, gridToScreen, dungeon);

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			glm::vec2 scaledDelta = screenToGrid * glm::vec3(io.MouseDelta.x, io.MouseDelta.y, 0.0f);

			node.gridPos += scaledDelta;
		}

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (posFree) {
				node.gridPos = dragTargetPos;
			}
			else {
				node.gridPos = dragStartPos;
			}

			draggedNode = UUID_NULL;
		}
	}

	for (auto& [id, node] : dungeon.nodes) {
		const bool selected = id == pEditorData->selectedNodeId;
		bool nodeHovered;
		DrawDungeonNode(node, gridToScreen, selected, nodeHovered);

		if (active && nodeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			draggedNode = id;
			dragStartPos = node.gridPos;
			pEditorData->selectedNodeId = id;
		}

		if (active && nodeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			contextNode = id;
			ImGui::OpenPopup("NodeContextMenu");
		}
	}

	if (active && contextNode == UUID_NULL && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		ImGui::OpenPopup("CanvasContextMenu");
	}

	if (ImGui::BeginPopup("NodeContextMenu")) {
		if (ImGui::MenuItem("Delete Node") && contextNode != UUID_NULL) {
			dungeon.nodes.erase(contextNode);
			contextNode = UUID_NULL;
		}
		ImGui::EndPopup();
	}
	else {
		contextNode = UUID_NULL;
	}

	if (ImGui::BeginPopup("CanvasContextMenu")) {
		bool posFree = DungeonPositionFree(UUID_NULL, hoveredCellPos, { 1, 1 }, dungeon);
		ImGui::BeginDisabled(!posFree);
		if (ImGui::MenuItem("Add exit")) {
			const u32 nodeId = Random::GenerateUUID32();
			dungeon.nodes.emplace(nodeId, DungeonNode(nodeId, DUNGEON_NODE_EXIT, 0, 0, hoveredCellPos));
		}
		ImGui::EndDisabled();
		ImGui::EndPopup();
	}

	drawList->PopClipRect();
}

static void DrawDungeonTools(EditedAsset& asset) {
	DungeonEditorData* pEditorData = (DungeonEditorData*)asset.userData;

	if (ImGui::BeginTabBar("Dungeon tools")) {
		if (ImGui::BeginTabItem("Properties")) {

			if (ImGui::InputText("Name", asset.name, MAX_ASSET_NAME_LENGTH)) {
				asset.dirty = true;
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Room list")) {
			DrawAssetList(ASSET_TYPE_ROOM_TEMPLATE);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Node Info")) {
			const auto it = pEditorData->dungeon.nodes.find(pEditorData->selectedNodeId);
			if (it == pEditorData->dungeon.nodes.end()) {
				ImGui::Text("No node selected!");
			}
			else {
				DungeonNode& node = it->second;

				ImGui::Text("Grid position: (%f, %f)", node.gridPos.x, node.gridPos.y);

				if (node.type == DUNGEON_NODE_ROOM) {
					ImGui::Text("Node Type: ROOM");

					ImGui::Text("Room: %s", AssetManager::GetAssetName(node.roomData.templateId.id));
				}
				else {
					ImGui::Text("Node Type: EXIT");

					constexpr const char* exitDirNames[4] = { "Right", "Left", "Bottom", "Top" };

					s32 dir = node.exitData.direction;
					if (DrawTypeSelectionCombo("Exit direction", exitDirNames, 4, dir)) {
						asset.dirty = true;
					}
					node.exitData.direction = dir;

					if (ImGui::InputScalar("Target area", ImGuiDataType_U8, &node.exitData.keyArea)) {
						asset.dirty = true;
					}
				}
			}


			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

}

static void DrawDungeonEditor(EditedAsset& asset) {
	ImGui::BeginChild("Dungeon editor");

	const r32 toolWindowWidth = 350.0f;
	ImGuiStyle& style = ImGui::GetStyle();
	const r32 canvasViewWidth = ImGui::GetContentRegionAvail().x - style.WindowPadding.x - toolWindowWidth;

	ImGui::BeginChild("Node graph", ImVec2(canvasViewWidth, 0));
	DrawDungeonCanvas(asset);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("Dungeon tools", ImVec2(toolWindowWidth, 0));
	DrawDungeonTools(asset);
	ImGui::EndChild();

	ImGui::EndChild();
}

static void DrawDungeonWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Dungeon editor", pContext->dungeonWindowOpen, ASSET_TYPE_DUNGEON, "New Dungeon", DrawDungeonEditor, state, PopulateDungeonEditorData, ApplyDungeonEditorData, DeleteDungeonEditorData);
}
#pragma endregion

#pragma region Overworld editor
struct OverworldEditorData {
	u32 editMode = OW_EDIT_MODE_NONE;
	glm::vec2 viewportPos = glm::vec2(0.0f);
	TilemapClipboard clipboard{};
};

static void PopulateOverworldEditorData(EditedAsset& editedAsset) {
	if (editedAsset.userData) {
		delete editedAsset.userData;
	}

	editedAsset.userData = new OverworldEditorData();
}

static void DeleteOverworldEditorData(EditedAsset& editedAsset) {
	delete editedAsset.userData;
}

static void DrawOverworldEditor(EditedAsset& asset) {
	ImGui::BeginChild("Overworld editor");

	Overworld* pOverworld = (Overworld*)asset.data;
	OverworldEditorData* pEditorData = (OverworldEditorData*)asset.userData;

	const bool showToolsWindow = true;
	const r32 toolWindowWidth = showToolsWindow ? 350.0f : 0.0f;
	ImGuiStyle& style = ImGui::GetStyle();
	const r32 tilemapWidth = ImGui::GetContentRegionAvail().x - style.WindowPadding.x - toolWindowWidth;

	ImGui::BeginChild("Tilemap", ImVec2(tilemapWidth, 0));
	ImVec2 topLeft = ImGui::GetCursorScreenPos();
	static const r32 aspectRatio = (r32)VIEWPORT_WIDTH_PIXELS / VIEWPORT_HEIGHT_PIXELS;
	const r32 contentWidth = ImGui::GetContentRegionAvail().x;
	const r32 contentHeight = contentWidth / aspectRatio;
	const r32 renderScale = contentWidth / VIEWPORT_WIDTH_PIXELS;
	const ImVec2 btmRight(topLeft.x + contentWidth, topLeft.y + contentHeight);
	const r32 tileDrawSize = METATILE_DIM_PIXELS * renderScale;

	DrawTilemapEditor(&pOverworld->tilemap, topLeft, renderScale, pEditorData->viewportPos, pEditorData->clipboard, pEditorData->editMode == OW_EDIT_MODE_TILES);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(topLeft, btmRight, true);

	for (u32 i = 0; i < MAX_OVERWORLD_KEY_AREA_COUNT; i++) {
		OverworldKeyArea& area = pOverworld->keyAreas[i];

		char label[8];
		snprintf(label, 8, "%#02x", i);
		const ImVec2 areaPosInPixelCoords = ImVec2((area.position.x - pEditorData->viewportPos.x) * tileDrawSize + topLeft.x, (area.position.y - pEditorData->viewportPos.y) * tileDrawSize + topLeft.y);
		drawList->AddText(areaPosInPixelCoords, IM_COL32(255, 255, 255, 255), label);
	}

	drawList->PopClipRect();
	ImGui::EndChild();

	ImGui::SameLine();

	pEditorData->editMode = OW_EDIT_MODE_NONE;
	if (showToolsWindow) {
		ImGui::BeginChild("Overworld tools", ImVec2(toolWindowWidth, 0));
		if (ImGui::BeginTabBar("Overworld tool tabs")) {
			if (ImGui::BeginTabItem("Tilemap")) {
				pEditorData->editMode = OW_EDIT_MODE_TILES;
				DrawTilemapTools(pEditorData->clipboard, pOverworld->tilemap.tilesetHandle);

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Key areas")) {
				pEditorData->editMode = OW_EDIT_MODE_AREAS;
				static u8 selectedArea = 0;

				char label[8];
				snprintf(label, 8, "%#02x", selectedArea);

				if (ImGui::BeginCombo("Area ID", label)) {
					for (u32 i = 0; i < MAX_OVERWORLD_KEY_AREA_COUNT; i++) {
						ImGui::PushID(i);

						const bool selected = selectedArea == i;

						snprintf(label, 8, "%#02x", i);
						if (ImGui::Selectable(label, selected)) {
							selectedArea = i;
						}

						if (selected) {
							ImGui::SetItemDefaultFocus();
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}

				ImGui::SeparatorText("Area properties");

				OverworldKeyArea& area = pOverworld->keyAreas[selectedArea];

				ImGui::InputScalarN("Position", ImGuiDataType_S8, &area.position, 2);

				if (DrawAssetField("Target dungeon", ASSET_TYPE_DUNGEON, area.dungeonId.id)) {
					asset.dirty = true;
				}

				if (ImGui::InputScalarN("Target grid cell", ImGuiDataType_S8, &area.targetGridCell, 2)) {
					asset.dirty = true;
				}

				bool flipDirection = area.flags.flipDirection;
				if (ImGui::Checkbox("Flip direction", &flipDirection)) {
					area.flags.flipDirection = flipDirection;
					asset.dirty = true;
				}

				bool passthrough = area.flags.passthrough;
				if (ImGui::Checkbox("Passthrough", &passthrough)) {
					area.flags.passthrough = passthrough;
					asset.dirty = true;
				}

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
		ImGui::EndChild();
	}

	ImGui::EndChild();
}

static void DrawOverworldWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Overworld editor", pContext->overworldWindowOpen, ASSET_TYPE_OVERWORLD, "New Overworld", DrawOverworldEditor, state, PopulateOverworldEditorData, nullptr, DeleteOverworldEditorData);
}
#pragma endregion

#pragma region Asset browser
static void SortAssets(std::vector<u64>& assetIds, ImGuiTableSortSpecs* pSortSpecs) {
	if (!pSortSpecs || pSortSpecs->SpecsCount == 0) return; // No sorting requested

	std::sort(assetIds.begin(), assetIds.end(), [pSortSpecs](u64 aId, u64 bId) {
		const ImGuiTableColumnSortSpecs* sortSpecs = pSortSpecs->Specs;

		const AssetEntry& a = *AssetManager::GetAssetInfo(aId);
		const AssetEntry& b = *AssetManager::GetAssetInfo(bId);

		for (int i = 0; i < pSortSpecs->SpecsCount; i++) {
			const ImGuiTableColumnSortSpecs& spec = sortSpecs[i];

			switch (spec.ColumnIndex) {
			case 0: { // Sort by Name
				int cmp = strcmp(a.name, b.name);
				if (cmp != 0) return spec.SortDirection == ImGuiSortDirection_Ascending ? (cmp < 0) : (cmp > 0);
				break;
			}
			case 1: { // Sort by Path
				int cmp = strcmp(a.relativePath, b.relativePath);
				if (cmp != 0) return spec.SortDirection == ImGuiSortDirection_Ascending ? (cmp < 0) : (cmp > 0);
				break;
			}
			case 2: { // Sort by UUID
				if (a.id != b.id) return spec.SortDirection == ImGuiSortDirection_Ascending ? (a.id < b.id) : (a.id > b.id);
				break;
			}
			case 3: { // Sort by Type
				if (a.flags.type != b.flags.type) return spec.SortDirection == ImGuiSortDirection_Ascending ? (a.flags.type < b.flags.type) : (a.flags.type > b.flags.type);
				break;
			}
			case 4: { // Sort by Size
				if (a.size != b.size) return spec.SortDirection == ImGuiSortDirection_Ascending ? (a.size < b.size) : (a.size > b.size);
				break;
			}
			default:
				break;
			}
		}
		return false; // Default (should never happen)
		});

	pSortSpecs->SpecsDirty = false; // Mark sorting as done
}

static void DumpAssetArchive() {
	const AssetIndex& index = AssetManager::GetIndex();

	struct IntermediateAssetHeader {
		u64 id;
		AssetType type;
		char name[MAX_ASSET_NAME_LENGTH];
		size_t size;
	};

	std::filesystem::path directory = "assets_dump";
	std::filesystem::create_directory(directory);
	for (u32 i = 0; i < index.Count(); i++) {
		PoolHandle<AssetEntry> handle = index.GetHandle(i);
		const AssetEntry* pAsset = index.Get(handle);
		if (!pAsset) continue;

		const u64 id = pAsset->id;
		const void* pData = AssetManager::GetAsset(id, pAsset->flags.type);

		std::filesystem::path filePath = directory / std::filesystem::path(std::to_string(id));
		filePath.replace_extension(".bin");

		FILE* pFile = fopen(filePath.string().c_str(), "wb");
		if (!pFile) {
			DEBUG_ERROR("Failed to open file for writing: %s", filePath.string().c_str());
			continue;
		}

		IntermediateAssetHeader header{};
		header.id = id;
		header.type = pAsset->flags.type;
		strncpy(header.name, pAsset->name, MAX_ASSET_NAME_LENGTH);
		header.size = pAsset->size;

		fwrite((const char*)&header, sizeof(IntermediateAssetHeader), 1, pFile);
		fwrite(pData, pAsset->size, 1, pFile);

		fclose(pFile);
	}
}

static void DrawAssetBrowser() {
	ImGui::Begin("Asset browser", &pContext->assetBrowserOpen, ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Save archive")) {
				AssetManager::SaveArchive(ASSETS_NPAK_OUTPUT);
			}
			if (ImGui::MenuItem("Reload archive")) {
				AssetManager::LoadArchive(ASSETS_NPAK_OUTPUT);
			}
			if (ImGui::MenuItem("Repack archive")) {
				AssetManager::RepackArchive();
			}
			if (ImGui::MenuItem("Dump archive")) {
				DumpAssetArchive();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	static u64 selectedAsset = UUID_NULL;
	static size_t assetCount = 0;
	static std::vector<u64> assetIds;

	ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Sortable;
	if (ImGui::BeginTable("Assets", 5, flags)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("UUID", ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
		ImGui::TableHeadersRow();

		ImGuiTableSortSpecs* pSortSpecs = ImGui::TableGetSortSpecs();

		const AssetIndex& index = AssetManager::GetIndex();
		const size_t newAssetCount = AssetManager::GetAssetCount();

		if (assetCount != newAssetCount) {
			assetIds.clear();
			assetIds.reserve(newAssetCount);
			for (u32 i = 0; i < index.Count(); i++) {
				PoolHandle<AssetEntry> handle = index.GetHandle(i);
				const AssetEntry* asset = index.Get(handle);
				if (asset) {
					assetIds.emplace_back(asset->id);
				}
			}
			assetCount = newAssetCount;
			if (pSortSpecs) {
				pSortSpecs->SpecsDirty = true;
			}
		}

		if (pSortSpecs && pSortSpecs->SpecsDirty) {
			SortAssets(assetIds, pSortSpecs);
		}

		for (const auto& id : assetIds) {
			const bool selected = id == selectedAsset;

			const AssetEntry* pAsset = AssetManager::GetAssetInfo(id);

			ImGui::PushID((s32)id);
			ImGui::TableNextRow();
			ImGui::BeginDisabled(pAsset->flags.deleted);
			ImGui::TableNextColumn();
			if (ImGui::Selectable(pAsset->name, selected, ImGuiSelectableFlags_SpanAllColumns)) {
				selectedAsset = id;
			}
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				ImGui::SetDragDropPayload("dd_asset", &id, sizeof(u64));
				ImGui::Text("%s", pAsset->name);

				ImGui::EndDragDropSource();
			}
			ImGui::TableNextColumn();
			ImGui::Text("%s", pAsset->relativePath);
			ImGui::TableNextColumn();
			ImGui::Text("%llu", id);
			ImGui::TableNextColumn();
			ImGui::Text("%s", ASSET_TYPE_NAMES[pAsset->flags.type]);
			ImGui::TableNextColumn();
			ImGui::Text("%u bytes", pAsset->size);
			ImGui::EndDisabled();
			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	ImGui::End();
}
#pragma endregion

#pragma region Animation
struct AnimationEditorData {
	ImVector<AnimationFrame> frames;
};

static void PopulateAnimationEditorData(EditedAsset& editedAsset) {
	if (editedAsset.userData) {
		delete editedAsset.userData;
	}

	editedAsset.userData = new AnimationEditorData();
	AnimationEditorData* pEditorData = (AnimationEditorData*)editedAsset.userData;

	Animation* pAnimation = (Animation*)editedAsset.data;
	for (u32 i = 0; i < pAnimation->frameCount; i++) {
		pEditorData->frames.push_back(pAnimation->GetFrames()[i]);
	}
}

static void ApplyAnimationEditorData(EditedAsset& editedAsset) {
	AnimationEditorData* pEditorData = (AnimationEditorData*)editedAsset.userData;
	Animation* pAnimation = (Animation*)editedAsset.data;

	pAnimation->frameCount = pEditorData->frames.size();
	const u32 newSize = Editor::Assets::GetAssetSize(editedAsset.type, pAnimation);
	ResizeEditedAsset(editedAsset, newSize);

	// Reassign in case the asset was resized
	pAnimation = (Animation*)editedAsset.data;
	for (u32 i = 0; i < pAnimation->frameCount; i++) {
		pAnimation->GetFrames()[i] = pEditorData->frames[i];
	}
}

static void DeleteAnimationEditorData(EditedAsset& editedAsset) {
	delete editedAsset.userData;
}

static void DrawAnimationPreview(const AnimationEditorData* pEditorData, s32 frameIndex, r32 size) {
	constexpr s32 gridSizeTiles = 8;

	const r32 renderScale = size / (gridSizeTiles * TILE_DIM_PIXELS);
	const r32 gridStepPixels = TILE_DIM_PIXELS * renderScale;
	ImVec2 gridPos = DrawTileGrid(ImVec2(size, size), gridStepPixels);
	ImVec2 origin = ImVec2(gridPos.x + size / 2, gridPos.y + size / 2);

	if (pEditorData->frames.empty() || frameIndex < 0 || frameIndex >= (s32)pEditorData->frames.size()) {
		return; // No frames to display or invalid index
	}

	const AnimationFrame& frame = pEditorData->frames[frameIndex];
	const Metasprite* pMetasprite = AssetManager::GetAsset(frame.metaspriteId);
	if (!pMetasprite) {
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddLine(ImVec2(origin.x - 10, origin.y), ImVec2(origin.x + 10, origin.y), IM_COL32(200, 200, 200, 255));
	drawList->AddLine(ImVec2(origin.x, origin.y - 10), ImVec2(origin.x, origin.y + 10), IM_COL32(200, 200, 200, 255));

	DrawMetasprite(pMetasprite, origin, renderScale);
}

static void DrawAnimationEditor(EditedAsset& asset) {
	ImGui::BeginChild("Animation editor");

	Animation* pAnimation = (Animation*)asset.data;
	AnimationEditorData* pEditorData = (AnimationEditorData*)asset.userData;

	ImGui::SeparatorText("Properties");
	if (ImGui::InputText("Name", asset.name, MAX_ASSET_NAME_LENGTH)) {
		asset.dirty = true;
	}

	static const s16 step = 1;
	u16 frameCount = pEditorData->frames.size();
	if (ImGui::InputScalar("Frame count", ImGuiDataType_U16, &frameCount, &step)) {
		pEditorData->frames.resize(frameCount);
		asset.dirty = true;
	}

	if (ImGui::InputScalar("Loop point", ImGuiDataType_S16, &pAnimation->loopPoint, &step)) {
		asset.dirty = true;
	}
	pAnimation->loopPoint = glm::clamp(pAnimation->loopPoint, s16(-1), s16(frameCount - 1));

	if (ImGui::InputScalar("Frame length", ImGuiDataType_U8, &pAnimation->frameLength, &step)) {
		asset.dirty = true;
	}

	// Playback
	static bool isPlaying = false;
	static s32 currentTick = 0;
	static r32 accumulator = 0.0f;

	const s32 totalTicks = frameCount * pAnimation->frameLength;
	constexpr r32 tickTime = 1.0f / 60.0f;

	static r32 previousTime = pContext->secondsElapsed;
	r32 currentTime = pContext->secondsElapsed;
	r32 deltaTime = currentTime - previousTime;
	previousTime = currentTime;

	if (isPlaying) {
		accumulator += deltaTime;
		while (accumulator >= tickTime) {
			accumulator -= tickTime;
			currentTick++;
		}
	}

	while (currentTick >= totalTicks && currentTick != 0) {
		if (pAnimation->loopPoint < 0) {
			isPlaying = false;
			currentTick = 0;
			break;
		}
		currentTick -= (frameCount - pAnimation->loopPoint) * pAnimation->frameLength;
	}

	s32 currentFrame = pAnimation->frameLength == 0 ? 0 : currentTick / pAnimation->frameLength;

	ImGui::SeparatorText("Preview");
	constexpr r32 previewSize = 256;
	ImGui::BeginChild("Animation preview", ImVec2(previewSize, previewSize));

	DrawAnimationPreview(pEditorData, currentFrame, previewSize);

	ImGui::EndChild();

	ImGui::SeparatorText("Timeline");

	ImGui::BeginChild("TimelineArea", ImVec2(0, 128.0f), ImGuiChildFlags_Border);

	ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 contentSize = ImGui::GetContentRegionAvail();
	const r32 timelineHeight = 64.0f;
	const ImVec2 timelineSize(contentSize.x, timelineHeight);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 topLeft = ImGui::GetCursorScreenPos();
	const ImVec2 btmRight = ImVec2(topLeft.x + timelineSize.x, topLeft.y + timelineSize.y);
	const r32 yHalf = (topLeft.y + btmRight.y) / 2.0f;

	ImGui::InvisibleButton("##canvas", timelineSize);

	const ImU32 color = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Border]);
	drawList->AddLine(ImVec2(topLeft.x, btmRight.y), ImVec2(btmRight.x, btmRight.y), color);

	const r32 frameWidth = timelineSize.x / frameCount;
	for (u32 i = 0; i < frameCount; i++) {
		const r32 x = i * frameWidth + topLeft.x;

		drawList->AddLine(ImVec2(x, topLeft.y), ImVec2(x, btmRight.y), color, 2.0f);

		for (u32 s = 0; s < frameCount; s++) {
			const r32 xSmallNormalized = (r32)s / frameCount;
			const r32 xSmall = xSmallNormalized * frameWidth + x;

			drawList->AddLine(ImVec2(xSmall, yHalf), ImVec2(xSmall, btmRight.y), color, 1.0f);
		}
	}
	
	const r32 xCurrentPosNormalized = r32(currentTick) / totalTicks;
	const r32 xCurrentPos = xCurrentPosNormalized * timelineSize.x + topLeft.x;
	drawList->AddCircleFilled(ImVec2(xCurrentPos, btmRight.y), 8.0f, IM_COL32(255, 255, 0, 255));

	const r32 xLoopPointNormalized = r32(pAnimation->loopPoint) / frameCount;
	const r32 xLoopPoint = xLoopPointNormalized * timelineSize.x + topLeft.x;
	drawList->AddCircleFilled(ImVec2(xLoopPoint, btmRight.y), 8.0f, IM_COL32(128, 0, 255, 255));

	ImGui::EndChild();

	const ImVec2 frameAreaTopLeft(topLeft.x, topLeft.y + timelineHeight);
	const ImVec2 frameAreaSize(contentSize.x, contentSize.y - timelineHeight);
	const r32 frameBoxSize = contentSize.y - timelineHeight;

	for (u32 i = 0; i < frameCount; i++) {
		const r32 x = i * frameWidth + topLeft.x;

		AnimationFrame& frame = pEditorData->frames[i];

		const ImVec2 frameMin(x, topLeft.y + timelineHeight + style.ItemSpacing.y);
		const ImVec2 frameMax(frameMin.x + frameBoxSize, frameMin.y + frameBoxSize);
		ImGui::SetCursorScreenPos(frameMin);

		ImGui::PushID(i);

		ImGui::Selectable("", false, 0, ImVec2(frameBoxSize, frameBoxSize));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("dd_asset", ImGuiDragDropFlags_AcceptBeforeDelivery))
			{
				const u64 assetId = *(const u64*)payload->Data;
				
				const AssetEntry* pAssetInfo = AssetManager::GetAssetInfo(assetId);
				const bool isValidMetasprite = pAssetInfo && pAssetInfo->flags.type == ASSET_TYPE_METASPRITE;

				if (isValidMetasprite && payload->IsDelivery()) {
					frame.metaspriteId = MetaspriteHandle(assetId);
					asset.dirty = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::PopID();

		drawList->AddRect(frameMin, frameMax, IM_COL32(255, 255, 255, 255), 4.0f);
		if (frame.metaspriteId != MetaspriteHandle::Null()) {
			const Metasprite* pMetasprite = AssetManager::GetAsset(frame.metaspriteId);
			if (pMetasprite) {
				DrawMetasprite(pMetasprite, ImVec2(frameMin.x + frameBoxSize * 0.5f, frameMin.y + frameBoxSize * 0.5f), 1.0f);
			}
		}
	}
	ImGui::NewLine();

	// Playback controls
	if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
		isPlaying = !isPlaying;
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop")) {
		isPlaying = false;
		currentTick = 0;
		accumulator = 0.0f;
	}

	ImGui::EndChild();
}

static void DrawAnimationWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Animation editor", pContext->animationWindowOpen, ASSET_TYPE_ANIMATION, "New Animation", DrawAnimationEditor, state, PopulateAnimationEditorData, ApplyAnimationEditorData, DeleteAnimationEditorData);
}
#pragma endregion

#pragma region Sound editor
constexpr const char* audioChannelNames[CHAN_COUNT] = { "Pulse 1", "Pulse 2", "Triangle", "Noise" };
constexpr const char* soundTypeNames[SOUND_TYPE_COUNT] = { "SFX", "Music" };

static void DrawSoundEditor(EditedAsset& asset) {
	ImGui::BeginChild("Sound editor");

	Sound* pSound = (Sound*)asset.data;

	ImGui::SeparatorText("Properties");
	{
		if (ImGui::InputText("Name", asset.name, MAX_ASSET_NAME_LENGTH)) {
			asset.dirty = true;
		}

		if (DrawTypeSelectionCombo("Type", soundTypeNames, SOUND_TYPE_COUNT, pSound->type, false)) {
			asset.dirty = true;
		}

		ImGui::BeginDisabled(true);
		ImGui::InputScalar("Length", ImGuiDataType_U32, &pSound->length);
		ImGui::InputScalar("Loop point", ImGuiDataType_U32, &pSound->loopPoint);
		ImGui::EndDisabled();

		ImGui::BeginDisabled(pSound->type != SOUND_TYPE_SFX);
		if (DrawTypeSelectionCombo("SFX channel", audioChannelNames, CHAN_COUNT, pSound->sfxChannel, false)) {
			asset.dirty = true;
		}
		ImGui::EndDisabled();
	}

	// NOTE: The asset needs to be saved for this to work properly. How to work around?
	ImGui::SeparatorText("Preview");
	{
		if (ImGui::Button("Play")) {
			if (pSound->type == SOUND_TYPE_SFX) {
				Audio::PlaySFX(SoundHandle(asset.id));
			}
			else {
				Audio::PlayMusic(SoundHandle(asset.id), true);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop")) {
			if (pSound->type == SOUND_TYPE_MUSIC) {
				Audio::StopMusic();
			}
		}
	}

	ImGui::EndChild();
}

static void DrawSoundWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Sound editor", pContext->soundWindowOpen, ASSET_TYPE_SOUND, "New Sound", DrawSoundEditor, state);
}

#pragma endregion

#pragma region Pattern editor
struct ChrEditorData {
	EditedAssetRenderData* pRenderData = nullptr;
	PaletteHandle selectedPalette = PaletteHandle::Null();
};

static EditorRenderBuffer* GetChrRenderBuffer(u64 id) {
	return GetRenderBuffer(id, ASSET_TYPE_CHR_BANK, CHR_SIZE_BYTES);
}

static EditorRenderBuffer* GetPaletteRenderBuffer(u64 id) {
	return GetRenderBuffer(id, ASSET_TYPE_PALETTE, PALETTE_COLOR_COUNT);
}

static void PopulateChrEditorData(EditedAsset& editedAsset) {
	ChrEditorData*& pEditorData = (ChrEditorData*&)editedAsset.userData;
	
	if (!pEditorData) {
		pEditorData = new ChrEditorData();

		pEditorData->pRenderData = new EditedAssetRenderData();
		pEditorData->pRenderData->pBuffer = Rendering::CreateEditorBuffer(CHR_SIZE_BYTES, editedAsset.data);
		pEditorData->pRenderData->pTexture = Rendering::CreateEditorTexture(CHR_DIM_PIXELS, CHR_DIM_PIXELS, EDITOR_TEXTURE_USAGE_CHR, pEditorData->pRenderData->pBuffer);
		pContext->tempRenderData.insert(pEditorData->pRenderData);
	}

	Rendering::UpdateEditorBuffer(pEditorData->pRenderData->pBuffer, editedAsset.data);
}

static void DeleteChrEditorData(EditedAsset& editedAsset) {
	ChrEditorData* pEditorData = (ChrEditorData*)editedAsset.userData;

	pContext->tempEraseList.push_back(pEditorData->pRenderData);

	// NOTE: This does not free the render data, so it's potentially leaky
	// Render data should be freed later when iterating the erase list
	delete editedAsset.userData;
}

static void DrawEditedChrSheet(r32 size, ImTextureID chrTexture, s8 bgColorIndex) {
	// Copypaska....
	constexpr s32 gridSizeTiles = CHR_DIM_TILES;

	const r32 renderScale = size / (gridSizeTiles * TILE_DIM_PIXELS);
	const r32 gridStepPixels = TILE_DIM_PIXELS * renderScale;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 gridSize = ImVec2(size, size);
	const ImVec2 topLeft = DrawTileGrid(gridSize, gridStepPixels);
	const ImVec2 btmRight(topLeft.x + size, topLeft.y + size);
	const glm::vec4 uvMinMax = { 0,0,1,1 };

	// DrawTileGrid has a background but it needs to be drawn over with the correct color
	if (bgColorIndex >= 0) {
		const ImVec2 colorPos(bgColorIndex % 16, bgColorIndex / 16);
		const ImVec2 bgUvMin(colorPos.x / 16, colorPos.y / 8);
		const ImVec2 bgUvMax((colorPos.x + 1.0f) / 16, (colorPos.y + 1.0f) / 8);
		drawList->AddImage(GetTextureID(pContext->pColorTexture), topLeft, btmRight, bgUvMin, bgUvMax);
	}

	drawList->AddImage(chrTexture, topLeft, btmRight, ImVec2(uvMinMax.x, uvMinMax.y), ImVec2(uvMinMax.z, uvMinMax.w));
}

static void DrawChrEditor(EditedAsset& asset) {
	ImGui::BeginChild("Chr editor");

	ChrEditorData* pEditorData = (ChrEditorData*)asset.userData;

	EditorRenderBuffer* pPaletteBuffer = GetPaletteRenderBuffer(pEditorData->selectedPalette.id);
	Rendering::UpdateEditorTexture(pEditorData->pRenderData->pTexture, pEditorData->pRenderData->pBuffer, pPaletteBuffer);

	ImGui::SeparatorText("Properties");
	if (ImGui::InputText("Name", asset.name, MAX_ASSET_NAME_LENGTH)) {
		asset.dirty = true;
	}

	ImGui::SeparatorText("Preview");

	constexpr r32 gridSizePixels = 512.0f;
	const Palette* pPalette = AssetManager::GetAsset(PaletteHandle(pEditorData->selectedPalette.id));
	const s8 bgColorIndex = pPalette ? pPalette->colors[0] : -1;
	DrawEditedChrSheet(gridSizePixels, GetTextureID(pEditorData->pRenderData->pTexture), bgColorIndex);
	DrawAssetField("Palette", ASSET_TYPE_PALETTE, pEditorData->selectedPalette.id);

	ImGui::EndChild();
}

static void DrawChrWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Pattern editor", pContext->chrWindowOpen, ASSET_TYPE_CHR_BANK, "New pattern sheet", DrawChrEditor, state, PopulateChrEditorData, nullptr, DeleteChrEditorData);
}
#pragma endregion

#pragma region Palette editor
struct PaletteEditorData {
	EditedAssetRenderData* pRenderData = nullptr;
	s32 selectedColor = -1;
	ChrBankHandle selectedChrBank = ChrBankHandle::Null();
};

static void PopulatePaletteEditorData(EditedAsset& editedAsset) {
	PaletteEditorData*& pEditorData = (PaletteEditorData*&)editedAsset.userData;

	if (!pEditorData) {
		pEditorData = new PaletteEditorData();

		pEditorData->pRenderData = new EditedAssetRenderData();
		pEditorData->pRenderData->pBuffer = Rendering::CreateEditorBuffer(PALETTE_COLOR_COUNT, editedAsset.data);
		pEditorData->pRenderData->pTexture = Rendering::CreateEditorTexture(CHR_DIM_PIXELS, CHR_DIM_PIXELS, EDITOR_TEXTURE_USAGE_CHR, nullptr, pEditorData->pRenderData->pBuffer);
		pContext->tempRenderData.insert(pEditorData->pRenderData);
	}

	Rendering::UpdateEditorBuffer(pEditorData->pRenderData->pBuffer, editedAsset.data);
}

static void DeletePaletteEditorData(EditedAsset& editedAsset) {
	PaletteEditorData* pEditorData = (PaletteEditorData*)editedAsset.userData;

	pContext->tempEraseList.push_back(pEditorData->pRenderData);

	// NOTE: This does not free the render data, so it's potentially leaky
	// Render data should be freed later when iterating the erase list
	delete editedAsset.userData;
}

static void DrawPaletteEditor(EditedAsset& asset) {
	ImGui::BeginChild("Palette editor");

	Palette* pPalette = (Palette*)asset.data;
	PaletteEditorData* pEditorData = (PaletteEditorData*)asset.userData;

	EditorRenderBuffer* pChrBuffer = GetChrRenderBuffer(pEditorData->selectedChrBank.id);
	Rendering::UpdateEditorTexture(pEditorData->pRenderData->pTexture, pChrBuffer, pEditorData->pRenderData->pBuffer);

	ImGui::SeparatorText("Properties");
	if (ImGui::InputText("Name", asset.name, MAX_ASSET_NAME_LENGTH)) {
		asset.dirty = true;
	}

	ImGui::SeparatorText("Palette");

	const s32 selectedColorIndex = pEditorData->selectedColor >= 0 ? pPalette->colors[pEditorData->selectedColor] : -1;

	{
		ImGui::PushID("Color table");
		constexpr ImVec2 gridSize = ImVec2(512, 256);
		s32 newIndex = selectedColorIndex;
		ImVec2 gridPos = DrawColorGrid(gridSize, &newIndex);
		DrawTileGridSelection(gridPos, gridSize, 32, newIndex);

		if (newIndex != selectedColorIndex && newIndex >= 0) {
			pPalette->colors[pEditorData->selectedColor] = newIndex;
			Rendering::UpdateEditorBuffer(pEditorData->pRenderData->pBuffer, pPalette);
			asset.dirty = true;
		}
		ImGui::PopID();
	}
	ImGui::Separator();
	{
		ImGui::PushID("Palette");
		constexpr ImVec2 gridSize = ImVec2(512, 64);
		ImVec2 gridPos = DrawTileGrid(gridSize, 64, &pEditorData->selectedColor);

		for (u32 i = 0; i < PALETTE_COLOR_COUNT; i++) {
			const u32 color = pPalette->colors[i];
			const u32 x = color % 16;
			const u32 y = color / 16;

			const ImVec2 tilePos = ImVec2(gridPos.x + i * 64, gridPos.y);
			const ImVec2 uvMin = ImVec2(x / 16.0f, y / 8.0f);
			const ImVec2 uvMax = ImVec2((x + 1) / 16.0f, (y + 1) / 8.0f);

			ImTextureID textureId = GetTextureID(pContext->pColorTexture);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddImage(textureId, tilePos, ImVec2(tilePos.x + 64, tilePos.y + 64), uvMin, uvMax);
			
			char indexStr[8];
			sprintf(indexStr, "0x%02x", color);
			drawList->AddText(ImVec2(tilePos.x + 8.0f, tilePos.y + 8.0f), IM_COL32(255, 255, 255, 255), indexStr);
		}

		DrawTileGridSelection(gridPos, gridSize, 64, pEditorData->selectedColor);
		ImGui::PopID();
	}

	ImGui::SeparatorText("Preview");
	{
		constexpr r32 gridSizePixels = 256.0f;
		DrawEditedChrSheet(gridSizePixels, GetTextureID(pEditorData->pRenderData->pTexture), pPalette->colors[0]);
		DrawAssetField("Pattern", ASSET_TYPE_CHR_BANK, pEditorData->selectedChrBank.id);
	}

	ImGui::EndChild();
}

static void DrawPaletteWindow() {
	static AssetEditorState state{};
	DrawAssetEditor("Palette editor", pContext->paletteWindowOpen, ASSET_TYPE_PALETTE, "New palette", DrawPaletteEditor, state, PopulatePaletteEditorData, nullptr, DeletePaletteEditorData);
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
			if (ImGui::MenuItem("Audio")) {
				pContext->audioWindowOpen = true;
			}
			if (ImGui::MenuItem("Asset browser")) {
				pContext->assetBrowserOpen = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Metasprite editor")) {
				pContext->spriteWindowOpen = true;
			}
			if (ImGui::MenuItem("Animation editor")) {
				pContext->animationWindowOpen = true;
			}
			if (ImGui::MenuItem("Tileset editor")) {
				pContext->tilesetWindowOpen = true;
			}
			if (ImGui::MenuItem("Room editor")) {
				pContext->roomWindowOpen = true;
			}
			if (ImGui::MenuItem("Dungeon editor")) {
				pContext->dungeonWindowOpen = true;
			}
			if (ImGui::MenuItem("Overworld editor")) {
				pContext->overworldWindowOpen = true;
			}
			if (ImGui::MenuItem("Actor editor")) {
				pContext->actorWindowOpen = true;
			}
			if (ImGui::MenuItem("Sound editor")) {
				pContext->soundWindowOpen = true;
			}
			if (ImGui::MenuItem("Pattern editor")) {
				pContext->chrWindowOpen = true;
			}
			if (ImGui::MenuItem("Palette editor")) {
				pContext->paletteWindowOpen = true;
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
	Debug::HookEditorDebugLog(ConsoleLog);
	assert(pContext != nullptr);
}

void Editor::Init(SDL_Window *pWindow) {
	ImGui::CreateContext();
	Rendering::InitEditor(pWindow);

	// Legacy CHR texture (keeping for now during transition)
	constexpr u32 sheetPaletteCount = PALETTE_COUNT / 2;
	pContext->pChrTexture = Rendering::CreateEditorTexture(CHR_DIM_PIXELS * sheetPaletteCount, CHR_DIM_PIXELS * CHR_COUNT, EDITOR_TEXTURE_USAGE_CHR);
	
	// Create indexed CHR textures and buffers for each sheet
	for (u32 i = 0; i < CHR_COUNT; i++) {
		pContext->pIndexedChrTextures[i] = Rendering::CreateEditorTexture(CHR_DIM_PIXELS, CHR_DIM_PIXELS, EDITOR_TEXTURE_USAGE_CHR_INDEXED);
		pContext->pChrBuffers[i] = Rendering::CreateEditorBuffer(sizeof(ChrSheet));
	}
	
	pContext->pPaletteTexture = Rendering::CreateEditorTexture(PALETTE_MEMORY_SIZE, 1, EDITOR_TEXTURE_USAGE_PALETTE);
	pContext->pColorTexture = Rendering::CreateEditorTexture(16, 8, EDITOR_TEXTURE_USAGE_COLOR);
}

void Editor::Free() {
	Rendering::FreeEditorTexture(pContext->pChrTexture); // Legacy
	
	// Free indexed CHR textures and buffers
	for (u32 i = 0; i < CHR_COUNT; i++) {
		Rendering::FreeEditorTexture(pContext->pIndexedChrTextures[i]);
		Rendering::FreeEditorBuffer(pContext->pChrBuffers[i]);
	}
	
	Rendering::FreeEditorTexture(pContext->pPaletteTexture);
	Rendering::FreeEditorTexture(pContext->pColorTexture);

	Rendering::ShutdownEditor();
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
#ifdef PLATFORM_WINDOWS
	vsprintf_s(s, fmt, args);
#else
	vsprintf(s, fmt, args);
#endif
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

void Editor::Update(r64 dt) {
	pContext->secondsElapsed += dt;

	// Clean up deleted textures
	for (auto pTempData : pContext->tempEraseList) {
		pContext->tempRenderData.erase(pTempData);
		Rendering::FreeEditorTexture(pTempData->pTexture);
		Rendering::FreeEditorBuffer(pTempData->pBuffer);
		free(pTempData);
	}
	pContext->tempEraseList.clear();

	for (auto id : pContext->bufferEraseList) {
		EditorRenderBuffer* pBuffer = pContext->assetRenderBuffers.at(id);
		Rendering::FreeEditorBuffer(pBuffer);
		pContext->assetRenderBuffers.at(id) = nullptr;
	}
	pContext->bufferEraseList.clear();

	Rendering::BeginEditorFrame();
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

	if (pContext->overworldWindowOpen) {
		DrawOverworldWindow();
	}

	if (pContext->assetBrowserOpen) {
		DrawAssetBrowser();
	}

	if (pContext->animationWindowOpen) {
		DrawAnimationWindow();
	}

	if (pContext->soundWindowOpen) {
		DrawSoundWindow();
	}

	if (pContext->chrWindowOpen) {
		DrawChrWindow();
	}

	if (pContext->paletteWindowOpen) {
		DrawPaletteWindow();
	}
}

void Editor::SetupFrame() {
	// Legacy CHR texture rendering (keeping for now during transition)
	Rendering::RenderEditorTexture(pContext->pChrTexture);
	
	// Render indexed CHR textures
	for (u32 i = 0; i < CHR_COUNT; i++) {
		ChrSheet* pChrSheet = Rendering::GetChrPtr(i);
		if (pChrSheet) {
			// Update the cached buffer with current CHR data
			Rendering::UpdateEditorBuffer(pContext->pChrBuffers[i], pChrSheet);
			// Render the indexed texture
			Rendering::RenderEditorTextureIndexed(pContext->pIndexedChrTextures[i], pContext->pChrBuffers[i]);
		}
	}
	
	Rendering::RenderEditorTexture(pContext->pPaletteTexture);

	for (auto pTempData : pContext->tempRenderData) {
		Rendering::RenderEditorTexture(pTempData->pTexture);
	}
}

void Editor::Render() {
	ImGui::Render();
	Rendering::RenderEditor();
}
#pragma endregion