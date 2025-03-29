#include "dialog.h"
#include "rendering_util.h"
#include "tiles.h"
#include "game_state.h"
#include "game_rendering.h"
#include "game_input.h"
#include "room.h"
#include "asset_manager.h"

enum DialogState {
    DIALOG_CLOSED = 0,
    DIALOG_OPENING,
    DIALOG_OPEN,
    DIALOG_CLOSING
};

static DialogState state = DIALOG_CLOSED;

static glm::ivec2 viewportOffset;
static glm::ivec2 targetSize;
static glm::ivec2 initialSize;
static glm::ivec2 currentSize;

static u8 GetBoxTileId(u32 x, u32 y, u32 w, u32 h) {
    u8 xEdge = (x == 0 || x == w - 1) ? 1 : 0;
    u8 yEdge = (y == 0 || y == h - 1) ? 2 : 0; // 2 == 0b00000010
    return xEdge | yEdge;
}

static void CopyLevelTileToNametable(Nametable* pNametables, const Tilemap* pTilemap, const glm::ivec2& worldPos) {
    const u32 nametableIndex = Rendering::Util::GetNametableIndexFromMetatilePos(worldPos);
    const glm::ivec2 nametableOffset = Rendering::Util::GetNametableOffsetFromMetatilePos(worldPos);

    const s32 tilesetIndex = Tiles::GetTilesetTileIndex(pTilemap, worldPos);
    const TilesetTile* tile = Tiles::GetTilesetTile(pTilemap, tilesetIndex);

    const Metatile& metatile = tile->metatile;
    const Tileset* pTileset = Assets::GetTilemapTileset(pTilemap);
    Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset, metatile);
}

static void CopyBoxTileToNametable(Nametable* pNametables, const glm::ivec2& worldPos, const glm::ivec2& tileOffset, const glm::ivec2& sizeTiles, u8 palette) {
    const u32 nametableIndex = Rendering::Util::GetNametableIndexFromMetatilePos(worldPos);
    const glm::ivec2 nametableOffset = Rendering::Util::GetNametableOffsetFromMetatilePos(worldPos);

    constexpr u16 borderTileOffset = 256;
    // Construct a metatile
    Metatile metatile{};
    for (u32 y = 0; y < METATILE_DIM_TILES; y++) {
        for (u32 x = 0; x < METATILE_DIM_TILES; x++) {
            metatile.tiles[x + y * METATILE_DIM_TILES] = {
                .tileId = u16(borderTileOffset + GetBoxTileId(tileOffset.x + x, tileOffset.y + y, sizeTiles.x, sizeTiles.y)),
                .palette = palette
            };
        }
    }

    Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset, metatile);
}

static void DrawBgBoxAnimated() {
    const glm::vec2 viewportPos = Game::Rendering::GetViewportPos();
    
    const glm::ivec2 worldPos = viewportOffset + glm::ivec2(viewportPos);
    const glm::ivec2 sizeTiles(currentSize.x << 1, currentSize.y << 1);

    Nametable* pNametables = Rendering::GetNametablePtr(0);
    const Tilemap* pTilemap = Game::GetCurrentTilemap();

    for (u32 y = 0; y < targetSize.y; y++) {
        for (u32 x = 0; x < targetSize.x; x++) {

            const glm::ivec2 offset(x, y);

            if (x < currentSize.x && y < currentSize.y) {
                const glm::ivec2 tileOffset(x << 1, y << 1);
                CopyBoxTileToNametable(pNametables, worldPos + offset, tileOffset, sizeTiles, 0x3);
            }
            else {
                CopyLevelTileToNametable(pNametables, pTilemap, worldPos + offset);
            }
        }
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

        Rendering::Util::SetNametableTile(pNametables, { xTile, yTile }, { .tileId = u16(c) });
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

            Rendering::Util::SetNametableTile(pNametables, { xTile, yTile }, { .tileId = 0 });
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

static void CloseDialog() {
    //dialogState.active = false;
}

static void AdvanceDialogText() {
    /*if (!dialogState.active) {
        return;
    }

    // Stop the previous coroutine
    Game::StopCoroutine(dialogState.currentLineCoroutine);

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
        Game::StartCoroutine(AnimBgBoxCoroutine, state, CloseDialog);
        return;
    }
    else {
        ClearBgText(glm::ivec2(8, 3), glm::ivec2(16, 4));
        AnimTextState state{
            .pText = dialogState.pDialogueLines[dialogState.currentLine],
            .boxViewportPos = glm::ivec2(8,3),
            .boxSize = glm::ivec2(16,4),
        };
        dialogState.currentLineCoroutine = Game::StartCoroutine(AnimTextCoroutine, state);
    }

    dialogState.currentLine++;*/
}

// TODO: Allow multiple dialogs simultaneously?
bool Game::OpenDialog(const glm::ivec2& offset, const glm::ivec2& size, const glm::ivec2& inSize) {
    if (state != DIALOG_CLOSED) {
        return false;
    }

    state = DIALOG_OPENING;
    viewportOffset = offset;
    targetSize = size;
    initialSize = inSize;
    currentSize = inSize;

    return true;
}

void Game::CloseDialog() {
    if (state == DIALOG_OPEN) {
        state = DIALOG_CLOSING;
    }
}

bool Game::UpdateDialog() {
    if (state == DIALOG_OPENING) {
        if (currentSize.x < targetSize.x) {
            currentSize.x++;
        }
        if (currentSize.y < targetSize.y) {
            currentSize.y++;
        }

        DrawBgBoxAnimated();

        if (currentSize == targetSize) {
            state = DIALOG_OPEN;
        }
    }
    else if (state == DIALOG_CLOSING) {
        if (currentSize.x > initialSize.x) {
            currentSize.x--;
        }
        if (currentSize.y > initialSize.y) {
            currentSize.y--;
        }

        DrawBgBoxAnimated();

        if (currentSize == initialSize) {
            state = DIALOG_CLOSED;
        }
    }

    return state != DIALOG_CLOSED;
}
bool Game::IsDialogActive() {
	return state != DIALOG_CLOSED;
}

bool Game::IsDialogOpen() {
    return state == DIALOG_OPEN;
}

glm::ivec4 Game::GetDialogInnerBounds() {
    return glm::ivec4(
        viewportOffset.x + 1,
        viewportOffset.y + 1,
        viewportOffset.x + currentSize.x - 1,
        viewportOffset.y + currentSize.y - 1
        );
}