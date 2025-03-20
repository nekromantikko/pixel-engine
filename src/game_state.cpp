#include "game_state.h"
#include "game_rendering.h"
#include "rendering_util.h"
#include "game_input.h"
#include "game_ui.h"
#include "coroutines.h"
#include "dialog.h"
#include "room.h"
#include "collision.h"
#include "actor_prototypes.h"
#include "dungeon.h"
#include "overworld.h"

// TODO: Define in editor in game settings 
constexpr s32 playerPrototypeIndex = 0;
constexpr s32 playerOverworldPrototypeIndex = 0x03;
constexpr s32 xpRemnantPrototypeIndex = 0x0c;

// Map drawing
static constexpr glm::ivec2 mapViewportOffset = { 7, 4 };
static constexpr glm::ivec2 mapDialogViewportOffset = mapViewportOffset - 1;
static constexpr glm::ivec2 mapSize = { 18, 10 };
static constexpr glm::ivec2 mapCenter = mapSize / 2;
static constexpr glm::ivec2 mapSizeTiles = mapSize * s32(METATILE_DIM_TILES);
static constexpr glm::ivec2 mapSizeScreens = { mapSize.x, mapSizeTiles.y };
static constexpr glm::ivec2 mapCenterScreens = mapSizeScreens / 2;
static constexpr glm::ivec2 mapDialogSize = mapSize + 2;
static constexpr glm::vec2 mapScrollMin = { -1, -1 };
static constexpr glm::vec2 mapScrollMax = { (DUNGEON_GRID_DIM - mapSizeScreens.x) * 2 + 1, DUNGEON_GRID_DIM - mapSizeScreens.y + 1 };
static constexpr u16 mapScrollRate = 1;
static constexpr u16 mapScrollSteps = TILE_DIM_PIXELS * mapScrollRate;
static constexpr r32 mapScrollStepLength = 1.0f / mapScrollSteps;
static u16 mapScrollCounter = 0;
static glm::vec2 mapScrollOffset(0);
static glm::vec2 mapScrollDir(0);

static GameData gameData;
static GameState state;

// TODO: Store bitfield instead
static bool discoveredScreens[DUNGEON_GRID_SIZE]{};

static s32 currentDungeonIndex;
static glm::i8vec2 currentRoomOffset;
static const RoomInstance* pCurrentRoom;

static u8 currentOverworldArea;
static glm::ivec2 overworldAreaEnterDir;

// 16ms Frames elapsed while not paused
static u32 gameplayFramesElapsed = 0;
static bool freezeGameplay = false;

#pragma region Callbacks
static void ReviveDeadActor(u64 id, PersistedActorData& persistedData) {
    persistedData.dead = false;
}
#pragma endregion

// TODO: Move somewhere else
#pragma region Dungeon utils
static const DungeonCell* GetDungeonCell(const Dungeon* pDungeon, const glm::i8vec2 offset) {
    if (offset.x < 0 || offset.x >= DUNGEON_GRID_DIM || offset.y < 0 || offset.y >= DUNGEON_GRID_DIM) {
        return nullptr;
    }
    
    const u32 cellIndex = offset.x + offset.y * DUNGEON_GRID_DIM;
    return &pDungeon->grid[cellIndex];
}

static const DungeonCell* GetDungeonCell(s32 dungeonIndex, const glm::i8vec2 offset) {
    const Dungeon* pDungeon = Assets::GetDungeon(dungeonIndex);
    if (!pDungeon) {
        return nullptr;
    }

    return GetDungeonCell(pDungeon, offset);
}

static const RoomInstance* GetDungeonRoom(s32 dungeonIndex, const glm::i8vec2 offset, glm::i8vec2* pOutRoomOffset = nullptr) {
    const Dungeon* pDungeon = Assets::GetDungeon(dungeonIndex);
    if (!pDungeon) {
        return nullptr;
    }

    const DungeonCell* pCell = GetDungeonCell(pDungeon, offset);
    if (!pCell) {
        return nullptr;
    }

    if (pCell->roomIndex < 0 || pCell->roomIndex >= MAX_DUNGEON_ROOM_COUNT) {
        return nullptr;
    }

    const RoomInstance& result = pDungeon->rooms[pCell->roomIndex];

    if (pOutRoomOffset) {
        const s8 xScreen = pCell->screenIndex % ROOM_MAX_DIM_SCREENS;
        const s8 yScreen = pCell->screenIndex / ROOM_MAX_DIM_SCREENS;
        *pOutRoomOffset = glm::i8vec2(offset.x - xScreen, offset.y - yScreen);
    }

    return &result;
}

static const glm::i8vec2 RoomPosToDungeonGridOffset(const glm::i8vec2& roomOffset, const glm::vec2 pos) {
    glm::i8vec2 gridOffset;
    gridOffset.x = roomOffset.x + s32(pos.x / VIEWPORT_WIDTH_METATILES);
    gridOffset.y = roomOffset.y + s32(pos.y / VIEWPORT_HEIGHT_METATILES);
    return gridOffset;
}

static const RoomTemplate* GetCurrentRoomTemplate() {
    if (!pCurrentRoom) {
        return nullptr;
    }

    return Assets::GetRoomTemplate(pCurrentRoom->templateIndex);
}
#pragma endregion

