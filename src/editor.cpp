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

struct ChrTextures {
	ImTextureID textures[PALETTE_COUNT];
};

struct EditorContext {
	ChrTextures chrTextures[CHR_COUNT];
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
};

static EditorContext* pContext;

#pragma region Utils
// Sign-extend 9-bit unsigned sprite position
static s32 SignExtendSpritePos(u16 spritePos) {
	return (spritePos ^ 0x100) - 0x100;
}

static ImVec2 GetChrTileCoord(u8 index) {
	ImVec2 coord = ImVec2(index % CHR_DIM_TILES, index / CHR_DIM_TILES);
	return coord;
}

static ImVec2 ChrTileCoordToTexCoord(ImVec2 coord, u8 chrIndex) {
	return ImVec2(coord.x / CHR_DIM_TILES, coord.y / CHR_DIM_TILES);
}

static ImVec2 TexCoordToChrTileCoord(ImVec2 normalized) {
	ImVec2 tileCoord = ImVec2(glm::floor(normalized.x * 16), glm::floor(normalized.y * 16));
	return tileCoord;
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

static void DrawMetatile(const Metatile& metatile, ImVec2 pos, r32 size, s32 palette, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	const r32 tileSize = size / METATILE_DIM_TILES;

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	for (u32 i = 0; i < METATILE_TILE_COUNT; i++) {
		ImVec2 pMin = ImVec2(pos.x + (i & 1) * tileSize, pos.y + (i >> 1) * tileSize);
		ImVec2 pMax = ImVec2(pMin.x + tileSize, pMin.y + tileSize);

		ImVec2 chrCoord = GetChrTileCoord(metatile.tiles[i]);
		ImVec2 uvMin = ChrTileCoordToTexCoord(chrCoord, 0);
		ImVec2 uvMax = ChrTileCoordToTexCoord(ImVec2(chrCoord.x + 1, chrCoord.y + 1), 0);

		drawList->AddImage(pContext->chrTextures[0].textures[palette], pMin, pMax, uvMin, uvMax, color);
	}
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
	drawList->AddImage(pContext->chrTextures[index].textures[palette + index * 4], chrPos, ImVec2(chrPos.x + size, chrPos.y + size), ImVec2(0, 0), ImVec2(1, 1));
	if (selectedTile != nullptr && *selectedTile >= 0) {
		DrawTileGridSelection(chrPos, gridSize, gridStepPixels, *selectedTile);
	}
}

static void DrawTileset(const Tileset* pTileset, r32 size, s32* selectedMetatile) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const r32 renderScale = size / (TILESET_DIM * METATILE_DIM_PIXELS);
	const r32 gridStepPixels = METATILE_DIM_PIXELS * renderScale;

	const ImVec2 gridSize = ImVec2(size, size);
	const ImVec2 chrPos = DrawTileGrid(gridSize, gridStepPixels, selectedMetatile);

	for (s32 i = 0; i < TILESET_SIZE; i++) {
		ImVec2 metatileCoord = ImVec2(i % TILESET_DIM, i / TILESET_DIM);
		ImVec2 metatileOffset = ImVec2(chrPos.x + metatileCoord.x * gridStepPixels, chrPos.y + metatileCoord.y * gridStepPixels);

		const Metatile& metatile = pTileset->tiles[i].metatile;
		const s32 palette = Tiles::GetTilesetPalette(pTileset, i);
		DrawMetatile(metatile, metatileOffset, renderScale * TILESET_DIM, palette);
	}
	if (selectedMetatile != nullptr && *selectedMetatile >= 0) {
		DrawTileGridSelection(chrPos, gridSize, gridStepPixels, *selectedMetatile);
	}
}

