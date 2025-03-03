#include "dialog.h"
#include "rendering_util.h"
#include "tiles.h"
#include "game_state.h"
#include "game_rendering.h"
#include "level.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>
static DialogState dialogState;

static u8 GetBoxTileId(u32 x, u32 y, u32 w, u32 h) {
    const u8 offset = 0x10;
    u8 xIndex = 0;
    u8 yIndex = 0;

    if (w != 1) {
        xIndex = 0b01;
        xIndex <<= (x % w != 0) ? 1 : 0;
        xIndex += (x % w == w - 1) ? 1 : 0;
    }
    if (h != 1) {
        yIndex = 0b01;
        yIndex <<= (y % h != 0) ? 1 : 0;
        yIndex += (y % h == h - 1) ? 1 : 0;
    }

    const u8 index = xIndex + (yIndex << 2);
    return index + offset;
}

static void CopyLevelTileToNametable(Nametable* pNametables, const Tilemap* pTilemap, const glm::ivec2& worldPos) {
    const u32 nametableIndex = Tiles::GetNametableIndex(worldPos);
    const glm::ivec2 nametableOffset = Tiles::GetNametableOffset(worldPos);

    const s32 tilesetIndex = Tiles::GetTilesetTileIndex(pTilemap, worldPos);
    const TilesetTile* tile = Tiles::GetTilesetTile(pTilemap, tilesetIndex);

    const Metatile& metatile = tile->metatile;
    const s32 palette = Tiles::GetTilesetPalette(pTilemap->pTileset, tilesetIndex);
    Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset.x, nametableOffset.y, metatile, palette);
}

static void CopyBoxTileToNametable(Nametable* pNametables, const glm::ivec2& worldPos, const glm::ivec2& tileOffset, const glm::ivec2& sizeTiles, u8 palette) {
    const u32 nametableIndex = Tiles::GetNametableIndex(worldPos);
    const glm::ivec2 nametableOffset = Tiles::GetNametableOffset(worldPos);

    // Construct a metatile
    Metatile metatile{};
    metatile.tiles[0] = GetBoxTileId(tileOffset.x, tileOffset.y, sizeTiles.x, sizeTiles.y);
    metatile.tiles[1] = GetBoxTileId(tileOffset.x + 1, tileOffset.y, sizeTiles.x, sizeTiles.y);
    metatile.tiles[2] = GetBoxTileId(tileOffset.x, tileOffset.y + 1, sizeTiles.x, sizeTiles.y);
    metatile.tiles[3] = GetBoxTileId(tileOffset.x + 1, tileOffset.y + 1, sizeTiles.x, sizeTiles.y);

    Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset.x, nametableOffset.y, metatile, palette);
}

static void DrawBgBoxAnimated(const glm::ivec2& viewportOffset, const glm::ivec2& size, const glm::ivec2 maxSize, u8 palette) {
    const glm::vec2 viewportPos = Game::Rendering::GetViewportPos();
    
    const glm::ivec2 worldPos = viewportOffset + glm::ivec2(viewportPos);
    const glm::ivec2 sizeTiles(size.x << 1, size.y << 1);

    Nametable* pNametables = Rendering::GetNametablePtr(0);
	const Level* pLevel = Game::GetCurrentLevel();

    for (u32 y = 0; y < maxSize.y; y++) {
        for (u32 x = 0; x < maxSize.x; x++) {

            const glm::ivec2 offset(x, y);

            if (x < size.x && y < size.y) {
                const glm::ivec2 tileOffset(x << 1, y << 1);
                CopyBoxTileToNametable(pNametables, worldPos + offset, tileOffset, sizeTiles, palette);
            }
            else {
                CopyLevelTileToNametable(pNametables, pLevel->pTilemap, worldPos + offset);
            }
        }
    }
}

struct BgBoxAnimState {
    const glm::ivec2 viewportPos;
    const u32 width;
    const u32 maxHeight;
    const u8 palette;
    const s32 direction = 1;

    u32 height = 0;
};

static bool AnimBgBoxCoroutine(void* userData) {
    BgBoxAnimState& state = *(BgBoxAnimState*)userData;

    if (state.direction > 0) {
        if (state.height < state.maxHeight) {
            state.height++;
        }

        DrawBgBoxAnimated(state.viewportPos, glm::ivec2(state.width, state.height), glm::ivec2(state.width, state.maxHeight), state.palette);

        return state.height != state.maxHeight;
    }
    else {
        if (state.height > 0) {
            state.height--;
        }

        DrawBgBoxAnimated(state.viewportPos, glm::ivec2(state.width, state.height), glm::ivec2(state.width, state.maxHeight), state.palette);

        return state.height != 0;
    }
}

