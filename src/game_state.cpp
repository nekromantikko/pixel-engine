#include "game_state.h"
#include "game_rendering.h"
#include "game.h"
#include "game_input.h"
#include "game_ui.h"
#include "coroutines.h"
#include "dialog.h"
#include "collision.h"
#include "asset_manager.h"
#include "debug.h"
#include "actors.h"
#include "rendering.h"
#include "tilemap.h"

// TODO: Use g_ prefix for globals or combine into larger state struct

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

static GameData g_gameData;
static GameState g_state;

// TODO: Store bitfield instead
static bool discoveredScreens[DUNGEON_GRID_SIZE]{};

static DungeonHandle currentDungeonId;
static glm::i8vec2 currentRoomOffset;

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

static const DungeonCell* GetDungeonCell(DungeonHandle dungeonHandle, const glm::i8vec2 offset) {
    const Dungeon* pDungeon = AssetManager::GetAsset(dungeonHandle);
    if (!pDungeon) {
        return nullptr;
    }

    return GetDungeonCell(pDungeon, offset);
}

static const RoomInstance* GetDungeonRoom(DungeonHandle dungeonHandle, const glm::i8vec2 offset, glm::i8vec2* pOutRoomOffset = nullptr) {
    const Dungeon* pDungeon = AssetManager::GetAsset(dungeonHandle);
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
    const RoomInstance* pCurrentRoom = Game::GetCurrentRoom();
    if (!pCurrentRoom) {
        return nullptr;
    }

    return AssetManager::GetAsset(pCurrentRoom->templateId);
}
#pragma endregion

#pragma region Dungeon Gameplay
static void CorrectPlayerSpawnY(const RoomTemplate* pTemplate, Actor* pPlayer) {
    HitResult hit{};

    const ActorPrototype* pPrototype = AssetManager::GetAsset(pPlayer->prototypeHandle);
    if (!pPrototype) {
        return;
    }

    const r32 dy = VIEWPORT_HEIGHT_METATILES / 2.0f;  // Sweep downwards to find a floor

    Collision::SweepBoxVertical(&pTemplate->tilemap, pPrototype->hitbox, pPlayer->position, dy, hit);
    while (hit.startPenetrating) {
        pPlayer->position.y -= 1.0f;
        Collision::SweepBoxVertical(&pTemplate->tilemap, pPrototype->hitbox, pPlayer->position, dy, hit);
    }
    pPlayer->position = hit.location;
}

static bool ActorIsCheckpoint(const Actor* pActor) {
    const ActorPrototype* pPrototype = AssetManager::GetAsset(pActor->prototypeHandle);
    if (!pPrototype) {
        return false;
    }

    return pPrototype->type == ACTOR_TYPE_INTERACTABLE && pPrototype->subtype == INTERACTABLE_TYPE_CHECKPOINT;
}

static bool SpawnPlayerAtCheckpoint() {
    Actor* pCheckpoint = Game::GetFirstActor(ActorIsCheckpoint);
    if (pCheckpoint == nullptr) {
        return false;
    }

    Actor* pPlayer = Game::SpawnActor(Game::GetConfig().playerPrototypeHandle, pCheckpoint->position);
    if (pPlayer) {
        Game::PlayerRespawnAtCheckpoint(pPlayer);
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
    const DungeonCell* pCell = GetDungeonCell(currentDungeonId, currentRoomOffset + screenOffset);
    if (pCell) {
        screenIndex = pCell->screenIndex;
    }

    r32 x = (screenIndex % ROOM_MAX_DIM_SCREENS) * VIEWPORT_WIDTH_METATILES;
    r32 y = (screenIndex / ROOM_MAX_DIM_SCREENS) * VIEWPORT_HEIGHT_METATILES;

    Actor* pPlayer = Game::SpawnActor(Game::GetConfig().playerPrototypeHandle, glm::vec2(x, y));
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

                g_state = GAME_STATE_DUNGEON_MAP;
            }
        }
		
        Game::UpdateActors();
        ViewportFollowPlayer();
        Game::UI::Update();
	}

    Game::Rendering::ClearSpriteLayers();
    Game::DrawActors();

    // Draw HUD
    Game::UI::DrawPlayerHealthBar(g_gameData.playerMaxHealth);
    Game::UI::DrawPlayerStaminaBar(g_gameData.playerMaxStamina);
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
    Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);

    return true;
}