#pragma region Dungeon Gameplay
static void CorrectPlayerSpawnY(const RoomTemplate* pTemplate, Actor* pPlayer) {
    HitResult hit{};

    const r32 dy = VIEWPORT_HEIGHT_METATILES / 2.0f;  // Sweep downwards to find a floor

    Collision::SweepBoxVertical(&pTemplate->tilemap, pPlayer->pPrototype->hitbox, pPlayer->position, dy, hit);
    while (hit.startPenetrating) {
        pPlayer->position.y -= 1.0f;
        Collision::SweepBoxVertical(&pTemplate->tilemap, pPlayer->pPrototype->hitbox, pPlayer->position, dy, hit);
    }
    pPlayer->position = hit.location;
}

static bool ActorIsCheckpoint(const Actor* pActor) {
    return pActor->pPrototype->type == ACTOR_TYPE_INTERACTABLE && pActor->pPrototype->subtype == INTERACTABLE_TYPE_CHECKPOINT;
}

static bool SpawnPlayerAtCheckpoint() {
    Actor* pCheckpoint = Game::GetFirstActor(ActorIsCheckpoint);
    if (pCheckpoint == nullptr) {
        return false;
    }

    Actor* pPlayer = Game::SpawnActor(playerPrototypeIndex, pCheckpoint->position);
    if (pPlayer) {
        pPlayer->state.playerState.flags.mode = PLAYER_MODE_SITTING;
        return true;
    }

    return false;
}

static bool SpawnPlayerAtEntrance(const glm::i8vec2 screenOffset, u8 direction) {
    if (direction == SCREEN_EXIT_DIR_DEATH_WARP) {
        // Restore life
        Game::SetPlayerHealth(Game::GetPlayerMaxHealth());

        return SpawnPlayerAtCheckpoint();
    }

    u8 screenIndex = 0;
    const DungeonCell* pCell = GetDungeonCell(currentDungeonIndex, currentRoomOffset + screenOffset);
    if (pCell) {
        screenIndex = pCell->screenIndex;
    }

    r32 x = (screenIndex % ROOM_MAX_DIM_SCREENS) * VIEWPORT_WIDTH_METATILES;
    r32 y = (screenIndex / ROOM_MAX_DIM_SCREENS) * VIEWPORT_HEIGHT_METATILES;

    Actor* pPlayer = Game::SpawnActor(playerPrototypeIndex, glm::vec2(x, y));
    if (pPlayer == nullptr) {
        return false;
    }

    constexpr r32 initialHSpeed = 0.0625f;
    const RoomTemplate* pTemplate = GetCurrentRoomTemplate();

    switch (direction) {
    case SCREEN_EXIT_DIR_RIGHT: {
        pPlayer->position.x += 0.5f;
        pPlayer->position.y += VIEWPORT_HEIGHT_METATILES / 2.0f;
        pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
        pPlayer->velocity.x = initialHSpeed;
        CorrectPlayerSpawnY(pTemplate, pPlayer);
        break;
    }
    case SCREEN_EXIT_DIR_LEFT: {
        pPlayer->position.x += VIEWPORT_WIDTH_METATILES - 0.5f;
        pPlayer->position.y += VIEWPORT_HEIGHT_METATILES / 2.0f;
        pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
        pPlayer->velocity.x = -initialHSpeed;
        CorrectPlayerSpawnY(pTemplate, pPlayer);
        break;
    }
    case SCREEN_EXIT_DIR_BOTTOM: {
        pPlayer->position.x += VIEWPORT_WIDTH_METATILES / 2.0f;
        pPlayer->position.y += 0.5f;
        pPlayer->velocity.y = 0.25f;
        break;
    }
    case SCREEN_EXIT_DIR_TOP: {
        pPlayer->position.x += VIEWPORT_WIDTH_METATILES / 2.0f;
        pPlayer->position.y += VIEWPORT_HEIGHT_METATILES - 0.5f;
        pPlayer->velocity.y = -0.25f;
        break;
    }
    default:
        break;
    }

    pPlayer->state.playerState.flags.mode = PLAYER_MODE_ENTERING;
    pPlayer->state.playerState.modeTransitionCounter = 15;

    return true;
}

static void ViewportFollowPlayer() {
    Actor* pPlayer = Game::GetPlayer();
    if (!pPlayer) {
        return;
    }

    constexpr glm::vec2 viewportScrollThreshold = { 4.0f, 3.0f };

    const glm::vec2 viewportPos = Game::Rendering::GetViewportPos();
    const glm::vec2 viewportCenter = viewportPos + glm::vec2{ VIEWPORT_WIDTH_METATILES / 2.0f, VIEWPORT_HEIGHT_METATILES / 2.0f };
    const glm::vec2 targetOffset = pPlayer->position - viewportCenter;

    glm::vec2 delta = { 0.0f, 0.0f };
    if (targetOffset.x > viewportScrollThreshold.x) {
        delta.x = targetOffset.x - viewportScrollThreshold.x;
    }
    else if (targetOffset.x < -viewportScrollThreshold.x) {
        delta.x = targetOffset.x + viewportScrollThreshold.x;
    }

    if (targetOffset.y > viewportScrollThreshold.y) {
        delta.y = targetOffset.y - viewportScrollThreshold.y;
    }
    else if (targetOffset.y < -viewportScrollThreshold.y) {
        delta.y = targetOffset.y + viewportScrollThreshold.y;
    }

    Game::Rendering::SetViewportPos(viewportPos + delta);
}