static void DrawBgText(const glm::ivec2& boxViewportOffset, const glm::ivec2& boxSize, const char* pText, u32 length) {
    const glm::vec2 viewportPos = Game::Rendering::GetViewportPos();
    const glm::ivec2 worldPos = boxViewportOffset + glm::ivec2(viewportPos);
    const glm::ivec2 worldTilePos(worldPos.x * METATILE_DIM_TILES, worldPos.y * METATILE_DIM_TILES);
    const glm::ivec2 innerSizeTiles((boxSize.x << 1) - 2, (boxSize.y << 1) - 2);

    const u32 xTileStart = worldTilePos.x + 1;
    const u32 yTileStart = worldTilePos.y + 1;

    u32 xTile = xTileStart;
    u32 yTile = yTileStart;

    Nametable* pNametables = Rendering::GetNametablePtr(0);

    for (u32 i = 0; i < length; i++) {
        const char c = pText[i];

        // Handle manual newlines
        if (c == '\n') {
            xTile = xTileStart; // Reset to the beginning of the line
            yTile++; // Move to the next line

            // Stop if we exceed the box height
            if (yTile >= yTileStart + innerSizeTiles.y) {
                break;
            }

            continue;
        }

        // Automatic newline if text exceedd box width
        if (xTile >= xTileStart + innerSizeTiles.x) {
            xTile = xTileStart;
            yTile++;

            if (yTile >= yTileStart + innerSizeTiles.y) {
                break;
            }
        }

        // TODO: These could be utils too
        const u32 nametableIndex = (xTile / NAMETABLE_WIDTH_TILES + yTile / NAMETABLE_HEIGHT_TILES) % NAMETABLE_COUNT;
        const glm::ivec2 nametableOffset(xTile % NAMETABLE_WIDTH_TILES, yTile % NAMETABLE_HEIGHT_TILES);
        const u32 nametableTileIndex = nametableOffset.x + nametableOffset.y * NAMETABLE_WIDTH_TILES;

        pNametables[nametableIndex].tiles[nametableTileIndex] = c;
        xTile++;
    }
}

static void ClearBgText(const glm::ivec2& boxViewportOffset, const glm::ivec2& boxSize) {
    const glm::vec2 viewportPos = Game::Rendering::GetViewportPos();
    const glm::ivec2 worldPos = boxViewportOffset + glm::ivec2(viewportPos);
    const glm::ivec2 worldTilePos(worldPos.x * METATILE_DIM_TILES, worldPos.y * METATILE_DIM_TILES);
    const glm::ivec2 innerSizeTiles((boxSize.x << 1) - 2, (boxSize.y << 1) - 2);

    const u32 xTileStart = worldTilePos.x + 1;
    const u32 yTileStart = worldTilePos.y + 1;

    Nametable* pNametables = Rendering::GetNametablePtr(0);

    for (u32 y = 0; y < innerSizeTiles.y; y++) {
        u32 yTile = yTileStart + y;

        for (u32 x = 0; x < innerSizeTiles.x; x++) {
            u32 xTile = xTileStart + x;

            const u32 nametableIndex = (xTile / NAMETABLE_WIDTH_TILES + yTile / NAMETABLE_HEIGHT_TILES) % NAMETABLE_COUNT;
            const glm::ivec2 nametableOffset(xTile % NAMETABLE_WIDTH_TILES, yTile % NAMETABLE_HEIGHT_TILES);
            const u32 nametableTileIndex = nametableOffset.x + nametableOffset.y * NAMETABLE_WIDTH_TILES;

            pNametables[nametableIndex].tiles[nametableTileIndex] = 0;
        }
    }
}

struct AnimTextState {
    const char* pText = nullptr;
    const glm::ivec2 boxViewportPos;
    const glm::ivec2 boxSize;

    u32 pos = 0;
};

static bool AnimTextCoroutine(void* userData) {
    AnimTextState& state = *(AnimTextState*)userData;

    if (state.pos <= strlen(state.pText)) {
        DrawBgText(state.boxViewportPos, state.boxSize, state.pText, state.pos);
        state.pos++;
        return true;
    }

    return false;
}

void Game::OpenDialog(const char* const* pDialogueLines, u32 lineCount) {
    if (dialogState.active) {
        return;
    }

    dialogState.active = true;
    dialogState.currentLine = 0;
    dialogState.pDialogueLines = pDialogueLines;
    dialogState.lineCount = lineCount;

    BgBoxAnimState state{
                .viewportPos = glm::ivec2(8,3),
                .width = 16,
                .maxHeight = 4,
                .palette = 3,
                .direction = 1,
    };
    StartCoroutine(AnimBgBoxCoroutine, state, AdvanceDialogText);
}
void Game::AdvanceDialogText() {
    if (!dialogState.active) {
        return;
    }

    // Stop the previous coroutine
    StopCoroutine(dialogState.currentLineCoroutine);

    if (dialogState.currentLine >= dialogState.lineCount) {
        // Close dialogue box, then end dialogue
        BgBoxAnimState state{
                .viewportPos = glm::ivec2(8,3),
                .width = 16,
                .maxHeight = 4,
                .palette = 3,
                .direction = -1,

                .height = 4
        };
        StartCoroutine(AnimBgBoxCoroutine, state, CloseDialog);
        return;
    }
    else {
        ClearBgText(glm::ivec2(8, 3), glm::ivec2(16, 4));
        AnimTextState state{
            .pText = dialogState.pDialogueLines[dialogState.currentLine],
            .boxViewportPos = glm::ivec2(8,3),
            .boxSize = glm::ivec2(16,4),
        };
        dialogState.currentLineCoroutine = StartCoroutine(AnimTextCoroutine, state);
    }

    dialogState.currentLine++;
}
void Game::CloseDialog() {
	dialogState.active = false;
}
bool Game::IsDialogActive() {
	return dialogState.active;
}
DialogState* Game::GetDialogState() {
	return &dialogState;
}