static void DrawSprite(const Sprite& sprite, const ImVec2& pos, r32 renderScale, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const r32 tileDrawSize = TILE_DIM_PIXELS * renderScale;

	const u8 index = (u8)sprite.tileId;
	ImVec2 tileCoord = GetChrTileCoord(index);
	ImVec2 tileStart = ChrTileCoordToTexCoord(tileCoord, 1);
	ImVec2 tileEnd = ChrTileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 1);

	const bool flipX = sprite.flipHorizontal;
	const bool flipY = sprite.flipVertical;
	const u8 palette = sprite.palette;

	drawList->AddImage(pContext->chrTextures[1].textures[4 + palette], pos, ImVec2(pos.x + tileDrawSize, pos.y + tileDrawSize), ImVec2(flipX ? tileEnd.x : tileStart.x, flipY ? tileEnd.y : tileStart.y), ImVec2(!flipX ? tileEnd.x : tileStart.x, !flipY ? tileEnd.y : tileStart.y), color);
}

static void DrawMetasprite(const Metasprite* pMetasprite, const ImVec2& origin, r32 renderScale, ImU32 color = IM_COL32(255, 255, 255, 255)) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const r32 tileDrawSize = TILE_DIM_PIXELS * renderScale;
	for (s32 i = pMetasprite->spriteCount - 1; i >= 0; i--) {
		const Sprite& sprite = pMetasprite->spritesRelativePos[i];
		const ImVec2 pos = ImVec2(origin.x + renderScale * SignExtendSpritePos(sprite.x), origin.y + renderScale * SignExtendSpritePos(sprite.y));
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
		result.min = { (r32)SignExtendSpritePos(sprite.x) / METATILE_DIM_PIXELS, (r32)SignExtendSpritePos(sprite.y) / METATILE_DIM_PIXELS };
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
			const glm::vec2 spriteMin = { (r32)SignExtendSpritePos(sprite.x) / METATILE_DIM_PIXELS, (r32)SignExtendSpritePos(sprite.y) / METATILE_DIM_PIXELS };
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
		ImVec2 pos = ImVec2(origin.x + renderScale * SignExtendSpritePos(sprite.x), origin.y + renderScale * SignExtendSpritePos(sprite.y));
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

static void SwapMetasprites(Metasprite* pMetasprites, s32 a, s32 b) {
	Metasprite& metaspriteA = pMetasprites[a];
	char* nameA = Metasprites::GetName(a);
	Metasprite& metaspriteB = pMetasprites[b];
	char* nameB = Metasprites::GetName(b);

	u32 tempSpriteCount = metaspriteA.spriteCount;
	char tempName[METASPRITE_MAX_NAME_LENGTH];
	Sprite tempSprites[METASPRITE_MAX_SPRITE_COUNT];

	memcpy(tempName, nameA, METASPRITE_MAX_NAME_LENGTH);
	memcpy(tempSprites, metaspriteA.spritesRelativePos, METASPRITE_MAX_SPRITE_COUNT * sizeof(Sprite));

	metaspriteA.spriteCount = metaspriteB.spriteCount;
	memcpy(nameA, nameB, METASPRITE_MAX_NAME_LENGTH);
	memcpy(metaspriteA.spritesRelativePos, metaspriteB.spritesRelativePos, METASPRITE_MAX_SPRITE_COUNT * sizeof(Sprite));

	metaspriteB.spriteCount = tempSpriteCount;
	memcpy(nameB, tempName, METASPRITE_MAX_NAME_LENGTH);
	memcpy(metaspriteB.spritesRelativePos, tempSprites, METASPRITE_MAX_SPRITE_COUNT * sizeof(Sprite));
}

static void SwapLevels(Level* pLevels, s32 a, s32 b) {
	Level& levelA = pLevels[a];
	Screen* levelAScreens = levelA.tilemap.pScreens;
	char* levelAName = levelA.name;
	Level& levelB = pLevels[b];
	Screen* levelBScreens = levelB.tilemap.pScreens;
	char* levelBName = levelB.name;

	// Copy A to temp
	Level temp = levelA;

	Screen tempScreens[LEVEL_MAX_SCREEN_COUNT];
	char tempName[LEVEL_MAX_NAME_LENGTH];

	memcpy(tempScreens, levelAScreens, LEVEL_MAX_SCREEN_COUNT * sizeof(Screen));
	memcpy(tempName, levelAName, LEVEL_MAX_NAME_LENGTH);

	// Copy B to A (But keep pointers pointing in original location)
	levelA = levelB;
	levelA.tilemap.pScreens = levelAScreens;
	levelA.name = levelAName;
	memcpy(levelAScreens, levelBScreens, LEVEL_MAX_SCREEN_COUNT * sizeof(Screen));
	memcpy(levelAName, levelBName, LEVEL_MAX_NAME_LENGTH);

	// Copy Temp to B
	levelB = temp;
	levelB.tilemap.pScreens = levelBScreens;
	levelB.name = levelBName;
	memcpy(levelBScreens, tempScreens, LEVEL_MAX_SCREEN_COUNT * sizeof(Screen));
	memcpy(levelBName, tempName, LEVEL_MAX_NAME_LENGTH);
}

// Callback for displaying waveform in audio window
static r32 GetAudioSample(void* data, s32 idx) {
	return ((r32)((u8*)data)[idx]);
}

template <typename T>
static void SwapElements(T* elements, s32 a, s32 b) {
	T temp = elements[a];
	elements[a] = elements[b];
	elements[b] = temp;
}

template <typename T>
static bool TrySwapElements(T* elements, ImVector<s32>& elementIndices, s32 i, s32 dir, void (*swapFunction)(T*, s32, s32) = nullptr) {
	s32& elementIndex = elementIndices[i];
	s32 nextElementIndex = elementIndex + dir;

	if (elementIndices.contains(nextElementIndex)) {
		return false;
	}

	if (swapFunction == nullptr) {
		SwapElements(elements, elementIndex, nextElementIndex);
	}
	else {
		swapFunction(elements, elementIndex, nextElementIndex);
	}
	elementIndex += dir;

	return true;
}

template <typename T>
static void MoveElements(T* elements, ImVector<s32>& elementIndices, s32 step, void (*swapFunction)(T*, s32, s32) = nullptr) {
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

				if (TrySwapElements(elements, elementIndices, i, dir, swapFunction)) {
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
					MoveElements<Metasprite>(pMetasprites, vec, step, SwapMetasprites);
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
		u8 index = (u8)sprite.tileId;
		ImVec2 tileCoord = GetChrTileCoord(index);
		ImVec2 tileStart = ChrTileCoordToTexCoord(tileCoord, 1);
		ImVec2 tileEnd = ChrTileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 1);
		ImVec2 pos = ImVec2(origin.x + renderScale * SignExtendSpritePos(sprite.x), origin.y + renderScale * SignExtendSpritePos(sprite.y));
		bool flipX = sprite.flipHorizontal;
		bool flipY = sprite.flipVertical;
		u8 palette = sprite.palette;

		// Select sprite by clicking (Topmost sprite gets selected)
		bool spriteClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x >= pos.x && io.MousePos.x < pos.x + gridStepPixels && io.MousePos.y >= pos.y && io.MousePos.y < pos.y + gridStepPixels;
		if (spriteClicked) {
			trySelect = i;
		}

		bool selected = spriteSelection.contains(i);
		// Move sprite if dragged
		ImVec2 posWithDrag = selected ? ImVec2(pos.x + dragDelta.x, pos.y + dragDelta.y) : pos;

		drawList->AddImage(pContext->chrTextures[1].textures[4 + palette], posWithDrag, ImVec2(posWithDrag.x + gridStepPixels, posWithDrag.y + gridStepPixels), ImVec2(flipX ? tileEnd.x : tileStart.x, flipY ? tileEnd.y : tileStart.y), ImVec2(!flipX ? tileEnd.x : tileStart.x, !flipY ? tileEnd.y : tileStart.y));


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

		ImGui::Text("Position: (%d, %d)", SignExtendSpritePos(sprite.x), SignExtendSpritePos(sprite.y));

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
	ImVec2 tileCoord = GetChrTileCoord(index);

	r32 x1 = sprite.flipHorizontal ? tileCoord.x + 1 : tileCoord.x;
	r32 x2 = sprite.flipHorizontal ? tileCoord.x : tileCoord.x + 1;
	r32 y1 = sprite.flipVertical ? tileCoord.y + 1 : tileCoord.y;
	r32 y2 = sprite.flipVertical ? tileCoord.y : tileCoord.y + 1;

	ImVec2 tileStart = ChrTileCoordToTexCoord(ImVec2(x1, y1), 1);
	ImVec2 tileEnd = ChrTileCoordToTexCoord(ImVec2(x2, y2), 1);
	ImGuiStyle& style = ImGui::GetStyle();
	r32 itemHeight = ImGui::GetItemRectSize().y - style.FramePadding.y;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	const ImVec2 topLeft = ImVec2(ImGui::GetItemRectMin().x + 88, ImGui::GetItemRectMin().y + style.FramePadding.y);
	const ImVec2 btmRight = ImVec2(topLeft.x + itemHeight, topLeft.y + itemHeight);
	drawList->AddImage(pContext->chrTextures[1].textures[sprite.palette + 4], topLeft, btmRight, tileStart, tileEnd);
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
	const Pool<Actor>* actors = Game::GetActors();

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

	const Tilemap* pTilemap = &pLevel->tilemap;

	for (s32 y = screenStartY; y <= screenEndY; y++) {
		for (s32 x = screenStartX; x <= screenEndX; x++) {
			const glm::vec2 screenPixelPos = { x * VIEWPORT_WIDTH_PIXELS, y * VIEWPORT_HEIGHT_PIXELS };
			const ImVec2 pMin = ImVec2((screenPixelPos.x - viewportPixelPos.x) * renderScale + topLeft.x, (screenPixelPos.y - viewportPixelPos.y) * renderScale + topLeft.y);
			const ImVec2 pMax = ImVec2(pMin.x + viewportDrawSize.x, pMin.y + viewportDrawSize.y);

			const s32 i = x + y * pTilemap->width;

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
				snprintf(pNewActor->name, ACTOR_MAX_NAME_LENGTH, "New actor");
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

			MoveViewport(pViewport, pNametables, &pLevel->tilemap, dx, dy);
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
				const ImVec2 namePos = ImVec2(pMin.x, pMax.y + tileDrawSize / 4);
				drawList->AddText(namePos, IM_COL32(255, 255, 255, 255), pActor->name);
			}

			break;
		}
		case EDIT_MODE_TILES:
		{
			const Tilemap* pTilemap = &pLevel->tilemap;
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
						Tiles::SetMapTile(pTilemap, metatileWorldPos, metatileIndex);

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
			RefreshViewport(pViewport, pNametables, &pLevel->tilemap);
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

			s32 size[2] = { level.tilemap.width, level.tilemap.height };
			if (ImGui::InputInt2("Size", size)) {
				if (size[0] >= 1 && size[1] >= 1 && size[0] * size[1] <= LEVEL_MAX_SCREEN_COUNT) {
					level.tilemap.width = size[0];
					level.tilemap.height = size[1];
				}
			}

			// Screens
			/*if (ImGui::TreeNode("Screens")) {
				// TODO: Lay these out nicer
				u32 screenCount = level.width * level.height;
				for (u32 i = 0; i < screenCount; i++) {
					Screen& screen = level.screens[i];

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

			DrawTileset(level.tilemap.pTileset, ImGui::GetContentRegionAvail().x - style.WindowPadding.x, &newSelection);

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

				ImGui::SeparatorText(pActor->name);
				ImGui::InputText("Name", pActor->name, ACTOR_MAX_NAME_LENGTH);

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
						Screen& screen = level.screens[i];

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
				RefreshViewport(pViewport, pNametables, &pCurrentLevel->tilemap);
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
						MoveElements<Level>(pLevels, vec, step, SwapLevels);

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

	Rendering::CreateImGuiChrTextures(0, pContext->chrTextures[0].textures);
	Rendering::CreateImGuiChrTextures(1, pContext->chrTextures[1].textures);
	Rendering::CreateImGuiPaletteTexture(&pContext->paletteTexture);
	Rendering::CreateImGuiGameTexture(&pContext->gameViewTexture);
}

void Editor::Free() {
	Rendering::FreeImGuiChrTextures(0, pContext->chrTextures[0].textures);
	Rendering::FreeImGuiChrTextures(1, pContext->chrTextures[1].textures);
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

	ImGui::Render();
}
#pragma endregion