static void StepDungeonGameplayFrame() {
	if (!freezeGameplay) {
        gameplayFramesElapsed++;

        // Pause menu
        if (Game::Input::ButtonPressed(BUTTON_START)) {
            if (Game::OpenDialog(mapDialogViewportOffset, mapDialogSize, { mapDialogSize.x, 0 })) {
                mapScrollDir = glm::vec2(0.0f);

                // Center map around player
                const Actor* pPlayer = Game::GetPlayer();
                if (pPlayer) {
                    const glm::ivec2 playerGridPos = Game::GetDungeonGridCell(pPlayer->position);
                    mapScrollOffset = glm::vec2((playerGridPos.x - mapCenterScreens.x) * 2 + 1, playerGridPos.y - mapCenterScreens.y);
                    mapScrollOffset.x = glm::clamp(mapScrollOffset.x, mapScrollMin.x, mapScrollMax.x);
                    mapScrollOffset.y = glm::clamp(mapScrollOffset.y, mapScrollMin.y, mapScrollMax.y);
                }

                state = GAME_STATE_DUNGEON_MAP;
            }
        }
		
        Game::UpdateActors();
        ViewportFollowPlayer();
        Game::UI::Update();
	}

    Game::Rendering::ClearSpriteLayers();
    Game::DrawActors();

    // Draw HUD
    Game::UI::DrawPlayerHealthBar(gameData.playerMaxHealth);
    Game::UI::DrawPlayerStaminaBar(gameData.playerMaxStamina);
    Game::UI::DrawExpCounter();
}
#pragma endregion

#pragma region Dungeon map
// TODO: Move somewhere else?
static u32 Hash(u32 input) {
    // Multiply with fibonacci primes
    input = (input ^ (input >> 16)) * 0xb11924e1;
    input = (input ^ (input >> 16)) * 0x19d699a5;
    input = input ^ (input >> 16);

    return input;
}

static bool DrawMapIcon(const glm::i8vec2 gridCell, u8 tileId, u8 palette, const glm::ivec2& scrollOffset, const glm::vec4& worldBounds) {
    const glm::ivec2 gridPos(gridCell.x * 2, gridCell.y); // Width = 2
    const glm::vec2 mapOffset = glm::vec2(gridPos - scrollOffset) / r32(METATILE_DIM_TILES);
    glm::vec2 worldPos = glm::vec2(worldBounds.x, worldBounds.y) + mapOffset;
    worldPos.x += 1.0f / (METATILE_DIM_TILES * 2);

    if (worldPos.x < worldBounds.x || worldPos.x >= worldBounds.z || worldPos.y < worldBounds.y || worldPos.y >= worldBounds.w) {
        return false;
    }

    const glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels(worldPos);

    Sprite sprite{};
    sprite.tileId = tileId;
    sprite.palette = palette;
    sprite.x = drawPos.x;
    sprite.y = drawPos.y;
    Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);

    return true;
}

static void DrawMapTile(Nametable* pNametables, const glm::ivec2& tileMin, const glm::ivec2& tileMax, const glm::ivec2& tilePos, const BgTile& tile) {
    if (tilePos.x >= tileMin.x && tilePos.x < tileMax.x && tilePos.y >= tileMin.y && tilePos.y < tileMax.y) {
        Rendering::Util::SetNametableTile(pNametables, tilePos, tile);
    }
}