static void DrawMap(const glm::ivec2 scrollOffset) {
    const glm::ivec2 viewportPos = Game::Rendering::GetViewportPos();
    const glm::ivec4 innerBounds = Game::GetDialogInnerBounds();
    const glm::ivec4 worldBounds = innerBounds + glm::ivec4(viewportPos, viewportPos);
    const glm::ivec4 worldTileBounds = worldBounds * s32(METATILE_DIM_TILES);

    const glm::ivec2 tileMin(worldTileBounds.x, worldTileBounds.y);
    const glm::ivec2 tileMax(worldTileBounds.z, worldTileBounds.w);

    const Dungeon* pDungeon = AssetManager::GetAsset(currentDungeonId);
    if (!pDungeon) {
        return;
    }

    // Draw map background
    constexpr u16 borderIndexOffset = 0x110;
    const glm::ivec2 borderMin(tileMin - 1);
    const glm::ivec2 borderMax(tileMax + 1);
    for (s32 y = borderMin.y; y < borderMax.y; ++y) {
        for (s32 x = borderMin.x; x < borderMax.x; ++x) {
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
            Game::Rendering::DrawBackgroundTile(Game::GetConfig().mapBankHandle, tile, { x, y });
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

            const s32 yTile = y + tileMin.y - scrollOffset.y;
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

                const s32 xTile = (x * 2) + tileMin.x - scrollOffset.x;

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
                        Game::Rendering::DrawBackgroundTile(Game::GetConfig().mapBankHandle, tile, { xTile, yTile });
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
                        Game::Rendering::DrawBackgroundTile(Game::GetConfig().mapBankHandle, tile, { xTile + 1, yTile });
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
                        Game::Rendering::DrawBackgroundTile(Game::GetConfig().mapBankHandle, tile, { xTile, yTile });
                    }
                    tile.tileId++;
                    if (xTile >= tileMin.x && xTile < tileMax.x) {
                        Game::Rendering::DrawBackgroundTile(Game::GetConfig().mapBankHandle, tile, { xTile + 1, yTile });
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
                        Game::Rendering::DrawBackgroundTile(Game::GetConfig().mapBankHandle, tile, { xTile, yTile });
                    }
                    tile.tileId++;
                    if (xTile + 1 >= tileMin.x && xTile + 1 < tileMax.x) {
                        Game::Rendering::DrawBackgroundTile(Game::GetConfig().mapBankHandle, tile, { xTile + 1, yTile });
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
            const RoomTemplate* pTemplate = AssetManager::GetAsset(roomInstance.templateId);

            u32 roomWidthScreens = pTemplate->width;
            u32 roomHeightScreens = pTemplate->height;

            const u32 roomX = cell.screenIndex % ROOM_MAX_DIM_SCREENS;
            const u32 roomY = cell.screenIndex / ROOM_MAX_DIM_SCREENS;

            // Room width = 2;
            const BgTile* pMapTiles = pTemplate->GetMapTiles();
            for (u32 i = 0; i < 2; i++) {
                const s32 xTile = (x * 2) + tileMin.x + i - scrollOffset.x;

                // Clipping: Check if tile is within worldTileBounds
                if (xTile >= tileMin.x && xTile < tileMax.x) {
                    const u32 roomTileIndex = (roomX * 2 + i) + roomY * ROOM_MAX_DIM_SCREENS * 2;
                    const BgTile& tile = pMapTiles[roomTileIndex];
                    Game::Rendering::DrawBackgroundTile(pTemplate->mapChrBankHandle, tile, {xTile, yTile});
                }
            }
        }
    }

    const Actor* pPlayer = Game::GetPlayer();
    if (pPlayer) {
        const glm::ivec2 playerGridPos = Game::GetDungeonGridCell(pPlayer->position);
        DrawMapIcon(playerGridPos, 0xac, 0x01, scrollOffset, worldBounds);
    }

    // Draw dropped exp with placeholder graphics
    if (g_gameData.expRemnant.dungeonId == currentDungeonId) {
        DrawMapIcon(g_gameData.expRemnant.gridOffset, 0x08, 0x00, scrollOffset, worldBounds);
    }

    if (!Game::IsDialogOpen()) {
        return;
    }

    // Draw scroll arrows
    Sprite sprite{};
    sprite.palette = 0x01;
    
    if (scrollOffset.x > mapScrollMin.x) {
        sprite.tileId = 0xa8;
        glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels({ worldBounds.x, worldBounds.y });
        drawPos.x += TILE_DIM_PIXELS * 0.5f;
        drawPos.y += mapCenter.y * s32(METATILE_DIM_PIXELS);

        sprite.x = drawPos.x;
        sprite.y = drawPos.y;
        sprite.flipVertical = true;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);

        sprite.y -= TILE_DIM_PIXELS;
        sprite.flipVertical = false;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
    }
    if (scrollOffset.x < mapScrollMax.x) {
        sprite.tileId = 0xa8;
        glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels({ worldBounds.x, worldBounds.y });
        drawPos.x += (mapSize.x * s32(METATILE_DIM_PIXELS)) - TILE_DIM_PIXELS * 1.5f;
        drawPos.y += mapCenter.y * s32(METATILE_DIM_PIXELS);

        sprite.x = drawPos.x;
        sprite.y = drawPos.y;
        sprite.flipHorizontal = true;
        sprite.flipVertical = true;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);

        sprite.y -= TILE_DIM_PIXELS;
        sprite.flipVertical = false;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
    }
    if (scrollOffset.y > mapScrollMin.y) {
        sprite.tileId = 0xa9;
        glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels({ worldBounds.x, worldBounds.y });
        drawPos.x += mapCenter.x * s32(METATILE_DIM_PIXELS);
        drawPos.y += TILE_DIM_PIXELS * 0.5f;

        sprite.x = drawPos.x;
        sprite.y = drawPos.y;
        sprite.flipHorizontal = true;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);

        sprite.x -= TILE_DIM_PIXELS;
        sprite.flipHorizontal = false;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
    }
    if (scrollOffset.y < mapScrollMax.y) {
        sprite.tileId = 0xa9;
        glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels({ worldBounds.x, worldBounds.y });
        drawPos.x += mapCenter.x * s32(METATILE_DIM_PIXELS);
        drawPos.y += (mapSize.y * s32(METATILE_DIM_PIXELS)) - TILE_DIM_PIXELS * 1.5f;

        sprite.x = drawPos.x;
        sprite.y = drawPos.y;
        sprite.flipVertical = true;
        sprite.flipHorizontal = true;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);

        sprite.x -= TILE_DIM_PIXELS;
        sprite.flipHorizontal = false;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
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
        g_state = GAME_STATE_DUNGEON;
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
    Game::UI::DrawPlayerHealthBar(g_gameData.playerMaxHealth);
    Game::UI::DrawPlayerStaminaBar(g_gameData.playerMaxStamina);
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
    DungeonHandle nextDungeon;
    glm::i8vec2 nextGridCell;
    u8 nextDirection;
    u8* cachedPaletteColors;

    r32 progress = 0.0f;
    u8 status = TRANSITION_FADE_OUT;
    u8 holdTimer = 12;
};