static void DrawMap(const glm::ivec2 scrollOffset) {
    const glm::ivec2 viewportPos = Game::Rendering::GetViewportPos();
    const glm::ivec4 innerBounds = Game::GetDialogInnerBounds();
    const glm::ivec4 worldBounds = innerBounds + glm::ivec4(viewportPos, viewportPos);
    const glm::ivec4 worldTileBounds = worldBounds * s32(METATILE_DIM_TILES);

    const glm::ivec2 tileMin(worldTileBounds.x, worldTileBounds.y);
    const glm::ivec2 tileMax(worldTileBounds.z, worldTileBounds.w);

    const Dungeon* pDungeon = Assets::GetDungeon(currentDungeonIndex);
    if (!pDungeon) {
        return;
    }

    Nametable* pNametables = Rendering::GetNametablePtr(0);

    // Draw map background
    constexpr u16 borderIndexOffset = 0x110;
    const glm::ivec2 borderMin(tileMin - 1);
    const glm::ivec2 borderMax(tileMax + 1);
    for (int y = borderMin.y; y < borderMax.y; ++y) {
        for (int x = borderMin.x; x < borderMax.x; ++x) {
            u8 xEdge = ((x == borderMin.x) || (x == borderMax.x - 1)) ? 1 : 0;
            u8 yEdge = ((y == borderMin.y) || (y == borderMax.y - 1)) ? 2 : 0;
            bool hFlip = (x == borderMin.x);
            bool vFlip = (y == borderMin.y);
            u16 tileIndex = xEdge | yEdge;

            u16 raggedOffset = Hash((x - borderMin.x) ^ (y - borderMin.y));
            if ((raggedOffset & 7) != 7) {
                raggedOffset = 0;
            }
            else raggedOffset = (raggedOffset >> 8) & 12;

            tileIndex += raggedOffset;
            tileIndex += borderIndexOffset;

            BgTile tile{};
            tile.tileId = tileIndex;
            tile.palette = 0x03;
            tile.flipHorizontal = hFlip;
            tile.flipVertical = vFlip;
            Rendering::Util::SetNametableTile(pNametables, { x, y }, tile);
        }
    }

    for (u32 y = 0; y < DUNGEON_GRID_DIM; y++) {
        for (u32 x = 0; x < DUNGEON_GRID_DIM; x++) {
            const u32 cellIndex = x + y * DUNGEON_GRID_DIM;
            const DungeonCell& cell = pDungeon->grid[cellIndex];

            // Empty tile
            if (cell.roomIndex == -1) {
                continue;
            }

            const u32 yTile = y + tileMin.y - scrollOffset.y;
            // Vertical clipping
            if (yTile < tileMin.y || yTile >= tileMax.y) {
                continue;
            }

            // Exit tile
            if (cell.roomIndex < -1) {
                constexpr u16 rightExitTileIndex = 0x10a;
                constexpr u16 leftExitTileIndex = 0x10b;
                constexpr u16 bottomExitTileIndex = 0x10c;
                constexpr u16 topExitTileIndex = 0x10e;

                const u8 dir = ~(cell.roomIndex) - 1;

                const u32 xTile = (x * 2) + tileMin.x - scrollOffset.x;

                switch (dir) {
                case SCREEN_EXIT_DIR_RIGHT: {
                    if (!discoveredScreens[cellIndex - 1]) {
                        continue;
                    }

                    const BgTile tile{
                        .tileId = rightExitTileIndex,
                        .palette = 0x03,
                    };
                    if (xTile >= tileMin.x && xTile < tileMax.x) {
                        Rendering::Util::SetNametableTile(pNametables, { xTile, yTile }, tile);
                    }
                    continue;
                }
                case SCREEN_EXIT_DIR_LEFT: {
                    if (!discoveredScreens[cellIndex + 1]) {
                        continue;
                    }

                    const BgTile tile{
                        .tileId = leftExitTileIndex,
                        .palette = 0x03,
                    };
                    if (xTile + 1 >= tileMin.x && xTile + 1 < tileMax.x) {
                        Rendering::Util::SetNametableTile(pNametables, { xTile + 1, yTile }, tile);
                    }
                    continue;
                }
                case SCREEN_EXIT_DIR_BOTTOM: {
                    if (!discoveredScreens[cellIndex - DUNGEON_GRID_DIM]) {
                        continue;
                    }

                    BgTile tile{
                        .tileId = bottomExitTileIndex,
                        .palette = 0x03,
                    };
                    if (xTile >= tileMin.x && xTile < tileMax.x) {
                        Rendering::Util::SetNametableTile(pNametables, { xTile, yTile }, tile);
                    }
                    tile.tileId++;
                    if (xTile >= tileMin.x && xTile < tileMax.x) {
                        Rendering::Util::SetNametableTile(pNametables, { xTile + 1, yTile }, tile);
                    }
                    continue;
                }
                case SCREEN_EXIT_DIR_TOP: {
                    if (!discoveredScreens[cellIndex + DUNGEON_GRID_DIM]) {
                        continue;
                    }

                    BgTile tile{
                        .tileId = topExitTileIndex,
                        .palette = 0x03,
                    };
                    if (xTile >= tileMin.x && xTile < tileMax.x) {
                        Rendering::Util::SetNametableTile(pNametables, { xTile, yTile }, tile);
                    }
                    tile.tileId++;
                    if (xTile + 1 >= tileMin.x && xTile + 1 < tileMax.x) {
                        Rendering::Util::SetNametableTile(pNametables, { xTile + 1, yTile }, tile);
                    }
                    continue;
                }
                default:
                    continue;
                }
            }

            if (!discoveredScreens[cellIndex]) {
                continue;
            }

            const RoomInstance& roomInstance = pDungeon->rooms[cell.roomIndex];
            const RoomTemplate* pTemplate = Assets::GetRoomTemplate(roomInstance.templateIndex);

            u32 roomWidthScreens = pTemplate->width;
            u32 roomHeightScreens = pTemplate->height;

            const u32 roomX = cell.screenIndex % ROOM_MAX_DIM_SCREENS;
            const u32 roomY = cell.screenIndex / ROOM_MAX_DIM_SCREENS;

            // Room width = 2;
            for (u32 i = 0; i < 2; i++) {
                const u32 xTile = (x * 2) + tileMin.x + i - scrollOffset.x;

                // Clipping: Check if tile is within worldTileBounds
                if (xTile >= tileMin.x && xTile < tileMax.x) {
                    const u32 roomTileIndex = (roomX * 2 + i) + roomY * ROOM_MAX_DIM_SCREENS * 2;
                    const BgTile& tile = pTemplate->mapTiles[roomTileIndex];
                    Rendering::Util::SetNametableTile(pNametables, { xTile, yTile }, tile);
                }
            }
        }
    }

    const Actor* pPlayer = Game::GetPlayer();
    if (pPlayer) {
        const glm::ivec2 playerGridPos = Game::GetDungeonGridCell(pPlayer->position);
        DrawMapIcon(playerGridPos, 0xfc, 0x01, scrollOffset, worldBounds);
    }

    // Draw dropped exp with placeholder graphics
    if (gameData.expRemnant.dungeonIndex == currentDungeonIndex) {
        DrawMapIcon(gameData.expRemnant.gridOffset, 0x68, 0x00, scrollOffset, worldBounds);
    }

    if (!Game::IsDialogOpen()) {
        return;
    }

    // Draw scroll arrows
    Sprite sprite{};
    sprite.palette = 0x01;
    
    if (scrollOffset.x > mapScrollMin.x) {
        sprite.tileId = 0xf8;
        glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels({ worldBounds.x, worldBounds.y });
        drawPos.x += TILE_DIM_PIXELS * 0.5f;
        drawPos.y += mapCenter.y * s32(METATILE_DIM_PIXELS);

        sprite.x = drawPos.x;
        sprite.y = drawPos.y;
        sprite.flipVertical = true;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);

        sprite.y -= TILE_DIM_PIXELS;
        sprite.flipVertical = false;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);
    }
    if (scrollOffset.x < mapScrollMax.x) {
        sprite.tileId = 0xf8;
        glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels({ worldBounds.x, worldBounds.y });
        drawPos.x += (mapSize.x * s32(METATILE_DIM_PIXELS)) - TILE_DIM_PIXELS * 1.5f;
        drawPos.y += mapCenter.y * s32(METATILE_DIM_PIXELS);

        sprite.x = drawPos.x;
        sprite.y = drawPos.y;
        sprite.flipHorizontal = true;
        sprite.flipVertical = true;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);

        sprite.y -= TILE_DIM_PIXELS;
        sprite.flipVertical = false;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);
    }
    if (scrollOffset.y > mapScrollMin.y) {
        sprite.tileId = 0xf9;
        glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels({ worldBounds.x, worldBounds.y });
        drawPos.x += mapCenter.x * s32(METATILE_DIM_PIXELS);
        drawPos.y += TILE_DIM_PIXELS * 0.5f;

        sprite.x = drawPos.x;
        sprite.y = drawPos.y;
        sprite.flipHorizontal = true;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);

        sprite.x -= TILE_DIM_PIXELS;
        sprite.flipHorizontal = false;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);
    }
    if (scrollOffset.y < mapScrollMax.y) {
        sprite.tileId = 0xf9;
        glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels({ worldBounds.x, worldBounds.y });
        drawPos.x += mapCenter.x * s32(METATILE_DIM_PIXELS);
        drawPos.y += (mapSize.y * s32(METATILE_DIM_PIXELS)) - TILE_DIM_PIXELS * 1.5f;

        sprite.x = drawPos.x;
        sprite.y = drawPos.y;
        sprite.flipVertical = true;
        sprite.flipHorizontal = true;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);

        sprite.x -= TILE_DIM_PIXELS;
        sprite.flipHorizontal = false;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);
    }
}

static glm::vec2 GetScrollDir(u8 inputDir) {
    glm::vec2 result(0.0f);
    const u8 horizontalDir = inputDir & 3;
    const u8 verticalDir = (inputDir >> 2) & 3;

    if (horizontalDir) {
        const bool signBit = (horizontalDir >> 1) & 1;
        result.x = signBit ? -1.0f : 1.0f;
    }
    else if (verticalDir) {
        const bool signBit = (verticalDir >> 1) & 1;
        result.y = signBit ? -1.0f : 1.0f;
    }

    return result;
}

static void StepDungeonMap() {
    if (!Game::UpdateDialog()) {
        state = GAME_STATE_DUNGEON;
    }

    if (Game::Input::ButtonPressed(BUTTON_START)) {
        Game::CloseDialog();
    }
    
    if (Game::IsDialogOpen()) {
        if (mapScrollCounter > 0) {
            mapScrollCounter--;

            mapScrollOffset += mapScrollDir * mapScrollStepLength;
        }
        else {
            mapScrollOffset = glm::roundEven(mapScrollOffset);

            const u8 inputDir = (Game::Input::GetCurrentState() >> 8) & 0xf;
            if (inputDir) {
                mapScrollDir = GetScrollDir(inputDir);
                const glm::vec2 targetOffset = mapScrollOffset + mapScrollDir;
                if (targetOffset.x >= mapScrollMin.x && targetOffset.x <= mapScrollMax.x && targetOffset.y >= mapScrollMin.y && targetOffset.y <= mapScrollMax.y) {
                    mapScrollCounter = mapScrollSteps;
                }
            }
        }
    }

    Game::Rendering::ClearSpriteLayers();
    DrawMap(mapScrollOffset);

    // Draw HUD
    Game::UI::DrawPlayerHealthBar(gameData.playerMaxHealth);
    Game::UI::DrawPlayerStaminaBar(gameData.playerMaxStamina);
    Game::UI::DrawExpCounter();
}
#pragma endregion

#pragma region Overworld Gameplay
static void StepOverworldGameplayFrame() {
    if (!freezeGameplay) {
        gameplayFramesElapsed++;

        Game::UpdateActors();
        ViewportFollowPlayer();
        Game::UI::Update();
    }

    Game::Rendering::ClearSpriteLayers();
    Game::DrawActors();
}
#pragma endregion

#pragma region Coroutines
struct ScreenShakeState {
    const s16 magnitude;
    u16 duration;
};

static bool ShakeScreenCoroutine(void* userData) {
    ScreenShakeState& state = *(ScreenShakeState*)userData;

    if (state.duration == 0) {
        freezeGameplay = false;
        return false;
    }

    const r32 magnitudeMetatiles = r32(state.magnitude) / METATILE_DIM_PIXELS;
    glm::vec2 viewportPos = Game::Rendering::GetViewportPos();
    viewportPos.x += Random::GenerateReal(-magnitudeMetatiles, magnitudeMetatiles);
    viewportPos.y += Random::GenerateReal(-magnitudeMetatiles, magnitudeMetatiles);

    Game::Rendering::SetViewportPos(viewportPos);

    state.duration--;
    return true;
}

enum LevelTransitionStatus : u8 {
    TRANSITION_FADE_OUT,
    TRANSITION_LOADING,
    TRANSITION_FADE_IN,
    TRANSITION_COMPLETE
};

struct LevelTransitionState {
    s32 nextDungeon;
    glm::i8vec2 nextGridCell;
    u8 nextDirection;

    r32 progress = 0.0f;
    u8 status = TRANSITION_FADE_OUT;
    u8 holdTimer = 12;
};