static void UpdateFadeToBlack(r32 progress, const u8* cachedColors) {
    progress = glm::smoothstep(0.0f, 1.0f, progress);

    u8 colors[PALETTE_COLOR_COUNT];
    for (u32 i = 0; i < PALETTE_COUNT; i++) {
        memcpy(colors, cachedColors + i * PALETTE_COLOR_COUNT, PALETTE_COLOR_COUNT);
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
            UpdateFadeToBlack(state->progress, state->cachedPaletteColors);
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
        Game::Rendering::FlushBackgroundTiles();
        Game::LoadRoom(state->nextDungeon, state->nextGridCell, state->nextDirection);
        state->status = TRANSITION_FADE_IN;
        freezeGameplay = false;
        break;
    }
    case TRANSITION_FADE_IN: {
        if (state->progress > 0.0f) {
            state->progress -= 0.10f;
            UpdateFadeToBlack(state->progress, state->cachedPaletteColors);
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
    g_gameData.playerMaxHealth = 96;
    SetPlayerHealth(g_gameData.playerMaxHealth);
    g_gameData.playerMaxStamina = 64;
    SetPlayerStamina(g_gameData.playerMaxStamina);
    SetPlayerExp(0);
    SetPlayerWeapon(PLAYER_WEAPON_LAUNCHER);

	// TODO: Initialize first checkpoint

    g_gameData.expRemnant.dungeonId = DungeonHandle::Null();

    g_gameData.persistedActorData.Clear();
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
    return g_gameData.playerCurrentHealth;
}
s16 Game::GetPlayerMaxHealth() {
	return g_gameData.playerMaxHealth;
}
void Game::AddPlayerHealth(s16 health) {
    g_gameData.playerCurrentHealth += health;
    g_gameData.playerCurrentHealth = glm::clamp(g_gameData.playerCurrentHealth, s16(0), g_gameData.playerMaxHealth);
	Game::UI::SetPlayerDisplayHealth(g_gameData.playerCurrentHealth);
}
void Game::SetPlayerHealth(s16 health) {
    g_gameData.playerCurrentHealth = health;
    g_gameData.playerCurrentHealth = glm::clamp(g_gameData.playerCurrentHealth, s16(0), g_gameData.playerMaxHealth);
	Game::UI::SetPlayerDisplayHealth(g_gameData.playerCurrentHealth);
}
s16 Game::GetPlayerStamina() {
    return g_gameData.playerCurrentStamina;
}

s16 Game::GetPlayerMaxStamina() {
    return g_gameData.playerMaxStamina;
}

void Game::AddPlayerStamina(s16 stamina) {
    g_gameData.playerCurrentStamina += stamina;
    g_gameData.playerCurrentStamina = glm::clamp(g_gameData.playerCurrentStamina, s16(0), g_gameData.playerMaxStamina);
    Game::UI::SetPlayerDisplayStamina(g_gameData.playerCurrentStamina);
}

void Game::SetPlayerStamina(s16 stamina) {
    g_gameData.playerCurrentStamina = stamina;
    g_gameData.playerCurrentStamina = glm::clamp(g_gameData.playerCurrentStamina, s16(0), g_gameData.playerMaxStamina);
    Game::UI::SetPlayerDisplayStamina(g_gameData.playerCurrentStamina);
}
s16 Game::GetPlayerExp() {
    return g_gameData.playerExperience;
}
void Game::AddPlayerExp(s16 exp) {
    g_gameData.playerExperience += exp;
    g_gameData.playerExperience = glm::clamp(g_gameData.playerExperience, s16(0), s16(SHRT_MAX));
	Game::UI::SetPlayerDisplayExp(g_gameData.playerExperience);
}
void Game::SetPlayerExp(s16 exp) {
    g_gameData.playerExperience = exp;
	Game::UI::SetPlayerDisplayExp(g_gameData.playerExperience);
}
u16 Game::GetPlayerWeapon() {
    return g_gameData.playerWeapon;
}
void Game::SetPlayerWeapon(u16 weapon) {
    g_gameData.playerWeapon = weapon;
}
#pragma endregion

#pragma region ExpRemnant
void Game::ClearExpRemnant() {
    g_gameData.expRemnant.dungeonId = DungeonHandle::Null();
}
void Game::SetExpRemnant(const glm::vec2& position, u16 value) {
    g_gameData.expRemnant.dungeonId = currentDungeonId;
    g_gameData.expRemnant.gridOffset = RoomPosToDungeonGridOffset(currentRoomOffset, position);
    g_gameData.expRemnant.position = position;
    g_gameData.expRemnant.value = value;
}
#pragma endregion

#pragma region Checkpoint
Checkpoint Game::GetCheckpoint() {
    return g_gameData.checkpoint;
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
    g_gameData.checkpoint = {
        .dungeonId = currentDungeonId,
        .gridOffset = RoomPosToDungeonGridOffset(currentRoomOffset, pCheckpoint->position)
    };

    // Revive dead actors
    g_gameData.persistedActorData.ForEach(ReviveDeadActor);
}
#pragma endregion

#pragma region Persisted actor data
PersistedActorData* Game::GetPersistedActorData(u64 id) {
    if (id == UUID_NULL || (id & (u64(0xFFFFFFFF) << 32)) == UUID_NULL) {
        return nullptr;
    }

    PersistedActorData* pPersistData = g_gameData.persistedActorData.Get(id);
    return pPersistData;
}
void Game::SetPersistedActorData(u64 id, const PersistedActorData& data) {
    if (id == UUID_NULL || (id & (u64(0xFFFFFFFF) << 32)) == UUID_NULL) {
        return;
    }

    PersistedActorData* pPersistData = g_gameData.persistedActorData.Get(id);
    if (pPersistData) {
        *pPersistData = data;
    }
    else {
        g_gameData.persistedActorData.Add(id, data);
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

    if (area.flags.flipDirection) {
        result *= -1;
    }

    return result;
}

const Overworld* Game::GetOverworld() {
    auto overworldHandle = Game::GetConfig().overworldHandle;
    auto pOverworld = AssetManager::GetAsset(overworldHandle);
	if (!pOverworld) {
		DEBUG_ERROR("Failed to load overworld asset with ID: %llu\n", overworldHandle.id);
		return nullptr;
	}
	return pOverworld;
}

bool Game::LoadOverworld(u8 keyAreaIndex, u8 direction) {
    const Overworld* pOverworld = GetOverworld();

    g_state = GAME_STATE_OVERWORLD;

    Rendering::SetViewportPos(glm::vec2(0.0f), false);
    ClearActors();

    const OverworldKeyArea& area = pOverworld->keyAreas[keyAreaIndex];
    const glm::ivec2 overworldDir = GetOverworldDir(area, direction);
    glm::ivec2 spawnPos = area.position;
    if (area.flags.passthrough) {
        spawnPos += overworldDir;
    }

    Actor* pPlayer = Game::SpawnActor(Game::GetConfig().playerOverworldPrototypeHandle, spawnPos);
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
    const Overworld* pOverworld = GetOverworld();
    const OverworldKeyArea& area = pOverworld->keyAreas[keyAreaIndex];

    overworldAreaEnterDir = direction;
    Game::TriggerLevelTransition(area.dungeonId, area.targetGridCell, GetSidescrollingDir(direction, area.flags.flipDirection));
}

bool Game::LoadRoom(DungeonHandle dungeonId, const glm::i8vec2 gridCell, u8 direction) {
    glm::i8vec2 roomOffset;
    const RoomInstance* pNextRoom = GetDungeonRoom(dungeonId, gridCell, &roomOffset);
    if (!pNextRoom) {
        const DungeonCell* pCell = GetDungeonCell(dungeonId, gridCell);
        if (!pCell || pCell->roomIndex == -1) {
            return LoadOverworld(currentOverworldArea, direction);
        }

        return LoadOverworld(pCell->screenIndex, direction);
    }

    currentDungeonId = dungeonId;
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
    g_state = GAME_STATE_DUNGEON;
    UnloadRoom();

    const RoomInstance* pCurrentRoom = GetCurrentRoom();
    const RoomTemplate* pTemplate = GetCurrentRoomTemplate();
    const RoomActor* pRoomActors = pTemplate->GetActors();
    for (u32 i = 0; i < pTemplate->actorCount; i++)
    {
        const RoomActor& actor = pRoomActors[i];

        const u64 combinedId = actor.id | (u64(pCurrentRoom->id) << 32);
        const PersistedActorData* pPersistData = g_gameData.persistedActorData.Get(combinedId);
        if (!pPersistData || !(pPersistData->dead || pPersistData->permaDead)) {
            SpawnActor(&actor, pCurrentRoom->id);
        }
    }

    // Spawn player
    SpawnPlayerAtEntrance(screenOffset, direction);
    ViewportFollowPlayer();

    // Spawn xp remnant, if it belongs in this room
    if (g_gameData.expRemnant.dungeonId == currentDungeonId && currentDungeonId != DungeonHandle::Null()) {
        const RoomInstance* pRoom = GetDungeonRoom(currentDungeonId, g_gameData.expRemnant.gridOffset);
        if (pRoom->id == pCurrentRoom->id) {
            Actor* pRemnant = SpawnActor(Game::GetConfig().xpRemnantPrototypeHandle, g_gameData.expRemnant.position);
            pRemnant->state.pickupState.value = g_gameData.expRemnant.value;
        }
    }

    gameplayFramesElapsed = 0;
    Rendering::RefreshViewport();

    return true;
}

DungeonHandle Game::GetCurrentDungeon() {
    return currentDungeonId;
}

glm::i8vec2 Game::GetCurrentRoomOffset() {
    return currentRoomOffset;
}

const RoomInstance* Game::GetCurrentRoom() {
    return GetDungeonRoom(currentDungeonId, currentRoomOffset);
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
    switch (g_state) {
    case GAME_STATE_DUNGEON:
    case GAME_STATE_DUNGEON_MAP: {
        const RoomTemplate* pTemplate = GetCurrentRoomTemplate();
        if (!pTemplate) {
            break;
        }
        return { pTemplate->width * VIEWPORT_WIDTH_METATILES, pTemplate->height * VIEWPORT_HEIGHT_METATILES };
    }
    case GAME_STATE_OVERWORLD: {
        const Overworld* pOverworld = GetOverworld();
		if (!pOverworld) {
			break;
		}
        return { pOverworld->tilemap.width, pOverworld->tilemap.height };
    }
    default:
        break;
    }

    return glm::i8vec2(0);
}

const Tilemap* Game::GetCurrentTilemap() {
    switch (g_state) {
    case GAME_STATE_DUNGEON:
    case GAME_STATE_DUNGEON_MAP: {
        const RoomTemplate* pTemplate = GetCurrentRoomTemplate();
        if (!pTemplate) {
            break;
        }
        return &pTemplate->tilemap;
    }
    case GAME_STATE_OVERWORLD: {
        const Overworld* pOverworld = GetOverworld();
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
    g_state = initialState;
}

void Game::StepFrame() {
    Input::Update();
    StepCoroutines();

	switch (g_state) {
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

void Game::TriggerLevelTransition(DungeonHandle targetDungeon, glm::i8vec2 targetGridCell, u8 enterDirection, void(*callback)()) {
	// This is a bit silly, but can't think of a better way to do this right now
    static u8 cachedPaletteColors[PALETTE_MEMORY_SIZE];
	memcpy(cachedPaletteColors, ::Rendering::GetPalettePtr(0), PALETTE_MEMORY_SIZE);
    
    LevelTransitionState state = {
            .nextDungeon = targetDungeon,
            .nextGridCell = targetGridCell,
            .nextDirection = enterDirection,
            .cachedPaletteColors = cachedPaletteColors,
    };
    StartCoroutine(LevelTransitionCoroutine, state, callback);
    freezeGameplay = true;
}

void Game::DestroyTileAt(const glm::ivec2& tileCoord, const glm::vec2& impactPoint) {
    const Tilemap* pTilemap = GetCurrentTilemap();
    if (!pTilemap) {
        return;
    }
    
    // Check if tile is destructible
    const s32 tilesetTileIndex = Tiles::GetTilesetTileIndex(pTilemap, tileCoord);
    if (tilesetTileIndex < 0) {
        return;
    }
    
    const Tileset* pTileset = AssetManager::GetAsset(pTilemap->tilesetHandle);
    if (!pTileset) {
        return;
    }
    
    const TilesetTile& tile = pTileset->tiles[tilesetTileIndex];
    if (tile.type != TILE_DESTRUCTIBLE) {
        return; // Only destroy destructible tiles
    }
    
    // Remove the tile by setting it to empty
    Tiles::SetTilesetTile(pTilemap, tileCoord, 0); // TILE_EMPTY = 0
    
    // Calculate tile center in world coordinates
    const glm::vec2 tileCenter = glm::vec2(tileCoord) + glm::vec2(0.5f, 0.5f);
    
    // Spawn 4 debris particles flying outward like Super Mario Bros
    constexpr r32 debrisSpeed = 0.15f;
    constexpr glm::vec2 directions[4] = {
        { -debrisSpeed, -debrisSpeed }, // Top-left
        {  debrisSpeed, -debrisSpeed }, // Top-right  
        { -debrisSpeed,  debrisSpeed }, // Bottom-left
        {  debrisSpeed,  debrisSpeed }  // Bottom-right
    };
    
    // TODO: Replace with actual tile debris effect prototype handle
    // This would need to be configured in game assets/config
    // For example: ActorPrototypeHandle debrisPrototype = GetConfig().tileDebrisEffect;
    
    for (u32 i = 0; i < 4; i++) {
        // Example of how this would work with proper asset handles:
        // SpawnActor(debrisPrototype, tileCenter, directions[i]);
        
        // For now, we could spawn a basic explosion effect as a placeholder
        // if there's a generic effect available in the game config
    }
    
    // TODO: Optional - play destruction sound effect
    // Audio::PlaySFX(destructionSoundHandle);
}
}