static void UpdateFadeToBlack(r32 progress) {
    progress = glm::smoothstep(0.0f, 1.0f, progress);

    u8 colors[PALETTE_COLOR_COUNT];
    for (u32 i = 0; i < PALETTE_COUNT; i++) {
        Game::Rendering::GetPalettePresetColors(i, colors);
        for (u32 j = 0; j < PALETTE_COLOR_COUNT; j++) {
            u8 color = colors[j];

            const s32 baseBrightness = (color & 0b1110000) >> 4;
            const s32 hue = color & 0b0001111;

            const s32 minBrightness = hue == 0 ? 0 : -1;

            s32 newBrightness = glm::mix(minBrightness, baseBrightness, 1.0f - progress);

            if (newBrightness >= 0) {
                colors[j] = hue | (newBrightness << 4);
            }
            else {
                colors[j] = 0x00;
            }
        }
        Game::Rendering::WritePaletteColors(i, colors);
    }
}

static bool LevelTransitionCoroutine(void* userData) {
    LevelTransitionState* state = (LevelTransitionState*)userData;

    switch (state->status) {
    case TRANSITION_FADE_OUT: {
        if (state->progress < 1.0f) {
            state->progress += 0.1f;
            UpdateFadeToBlack(state->progress);
            return true;
        }
        state->status = TRANSITION_LOADING;
        break;
    }
    case TRANSITION_LOADING: {
        if (state->holdTimer > 0) {
            state->holdTimer--;
            return true;
        }
        Game::LoadRoom(state->nextDungeon, state->nextGridCell, state->nextDirection);
        state->status = TRANSITION_FADE_IN;
        freezeGameplay = false;
        break;
    }
    case TRANSITION_FADE_IN: {
        if (state->progress > 0.0f) {
            state->progress -= 0.10f;
            UpdateFadeToBlack(state->progress);
            return true;
        }
        state->status = TRANSITION_COMPLETE;
        break;
    }
    default:
        return false;
    }

    return true;
}
#pragma endregion

#pragma region GameData
// Initializes game data to new game state
void Game::InitGameData() {
    // TODO: Come up with actual values
	gameData.playerMaxHealth = 96;
    SetPlayerHealth(gameData.playerMaxHealth);
    gameData.playerMaxStamina = 64;
    SetPlayerStamina(gameData.playerMaxStamina);
    SetPlayerExp(0);
    SetPlayerWeapon(PLAYER_WEAPON_LAUNCHER);

	// TODO: Initialize first checkpoint

	gameData.expRemnant.dungeonIndex = -1;

	gameData.persistedActorData.Clear();
}
void Game::LoadGameData(u32 saveSlot) {
	// TODO: Load game data from save slot
}
void Game::SaveGameData(u32 saveSlot) {
	// TODO: Save game data to save slot
}
#pragma endregion

#pragma region Player
s16 Game::GetPlayerHealth() {
    return gameData.playerCurrentHealth;
}
s16 Game::GetPlayerMaxHealth() {
	return gameData.playerMaxHealth;
}
void Game::AddPlayerHealth(s16 health) {
    gameData.playerCurrentHealth += health;
    gameData.playerCurrentHealth = glm::clamp(gameData.playerCurrentHealth, s16(0), gameData.playerMaxHealth);
	Game::UI::SetPlayerDisplayHealth(gameData.playerCurrentHealth);
}
void Game::SetPlayerHealth(s16 health) {
    gameData.playerCurrentHealth = health;
    gameData.playerCurrentHealth = glm::clamp(gameData.playerCurrentHealth, s16(0), gameData.playerMaxHealth);
	Game::UI::SetPlayerDisplayHealth(gameData.playerCurrentHealth);
}
s16 Game::GetPlayerStamina() {
    return gameData.playerCurrentStamina;
}

s16 Game::GetPlayerMaxStamina() {
    return gameData.playerMaxStamina;
}

void Game::AddPlayerStamina(s16 stamina) {
    gameData.playerCurrentStamina += stamina;
    gameData.playerCurrentStamina = glm::clamp(gameData.playerCurrentStamina, s16(0), gameData.playerMaxStamina);
    Game::UI::SetPlayerDisplayStamina(gameData.playerCurrentStamina);
}

void Game::SetPlayerStamina(s16 stamina) {
    gameData.playerCurrentStamina = stamina;
    gameData.playerCurrentStamina = glm::clamp(gameData.playerCurrentStamina, s16(0), gameData.playerMaxStamina);
    Game::UI::SetPlayerDisplayStamina(gameData.playerCurrentStamina);
}
s16 Game::GetPlayerExp() {
    return gameData.playerExperience;
}
void Game::AddPlayerExp(s16 exp) {
	gameData.playerExperience += exp;
    gameData.playerExperience = glm::clamp(gameData.playerExperience, s16(0), s16(SHRT_MAX));
	Game::UI::SetPlayerDisplayExp(gameData.playerExperience);
}
void Game::SetPlayerExp(s16 exp) {
	gameData.playerExperience = exp;
	Game::UI::SetPlayerDisplayExp(gameData.playerExperience);
}
u16 Game::GetPlayerWeapon() {
    return gameData.playerWeapon;
}
void Game::SetPlayerWeapon(u16 weapon) {
    gameData.playerWeapon = weapon;
}
#pragma endregion

#pragma region ExpRemnant
void Game::ClearExpRemnant() {
    gameData.expRemnant.dungeonIndex = -1;
}
void Game::SetExpRemnant(const glm::vec2& position, u16 value) {
    gameData.expRemnant.dungeonIndex = currentDungeonIndex;
    gameData.expRemnant.gridOffset = RoomPosToDungeonGridOffset(currentRoomOffset, position);
	gameData.expRemnant.position = position;
	gameData.expRemnant.value = value;
}
#pragma endregion

#pragma region Checkpoint
Checkpoint Game::GetCheckpoint() {
    return gameData.checkpoint;
}
void Game::ActivateCheckpoint(const Actor* pCheckpoint) {
    if (pCheckpoint == nullptr) {
        return;
    }

    // Set checkpoint active
    PersistedActorData* pData = GetPersistedActorData(pCheckpoint->persistId);
    if (pData) {
        pData->activated = true;
    }
    else SetPersistedActorData(pCheckpoint->persistId, { .activated = true });

    // Set checkpoint data
    gameData.checkpoint = {
        .dungeonIndex = currentDungeonIndex,
        .gridOffset = RoomPosToDungeonGridOffset(currentRoomOffset, pCheckpoint->position)
    };

    // Revive dead actors
    gameData.persistedActorData.ForEach(ReviveDeadActor);
}
#pragma endregion

#pragma region Persisted actor data
PersistedActorData* Game::GetPersistedActorData(u64 id) {
    if (id == UUID_NULL || (id & (u64(0xFFFFFFFF) << 32)) == UUID_NULL) {
        return nullptr;
    }

    PersistedActorData* pPersistData = gameData.persistedActorData.Get(id);
    return pPersistData;
}
void Game::SetPersistedActorData(u64 id, const PersistedActorData& data) {
    if (id == UUID_NULL || (id & (u64(0xFFFFFFFF) << 32)) == UUID_NULL) {
        return;
    }

    PersistedActorData* pPersistData = gameData.persistedActorData.Get(id);
    if (pPersistData) {
        *pPersistData = data;
    }
    else {
        gameData.persistedActorData.Add(id, data);
    }
}
#pragma endregion

#pragma region Level
static u8 GetSidescrollingDir(glm::ivec2 facingDir, bool flip) {
    if (flip) {
        facingDir *= -1;
    }

    if (facingDir.x != 0) {
        return facingDir.x > 0 ? SCREEN_EXIT_DIR_RIGHT : SCREEN_EXIT_DIR_LEFT;
    }

    return facingDir.y > 0 ? SCREEN_EXIT_DIR_RIGHT : SCREEN_EXIT_DIR_LEFT;
}

static glm::ivec2 GetOverworldDir(const OverworldKeyArea& area, u8 direction) {
    glm::ivec2 result = overworldAreaEnterDir.x ? glm::ivec2(1, 0) : glm::ivec2(0, 1);

    if (direction == SCREEN_EXIT_DIR_LEFT || direction == SCREEN_EXIT_DIR_TOP) {
        result *= -1;
    }

    if (area.flipDirection) {
        result *= -1;
    }

    return result;
}

bool Game::LoadOverworld(u8 keyAreaIndex, u8 direction) {
    state = GAME_STATE_OVERWORLD;
    const Overworld* pOverworld = Assets::GetOverworld();

    Rendering::SetViewportPos(glm::vec2(0.0f), false);
    ClearActors();

    const OverworldKeyArea& area = pOverworld->keyAreas[keyAreaIndex];
    const glm::ivec2 overworldDir = GetOverworldDir(area, direction);
    glm::ivec2 spawnPos = area.position;
    if (area.passthrough) {
        spawnPos += overworldDir;
    }

    Actor* pPlayer = Game::SpawnActor(playerOverworldPrototypeIndex, spawnPos);
    if (!pPlayer) {
        return false;
    }
    pPlayer->state.playerOverworld.facingDir = overworldDir;

    gameplayFramesElapsed = 0;
    Rendering::RefreshViewport();

    return true;
}

void Game::EnterOverworldArea(u8 keyAreaIndex, const glm::ivec2& direction) {
    currentOverworldArea = keyAreaIndex;
    const Overworld* pOverworld = Assets::GetOverworld();
    const OverworldKeyArea& area = pOverworld->keyAreas[keyAreaIndex];

    overworldAreaEnterDir = direction;
    Game::TriggerLevelTransition(area.dungeonIndex, area.targetGridCell, GetSidescrollingDir(direction, area.flipDirection));
}

bool Game::LoadRoom(const RoomInstance* pRoom, const glm::i8vec2 screenOffset, u8 direction) {
    currentDungeonIndex = -1;
    currentRoomOffset = { 0,0 };

    pCurrentRoom = pRoom;

    return ReloadRoom(screenOffset, direction);
}

bool Game::LoadRoom(s32 dungeonIndex, const glm::i8vec2 gridCell, u8 direction) {
    glm::i8vec2 roomOffset;
    const RoomInstance* pNextRoom = GetDungeonRoom(dungeonIndex, gridCell, &roomOffset);
    if (!pNextRoom) {
        const DungeonCell* pCell = GetDungeonCell(dungeonIndex, gridCell);
        if (!pCell || pCell->roomIndex == -1) {
            return LoadOverworld(currentOverworldArea, direction);
        }

        return LoadOverworld(pCell->screenIndex, direction);
    }

    currentDungeonIndex = dungeonIndex;
    pCurrentRoom = pNextRoom;
    currentRoomOffset = roomOffset;

    const glm::i8vec2 screenOffset = gridCell - currentRoomOffset;
    return ReloadRoom(screenOffset, direction);
}

void Game::UnloadRoom() {
    Rendering::SetViewportPos(glm::vec2(0.0f), false);

	ClearActors();

    Rendering::RefreshViewport();
}

bool Game::ReloadRoom(const glm::i8vec2 screenOffset, u8 direction) {
    state = GAME_STATE_DUNGEON;
    UnloadRoom();

    const RoomTemplate* pTemplate = Assets::GetRoomTemplate(pCurrentRoom->templateIndex);

    for (u32 i = 0; i < pTemplate->actors.Count(); i++)
    {
        auto handle = pTemplate->actors.GetHandle(i);
        const RoomActor* pActor = pTemplate->actors.Get(handle);

        const u64 combinedId = pActor->id | (u64(pCurrentRoom->id) << 32);
        const PersistedActorData* pPersistData = gameData.persistedActorData.Get(combinedId);
        if (!pPersistData || !(pPersistData->dead || pPersistData->permaDead)) {
            SpawnActor(pActor, pCurrentRoom->id);
        }
    }

    // Spawn player
    SpawnPlayerAtEntrance(screenOffset, direction);
    ViewportFollowPlayer();

    // Spawn xp remnant, if it belongs in this room
    if (gameData.expRemnant.dungeonIndex == currentDungeonIndex && currentDungeonIndex >= 0) {
        const RoomInstance* pRoom = GetDungeonRoom(currentDungeonIndex, gameData.expRemnant.gridOffset);
        if (pRoom->id == pCurrentRoom->id) {
            Actor* pRemnant = SpawnActor(xpRemnantPrototypeIndex, gameData.expRemnant.position);
            pRemnant->state.pickupState.value = gameData.expRemnant.value;
        }
    }

    gameplayFramesElapsed = 0;
    Rendering::RefreshViewport();

    return true;
}

s32 Game::GetCurrentDungeon() {
    return currentDungeonIndex;
}

glm::i8vec2 Game::GetCurrentRoomOffset() {
    return currentRoomOffset;
}

const RoomInstance* Game::GetCurrentRoom() {
    return pCurrentRoom;
}

glm::i8vec2 Game::GetDungeonGridCell(const glm::vec2& worldPos) {
    const RoomTemplate* pTemplate = GetCurrentRoomTemplate();
    if (!pTemplate) {
        return { -1, -1 };
    }

    const glm::i8vec2 roomOffset = GetCurrentRoomOffset();
    glm::i8vec2 screenOffset = {
        s8(worldPos.x / VIEWPORT_WIDTH_METATILES),
        s8(worldPos.y / VIEWPORT_HEIGHT_METATILES)
    };
    screenOffset.x = glm::clamp(screenOffset.x, s8(0), s8(pTemplate->width - 1));
    screenOffset.y = glm::clamp(screenOffset.y, s8(0), s8(pTemplate->height - 1));

    return roomOffset + screenOffset;
}

void Game::DiscoverScreen(const glm::i8vec2 gridCell) {
    const u32 cellIndex = gridCell.x + gridCell.y * DUNGEON_GRID_DIM;
    discoveredScreens[cellIndex] = true;
}

glm::ivec2 Game::GetCurrentPlayAreaSize() {
    switch (state) {
    case GAME_STATE_DUNGEON:
    case GAME_STATE_DUNGEON_MAP: {
        if (!pCurrentRoom) {
            break;
        }

        const RoomTemplate* pTemplate = Assets::GetRoomTemplate(pCurrentRoom->templateIndex);
        return { pTemplate->width * VIEWPORT_WIDTH_METATILES, pTemplate->height * VIEWPORT_HEIGHT_METATILES };
    }
    case GAME_STATE_OVERWORLD: {
        const Overworld* pOverworld = Assets::GetOverworld();
        return { pOverworld->tilemap.width, pOverworld->tilemap.height };
    }
    default:
        break;
    }

    return glm::i8vec2(0);
}

const Tilemap* Game::GetCurrentTilemap() {
    switch (state) {
    case GAME_STATE_DUNGEON:
    case GAME_STATE_DUNGEON_MAP: {
        if (!pCurrentRoom) {
            break;
        }

        const RoomTemplate* pTemplate = Assets::GetRoomTemplate(pCurrentRoom->templateIndex);
        return &pTemplate->tilemap;
    }
    case GAME_STATE_OVERWORLD: {
        const Overworld* pOverworld = Assets::GetOverworld();
        return &pOverworld->tilemap;
    }
    default:
        break;
    }

    return nullptr;
}

u32 Game::GetFramesElapsed() {
	return gameplayFramesElapsed;
}
#pragma endregion

void Game::InitGameState(GameState initialState) {
    state = initialState;
}

void Game::StepFrame() {
    Input::Update();
    StepCoroutines();

	switch (state) {
	case GAME_STATE_TITLE:
		break;
    case GAME_STATE_OVERWORLD:
        StepOverworldGameplayFrame();
        break;
	case GAME_STATE_DUNGEON:
		StepDungeonGameplayFrame();
        break;
	case GAME_STATE_DUNGEON_MAP:
        StepDungeonMap();
		break;
	case GAME_STATE_GAME_OVER:
		break;
	case GAME_STATE_CREDITS:
        break;
	case GAME_STATE_EXIT:
		break;
    default:
        break;
	}
}

void Game::TriggerScreenShake(s16 magnitude, u16 duration, bool freeze) {
    freezeGameplay |= freeze;
    ScreenShakeState state = {
        .magnitude = magnitude,
        .duration = duration
    };
    StartCoroutine(ShakeScreenCoroutine, state);
}

void Game::TriggerLevelTransition(s32 targetDungeon, glm::i8vec2 targetGridCell, u8 enterDirection, void(*callback)()) {
    LevelTransitionState state = {
            .nextDungeon = targetDungeon,
            .nextGridCell = targetGridCell,
            .nextDirection = enterDirection,
    };
    StartCoroutine(LevelTransitionCoroutine, state, callback);
    freezeGameplay = true;
}