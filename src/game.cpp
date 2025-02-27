#include "game.h"
#include "system.h"
#include "input.h"
#include <cstring>
#include <cstdio>
#include "rendering_util.h"
#include "level.h"
#include "viewport.h"
#include "collision.h"
#include "metasprite.h"
#include "tiles.h"
#include <imgui.h>
#include <vector>
#include "audio.h"
#include "nes_timing.h"
#include <gtc/constants.hpp>
#include "random.h"

// TODO: Move somewhere else?
constexpr u32 MAX_COROUTINE_COUNT = 256;
constexpr u32 MAX_COROUTINE_STATE_SIZE = 32;

typedef bool (*CoroutineFunc)(void*);

struct Coroutine {
    CoroutineFunc func;
    alignas(void*) u8 state[MAX_COROUTINE_STATE_SIZE];

    void (*callback)() = nullptr;
};

// TODO: Move somewhere else?
enum SpriteLayerType : u8 {
    SPRITE_LAYER_UI,
    SPRITE_LAYER_FX,
    SPRITE_LAYER_FG,
    SPRITE_LAYER_BG,

    SPRITE_LAYER_COUNT
};

struct SpriteLayer {
    Sprite* pNextSprite = nullptr;
    u32 spriteCount = 0;
};

constexpr u32 LAYER_SPRITE_COUNT = MAX_SPRITE_COUNT / SPRITE_LAYER_COUNT;

static void ClearSpriteLayers(SpriteLayer* layers, bool fullClear = false) {
    const Sprite* pSprites = Rendering::GetSpritesPtr(0);

    for (u32 i = 0; i < SPRITE_LAYER_COUNT; i++) {
        SpriteLayer& layer = layers[i];

        u32 beginIndex = i << 10;
        Sprite* pBeginSprite = Rendering::GetSpritesPtr(beginIndex);

        const u32 spritesToClear = fullClear ? LAYER_SPRITE_COUNT : layer.spriteCount;
        Rendering::Util::ClearSprites(pBeginSprite, spritesToClear);
        layer.pNextSprite = pBeginSprite;
        layer.spriteCount = 0;
    }
}

static Sprite* GetNextFreeSprite(SpriteLayer* pLayer, u32 count = 1) {
    if (pLayer->spriteCount + count > LAYER_SPRITE_COUNT) {
        return nullptr;
    }

    Sprite* result = pLayer->pNextSprite;
    pLayer->spriteCount += count;
    pLayer->pNextSprite += count;

    return result;
}

namespace Game {
    r64 secondsElapsed = 0.0f;
    u32 clockCounter = 0;

    // 16ms Frames elapsed while not paused
    u32 gameplayFramesElapsed = 0;

    // Coroutines
    Pool<Coroutine, MAX_COROUTINE_COUNT> coroutines;
    Pool<PoolHandle<Coroutine>, MAX_COROUTINE_COUNT> coroutineRemoveList;

    PoolHandle<Coroutine> transitionCoroutine = PoolHandle<Coroutine>::Null();

    // Input
    u8 currentInput = BUTTON_NONE;
    u8 previousInput = BUTTON_NONE;

    // Rendering data
    RenderSettings* pRenderSettings;
    ChrSheet* pChr;
    Nametable* pNametables;
    Scanline* pScanlines;
    Palette* pPalettes;

    // Sprites
    SpriteLayer spriteLayers[SPRITE_LAYER_COUNT];

    // Viewport
    Viewport viewport;

    Level* pCurrentLevel = nullptr;

    Pool<Actor, MAX_DYNAMIC_ACTOR_COUNT> actors;
    Pool<PoolHandle<Actor>, MAX_DYNAMIC_ACTOR_COUNT> actorRemoveList;

    // Global player stuff
    PoolHandle<Actor> playerHandle;
    u16 playerMaxHealth = 96;
    u16 playerHealth = 96;
    u16 playerDispRedHealth = 96;
    u16 playerDispYellowHealth = 96;
    u16 playerExp = 0;
    u16 playerDispExp = 0;
    u8 playerWeapon = PLAYER_WEAPON_LAUNCHER;

    // Dialogue stuff
    struct DialogueState {
        bool active = false;
        u32 currentLine = 0;
        const char* const* pDialogueLines = nullptr;
        u32 lineCount;
        PoolHandle<Coroutine> currentLineCoroutine = PoolHandle<Coroutine>::Null();
    };
    DialogueState dialogue;

    struct Checkpoint {
        u16 levelIndex;
        u8 screenIndex;
    };
    Checkpoint lastCheckpoint;

    ChrSheet playerBank;

    bool paused = false;

    Sound jumpSfx;
    Sound gunSfx;
    Sound ricochetSfx;
    Sound damageSfx;
    Sound expSfx;
    Sound enemyDieSfx;

    //Sound bgm;
    //bool musicPlaying = false;

    u8 basePaletteColors[PALETTE_MEMORY_SIZE];

    // TODO: Try to eliminate as much of this as possible
    constexpr s32 playerPrototypeIndex = 0;
    constexpr s32 playerGrenadePrototypeIndex = 1;
    constexpr s32 playerArrowPrototypeIndex = 4;
    constexpr s32 dmgNumberPrototypeIndex = 5;
    constexpr s32 enemyFireballPrototypeIndex = 8;
    constexpr s32 haloSmallPrototypeIndex = 0x0a;
    constexpr s32 haloLargePrototypeIndex = 0x0b;

    constexpr u8 playerWingFrameBankOffsets[4] = { 0x00, 0x08, 0x10, 0x18 };
    constexpr u8 playerHeadFrameBankOffsets[12] = { 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C };
    constexpr u8 playerLegsFrameBankOffsets[4] = { 0x50, 0x54, 0x58, 0x5C };
    constexpr u8 playerBowFrameBankOffsets[3] = { 0x60, 0x68, 0x70 };
    constexpr u8 playerLauncherFrameBankOffsets[3] = { 0x80, 0x88, 0x90 };

    constexpr u8 playerWingFrameChrOffset = 0x00;
    constexpr u8 playerHeadFrameChrOffset = 0x08;
    constexpr u8 playerLegsFrameChrOffset = 0x0C;
    constexpr u8 playerWeaponFrameChrOffset = 0x18;

    constexpr u8 playerWingFrameTileCount = 8;
    constexpr u8 playerHeadFrameTileCount = 4;
    constexpr u8 playerLegsFrameTileCount = 4;
    constexpr u8 playerHandFrameTileCount = 2;
    constexpr u8 playerWeaponFrameTileCount = 8;

    constexpr glm::ivec2 playerBowOffsets[3] = { { 10, -4 }, { 9, -14 }, { 10, 4 } };
    constexpr u32 playerBowFwdMetaspriteIndex = 8;
    constexpr u32 playerBowDiagMetaspriteIndex = 9;
    constexpr u32 playerBowArrowFwdMetaspriteIndex = 3;
    constexpr u32 playerBowArrowDiagMetaspriteIndex = 4;

    constexpr glm::ivec2 playerLauncherOffsets[3] = { { 5, -5 }, { 7, -12 }, { 8, 1 } };
    constexpr u32 playerLauncherFwdMetaspriteIndex = 10;
    constexpr u32 playerLauncherDiagMetaspriteIndex = 11;
    constexpr u32 playerLauncherGrenadeMetaspriteIndex = 12;

    constexpr glm::vec2 viewportScrollThreshold = { 4.0f, 3.0f };

#pragma region Coroutines
    static bool StepCoroutine(Coroutine* pCoroutine) {
        return pCoroutine->func(pCoroutine->state);
    }

    template <typename S>
    static PoolHandle<Coroutine> StartCoroutine(CoroutineFunc func, const S& state, void (*callback)() = nullptr) {
        static_assert(sizeof(S) <= MAX_COROUTINE_STATE_SIZE);

        auto handle = coroutines.Add();
        Coroutine* pCoroutine = coroutines.Get(handle);

        if (pCoroutine == nullptr) {
            return PoolHandle<Coroutine>::Null();
        }

        pCoroutine->func = func;
        memcpy(pCoroutine->state, &state, sizeof(S));
        pCoroutine->callback = callback;
        return handle;
    }
#pragma endregion

#pragma region Input
    static bool ButtonDown(u8 flags) {
        return Input::ButtonDown(flags, currentInput);
    }

    static bool ButtonUp(u8 flags) {
        return Input::ButtonUp(flags, currentInput);
    }

    static bool ButtonPressed(u8 flags) {
        return Input::ButtonPressed(flags, currentInput, previousInput);
    }

    static bool ButtonReleased(u8 flags) {
        return Input::ButtonReleased(flags, currentInput, previousInput);
    }
#

#pragma region Viewport
    static void UpdateScreenScroll() {
        // Drugs mode
        /*for (int i = 0; i < 288; i++) {
            float sine = glm::sin(gameplayFramesElapsed / 60.f + (i / 16.0f));
            const Scanline state = {
                (s32)((viewport.x + sine / 4) * METATILE_DIM_PIXELS),
                (s32)(viewport.y * METATILE_DIM_PIXELS)
            };
            pScanlines[i] = state;
        }*/

        const Scanline state = {
            (s32)(viewport.x * METATILE_DIM_PIXELS),
            (s32)(viewport.y * METATILE_DIM_PIXELS)
        };
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = state;
        }
    }

    static void UpdateViewport() {
        Actor* pPlayer = actors.Get(playerHandle);
        if (pPlayer == nullptr) {
            return;
        }

        glm::vec2 viewportCenter = glm::vec2{ viewport.x + VIEWPORT_WIDTH_METATILES / 2.0f, viewport.y + VIEWPORT_HEIGHT_METATILES / 2.0f };
        glm::vec2 targetOffset = pPlayer->position - viewportCenter;

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

        MoveViewport(&viewport, pNametables, pCurrentLevel->pTilemap, delta.x, delta.y);
    }

    static bool PositionInViewportBounds(glm::vec2 pos) {
        return pos.x >= viewport.x &&
            pos.x < viewport.x + VIEWPORT_WIDTH_METATILES &&
            pos.y >= viewport.y &&
            pos.y < viewport.y + VIEWPORT_HEIGHT_METATILES;
    }

    static glm::ivec2 WorldPosToScreenPixels(glm::vec2 pos) {
        return glm::ivec2{
            (s32)glm::roundEven((pos.x - viewport.x) * METATILE_DIM_PIXELS),
            (s32)glm::roundEven((pos.y - viewport.y) * METATILE_DIM_PIXELS)
        };
    }
#pragma endregion

#pragma region Actor initialization
    static void InitializeActor(Actor* pActor) {
        const ActorPrototype* pPrototype = pActor->pPrototype;

        pActor->flags.facingDir = ACTOR_FACING_RIGHT;
        pActor->flags.inAir = true;
        pActor->flags.active = true;
        pActor->flags.pendingRemoval = false;

        pActor->initialPosition = pActor->position;
        pActor->velocity = glm::vec2{};

        pActor->frameIndex = 0;
        pActor->animCounter = pPrototype->animations[0].frameLength;

        switch (pPrototype->type) {
        case ACTOR_TYPE_PLAYER: {
            pActor->playerState.damageCounter = 0;
            break;
        }
        case ACTOR_TYPE_NPC: {
            pActor->npcState.health = pPrototype->npcData.health;
            pActor->npcState.damageCounter = 0;
            break;
        }
        case ACTOR_TYPE_BULLET: {
            pActor->bulletState.lifetime = pPrototype->bulletData.lifetime;
            pActor->bulletState.lifetimeCounter = pPrototype->bulletData.lifetime;
            break;
        }
        case ACTOR_TYPE_PICKUP: {
            break;
        }
        case ACTOR_TYPE_EFFECT: {
            pActor->effectState.lifetime = pPrototype->effectData.lifetime;
            pActor->effectState.lifetimeCounter = pPrototype->effectData.lifetime;
            break;
        }
        default: 
            break;
        }
    }

    static Actor* SpawnActor(const Actor* pTemplate) {
        auto handle = actors.Add(*pTemplate);

        if (handle == PoolHandle<Actor>::Null()) {
            return nullptr;
        }

        if (pTemplate->pPrototype->type == ACTOR_TYPE_PLAYER) {
            playerHandle = handle;
        }

        Actor* pActor = actors.Get(handle);
        InitializeActor(pActor);

        return pActor;
    }

    static Actor* SpawnActor(u32 presetIndex, const glm::vec2& position) {
        auto handle = actors.Add();

        if (handle == PoolHandle<Actor>::Null()) {
            return nullptr;
        }

        Actor* pActor = actors.Get(handle);

        const ActorPrototype* pPrototype = Actors::GetPrototype(presetIndex);
        pActor->pPrototype = pPrototype;
        if (pPrototype->type == ACTOR_TYPE_PLAYER) {
            playerHandle = handle;
        }

        pActor->position = position;
        InitializeActor(pActor);

        return pActor;
    }
#pragma endregion

#pragma region Actor utils
    // Returns false if counter stops, true if keeps running
    static bool UpdateCounter(u16& counter) {
        if (counter == 0) {
            return false;
        }

        counter--;
        return true;
    }
#pragma endregion

    /*s32 Tiles::GetNametableIndex(const glm::ivec2& pos) {
        return (pos.x / NAMETABLE_WIDTH_METATILES + pos.y / NAMETABLE_HEIGHT_METATILES) % NAMETABLE_COUNT;
    }

    glm::ivec2 Tiles::GetNametableOffset(const glm::ivec2& pos) {
        return { (s32)(pos.x % NAMETABLE_WIDTH_METATILES), (s32)(pos.y % NAMETABLE_HEIGHT_METATILES) };
    }*/

#pragma region Rendering
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

    static void CopyLevelTileToNametable(const glm::ivec2& worldPos) {
        const u32 nametableIndex = Tiles::GetNametableIndex(worldPos);
        const glm::ivec2 nametableOffset = Tiles::GetNametableOffset(worldPos);

        const Tilemap* pTilemap = pCurrentLevel->pTilemap;
        const s32 tilesetIndex = Tiles::GetTilesetIndex(pTilemap, worldPos);
        const TilesetTile* tile = Tiles::GetTilesetTile(pTilemap, tilesetIndex);

        const Metatile& metatile = tile->metatile;
        const s32 palette = Tiles::GetTilesetPalette(pTilemap->pTileset, tilesetIndex);
        Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset.x, nametableOffset.y, metatile, palette);
    }

    static void CopyBoxTileToNametable(const glm::ivec2& worldPos, const glm::ivec2& tileOffset, const glm::ivec2& sizeTiles, u8 palette) {
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

    static void DrawBgBoxAnimated(const glm::ivec2& viewportPos, const glm::ivec2& size, const glm::ivec2 maxSize, u8 palette) {
        const glm::ivec2 worldPos = viewportPos + glm::ivec2(viewport.x, viewport.y);
        const glm::ivec2 sizeTiles(size.x << 1, size.y << 1);

        for (u32 y = 0; y < maxSize.y; y++) {
            for (u32 x = 0; x < maxSize.x; x++) {

                const glm::ivec2 offset(x, y);

                if (x < size.x && y < size.y) {
                    const glm::ivec2 tileOffset(x << 1, y << 1);
                    CopyBoxTileToNametable(worldPos + offset, tileOffset, sizeTiles, palette);
                }
                else {
                    CopyLevelTileToNametable(worldPos + offset);
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

    static void DrawBgText(const glm::ivec2& boxViewportPos, const glm::ivec2& boxSize, const char* pText, u32 length) {
        const glm::ivec2 worldPos = boxViewportPos + glm::ivec2(viewport.x, viewport.y);
        const glm::ivec2 worldTilePos(worldPos.x * METATILE_DIM_TILES, worldPos.y * METATILE_DIM_TILES);
        const glm::ivec2 innerSizeTiles((boxSize.x << 1) - 2, (boxSize.y << 1) - 2);

        const u32 xTileStart = worldTilePos.x + 1;
        const u32 yTileStart = worldTilePos.y + 1;

        u32 xTile = xTileStart;
        u32 yTile = yTileStart;

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

    static void ClearBgText(const glm::ivec2& boxViewportPos, const glm::ivec2& boxSize) {
        const glm::ivec2 worldPos = boxViewportPos + glm::ivec2(viewport.x, viewport.y);
        const glm::ivec2 worldTilePos(worldPos.x * METATILE_DIM_TILES, worldPos.y * METATILE_DIM_TILES);
        const glm::ivec2 innerSizeTiles((boxSize.x << 1) - 2, (boxSize.y << 1) - 2);

        const u32 xTileStart = worldTilePos.x + 1;
        const u32 yTileStart = worldTilePos.y + 1;

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

    static bool DrawSprite(SpriteLayer* pLayer, const Sprite& sprite) {
        Sprite* outSprite = GetNextFreeSprite(pLayer);
        if (outSprite == nullptr) {
            return false;
        }
        *outSprite = sprite;

        return true;
    }

    static void DrawPlayerHealthBar() {
        const u16 xStart = 16;
        const u16 y = 16;

        SpriteLayer& layer = spriteLayers[SPRITE_LAYER_UI];

        const u32 totalSegments = playerMaxHealth >> 2;
        const u32 fullRedSegments = playerDispRedHealth >> 2;
        const u32 fullYellowSegments = playerDispYellowHealth >> 2;

        u16 x = xStart;
        for (u32 i = 0; i < totalSegments; i++) {
            const u16 healthDrawn = (i * 4);
            const u16 remainingRedHealth = healthDrawn > playerDispRedHealth ? 0 : playerDispRedHealth - healthDrawn;
            const u16 remainingYellowHealth = healthDrawn > playerDispYellowHealth ? 0 : playerDispYellowHealth - healthDrawn;

            u8 tileId;
            if (i < fullRedSegments) {
                tileId = 0xe4;
            }
            else if (i < fullYellowSegments) {
                tileId = 0xe5 + (remainingRedHealth & 3);
            }
            else {
                if (remainingYellowHealth != remainingRedHealth) {
                    tileId = 0xe8 + (remainingYellowHealth & 3);
                }
                else tileId = 0xe0 + (remainingRedHealth & 3);
            }

            Sprite sprite{};
            sprite.tileId = tileId;
            sprite.palette = 0x1;
            sprite.x = x;
            sprite.y = y;
            DrawSprite(&layer, sprite);

            x += 8;
        }
    }

    static void DrawExpCounter() {
        static char buffer[10];
        snprintf(buffer, sizeof(buffer), "%05u", playerDispExp);
        u32 length = strlen(buffer);

        SpriteLayer& layer = spriteLayers[SPRITE_LAYER_UI];

        const u16 xStart = VIEWPORT_WIDTH_PIXELS - 16 - (length*8);
        const u16 y = 16;

        // Draw halo indicator
        Sprite sprite{};
        sprite.tileId = 0x68;
        sprite.palette = 0x0;
        sprite.x = xStart - 8;
        sprite.y = y;
        DrawSprite(&layer, sprite);

        // Draw counter
        u16 x = xStart;
        for (u32 i = 0; i < length; i++) {
            Sprite sprite{};
            sprite.tileId = 0xd6 + buffer[i] - '0';
            sprite.palette = 0x1;
            sprite.x = x;
            sprite.y = y;
            DrawSprite(&layer, sprite);

            x += 8;
        }
    }

    static bool DrawActor(const Actor* pActor, u8 layerIndex = SPRITE_LAYER_FG, const glm::ivec2& pixelOffset = {0,0}, bool hFlip = false, bool vFlip = false, s32 paletteOverride = -1) {
        // Culling
        if (!PositionInViewportBounds(pActor->position)) {
            return false;
        }

        SpriteLayer& layer = spriteLayers[layerIndex];

        glm::ivec2 drawPos = WorldPosToScreenPixels(pActor->position) + pixelOffset;
        const Animation& currentAnim = pActor->pPrototype->animations[0];

        switch (currentAnim.type) {
        case ANIMATION_TYPE_SPRITES: {
            Sprite* outSprite = GetNextFreeSprite(&layer);
            if (outSprite == nullptr) {
                return false;
            }

            const s32 metaspriteIndex = (s32)currentAnim.metaspriteIndex;
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(metaspriteIndex);
            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos + pActor->frameIndex, outSprite, 1, drawPos, hFlip, vFlip, paletteOverride);
            break;
        }
        case ANIMATION_TYPE_METASPRITES: {
            const s32 metaspriteIndex = (s32)currentAnim.metaspriteIndex + pActor->frameIndex;
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(metaspriteIndex);

            Sprite* outSprites = GetNextFreeSprite(&layer, pMetasprite->spriteCount);
            if (outSprites == nullptr) {
                return false;
            }

            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos, outSprites, pMetasprite->spriteCount, drawPos, hFlip, vFlip, paletteOverride);
            break;
        }
        default:
            break;
        }

        return true;
    }

    static s32 GetDamagePaletteOverride(u8 damageCounter) {
        return (damageCounter > 0) ? (gameplayFramesElapsed / 3) % 4 : -1;
    }

    static void GetAnimFrameFromDirection(Actor* pActor) {
        const glm::vec2 dir = glm::normalize(pActor->velocity);
        const r32 angle = glm::atan(dir.y, dir.x);

        const Animation& currentAnim = pActor->pPrototype->animations[0];
        pActor->frameIndex = (s32)glm::roundEven(((angle + glm::pi<r32>()) / (glm::pi<r32>() * 2)) * currentAnim.frameCount) % currentAnim.frameCount;
    }

    // General function that can be used to advance "fake" animations like pattern bank streaming
    static void AdvanceAnimation(u16& animCounter, u16& frameIndex, u16 frameCount, u8 frameLength, s16 loopPoint) {
        const bool loop = loopPoint != -1;
        if (animCounter == 0) {
            // End of anim reached
            if (frameIndex == frameCount - 1) {
                if (loop) {
                    frameIndex = loopPoint;
                }
                else return;
            }
            else frameIndex++;
            
            animCounter = frameLength;
            return;
        }
        animCounter--;
    }

    static void AdvanceCurrentAnimation(Actor* pActor) {
        const Animation& currentAnim = pActor->pPrototype->animations[0];
        AdvanceAnimation(pActor->animCounter, pActor->frameIndex, currentAnim.frameCount, currentAnim.frameLength, currentAnim.loopPoint);
    }
#pragma endregion

#pragma region Collision
    static bool ActorsColliding(const Actor* pActor, const Actor* pOther) {
        const AABB& hitbox = pActor->pPrototype->hitbox;
        const AABB& hitboxOther = pOther->pPrototype->hitbox;
        if (Collision::BoxesOverlap(hitbox, pActor->position, hitboxOther, pOther->position)) {
            return true;
        }

        return false;
    }

    // TODO: Actor collisions could use HitResult as well...
    static void ForEachActorCollision(Actor* pActor, u16 type, u8 alignment, void (*callback)(Actor*, Actor*)) {
        for (u32 i = 0; i < actors.Count(); i++)
        {
            if (!pActor->flags.active || pActor->flags.pendingRemoval) {
                break;
            }

            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pOther = actors.Get(handle);

            if (pOther == nullptr || pOther->pPrototype->type != type || pOther->pPrototype->alignment != alignment || pOther->flags.pendingRemoval || !pOther->flags.active) {
                continue;
            }

            const AABB& hitbox = pActor->pPrototype->hitbox;
            const AABB& hitboxOther = pOther->pPrototype->hitbox;
            if (ActorsColliding(pActor, pOther)) {
                callback(pActor, pOther);
            }
        }
    }

    static bool ActorCollidesWithPlayer(Actor* pActor, Actor* pPlayer) {
        if (pPlayer != nullptr) {
            if (ActorsColliding(pActor, pPlayer)) {
                return true;
            }
        }
        return false;
    }
#pragma endregion

#pragma region Movement
    static void ActorFacePlayer(Actor* pActor) {
        pActor->flags.facingDir = ACTOR_FACING_RIGHT;

        Actor* pPlayer = actors.Get(playerHandle);
        if (pPlayer == nullptr) {
            return;
        }

        if (pPlayer->position.x < pActor->position.x) {
            pActor->flags.facingDir = ACTOR_FACING_LEFT;
        }
    }

    static bool ActorMoveHorizontal(Actor* pActor, HitResult& outHit) {
        const AABB& hitbox = pActor->pPrototype->hitbox;

        const r32 dx = pActor->velocity.x;

        Collision::SweepBoxHorizontal(pCurrentLevel->pTilemap, hitbox, pActor->position, dx, outHit);
        pActor->position.x = outHit.location.x;
        return outHit.blockingHit;
    }

    static bool ActorMoveVertical(Actor* pActor, HitResult& outHit) {
        const AABB& hitbox = pActor->pPrototype->hitbox;

        const r32 dy = pActor->velocity.y;

        Collision::SweepBoxVertical(pCurrentLevel->pTilemap, hitbox, pActor->position, dy, outHit);
        pActor->position.y = outHit.location.y;
        return outHit.blockingHit;
    }

    static void ApplyGravity(Actor* pActor, r32 gravity = 0.01f) {
        pActor->velocity.y += gravity;
    }
#pragma endregion

#pragma region Damage
    struct ExpAnimState {
        const u16& targetExp;
        u16& currentExp;

        r32 progress = 0.0f;
    };

    static bool AnimateExpCoroutine(void* userData) {
        ExpAnimState& state = *(ExpAnimState*)userData;

        state.progress += 0.05f;
        const r32 t = glm::smoothstep(0.0f, 1.0f, state.progress);
        state.currentExp = glm::mix(state.currentExp, state.targetExp, t);

        return state.currentExp != state.targetExp;
    }

    // TODO: Where to get this info properly?
    constexpr u16 largeExpValue = 500;
    constexpr u16 smallExpValue = 10;

    struct SpawnExpState {
        glm::vec2 position;
        u16 remainingValue;
    };

    static bool SpawnExpCoroutine(void* s) {
        SpawnExpState& state = *(SpawnExpState*)s;

        if (state.remainingValue > 0) {
            u16 spawnedValue = state.remainingValue >= largeExpValue ? largeExpValue : smallExpValue;
            s32 prototypeIndex = spawnedValue >= largeExpValue ? haloLargePrototypeIndex : haloSmallPrototypeIndex;

            Actor* pSpawned = SpawnActor(prototypeIndex, state.position);
            const r32 speed = Random::GenerateReal(0.1f, 0.3f);
            pSpawned->velocity = Random::GenerateDirection() * speed;
            pSpawned->pickupState.lingerCounter = 30;
            pSpawned->flags.facingDir = (s8)Random::GenerateInt(-1, 1);
            pSpawned->pickupState.value = pSpawned->pPrototype->pickupData.value;

            if (state.remainingValue < spawnedValue) {
                state.remainingValue = 0;
            }
            else state.remainingValue -= spawnedValue;

            return true;
        }
        return false;
    }

    static void NPCDie(Actor* pActor) {
        pActor->flags.pendingRemoval = true;

        Audio::PlaySFX(&enemyDieSfx, CHAN_ID_NOISE);
        SpawnActor(pActor->pPrototype->npcData.spawnOnDeath, pActor->position);

        // Spawn exp halos
        const u16 totalExpValue = pActor->pPrototype->npcData.expValue;
        if (totalExpValue > 0) {
            SpawnExpState coroutineState = {
                .position = pActor->position,
                .remainingValue = totalExpValue
            };
            StartCoroutine(SpawnExpCoroutine, coroutineState);
        }
    }

    static bool ActorTakeDamage(Actor* pActor, u32 dmgValue, u16& health, u16& damageCounter) {
        constexpr s32 damageDelay = 30;

        if (dmgValue > health) {
            health = 0;
        }
        else health -= dmgValue;
        damageCounter = damageDelay;

        // Spawn damage numbers
        const AABB& hitbox = pActor->pPrototype->hitbox;
        // Random point inside hitbox
        const glm::vec2 randomPointInsideHitbox = {
            Random::GenerateReal(hitbox.x1, hitbox.x2),
            Random::GenerateReal(hitbox.y1, hitbox.y2)
        };
        const glm::vec2 spawnPos = pActor->position + randomPointInsideHitbox;

        Actor* pDmg = SpawnActor(dmgNumberPrototypeIndex, spawnPos);
        if (pDmg != nullptr) {
            pDmg->effectState.value = -dmgValue;
            pDmg->velocity = { 0, -0.03125f };
        }

        if (health <= 0) {
            return false;
        }

        return true;
    }
#pragma endregion

#pragma region Player logic
    static void CorrectPlayerSpawnY(const Level* pLevel, Actor* pPlayer) {
        HitResult hit{};

        const r32 dy = VIEWPORT_HEIGHT_METATILES / 2.0f;  // Sweep downwards to find a floor

        Collision::SweepBoxVertical(pLevel->pTilemap, pPlayer->pPrototype->hitbox, pPlayer->position, dy, hit);
        while (hit.startPenetrating) {
            pPlayer->position.y -= 1.0f;
            Collision::SweepBoxVertical(pLevel->pTilemap, pPlayer->pPrototype->hitbox, pPlayer->position, dy, hit);
        }
        pPlayer->position = hit.location;
    }

    static void SpawnPlayerAtEntrance(const Level* pLevel, u8 screenIndex, u8 direction) {
        r32 x = (screenIndex % TILEMAP_MAX_DIM_SCREENS) * VIEWPORT_WIDTH_METATILES;
        r32 y = (screenIndex / TILEMAP_MAX_DIM_SCREENS) * VIEWPORT_HEIGHT_METATILES;

        Actor* pPlayer = SpawnActor(playerPrototypeIndex, glm::vec2(x, y));
        if (pPlayer == nullptr) {
            return;
        }

        constexpr r32 initialHSpeed = 0.0625f;

        switch (direction) {
        case SCREEN_EXIT_DIR_LEFT: {
            pPlayer->position.x += 0.5f;
            pPlayer->position.y += VIEWPORT_HEIGHT_METATILES / 2.0f;
            pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
            pPlayer->velocity.x = initialHSpeed;
            CorrectPlayerSpawnY(pLevel, pPlayer);
            break;
        }
        case SCREEN_EXIT_DIR_RIGHT: {
            pPlayer->position.x += VIEWPORT_WIDTH_METATILES - 0.5f;
            pPlayer->position.y += VIEWPORT_HEIGHT_METATILES / 2.0f;
            pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
            pPlayer->velocity.x = -initialHSpeed;
            CorrectPlayerSpawnY(pLevel, pPlayer);
            break;
        }
        case SCREEN_EXIT_DIR_TOP: {
            pPlayer->position.x += VIEWPORT_WIDTH_METATILES / 2.0f;
            pPlayer->position.y += 0.5f;
            pPlayer->velocity.y = 0.25f;
            break;
        }
        case SCREEN_EXIT_DIR_BOTTOM: {
            pPlayer->position.x += VIEWPORT_WIDTH_METATILES / 2.0f;
            pPlayer->position.y += VIEWPORT_HEIGHT_METATILES - 0.5f;
            pPlayer->velocity.y = -0.25f;
            break;
        }
        default:
            break;
        }

        pPlayer->playerState.entryDelayCounter = 15;
    }

    static void UpdateFadeToBlack(r32 progress) {
        progress = glm::smoothstep(0.0f, 1.0f, progress);

        for (u32 i = 0; i < PALETTE_MEMORY_SIZE; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            const s32 baseBrightness = (baseColor & 0b1110000) >> 4;
            const s32 hue = baseColor & 0b0001111;

            const s32 minBrightness = hue == 0 ? 0 : -1;

            s32 newBrightness = glm::mix(minBrightness, baseBrightness, 1.0f - progress);

            if (newBrightness >= 0) {
                ((u8*)pPalettes)[i] = hue | (newBrightness << 4);
            }
            else {
                ((u8*)pPalettes)[i] = 0x00;
            }
        }
    }

    enum ScreenTransitionState : u8 {
        TRANSITION_FADE_OUT,
        TRANSITION_LOADING,
        TRANSITION_FADE_IN,
        TRANSITION_COMPLETE
    };

    struct ScreenFadeState {
        u16 nextLevelIndex;
        u16 nextScreenIndex;
        u8 nextDirection;
        r32 progress;

        u8 transitionState = TRANSITION_FADE_OUT;
        u8 holdTimer = 12;
    };

    static bool ScreenFadeCoroutine(void* userData) {
        ScreenFadeState* state = (ScreenFadeState*)userData;

        switch (state->transitionState) {
        case TRANSITION_FADE_OUT: {
            if (state->progress < 1.0f) {
                state->progress += 0.1f;
                UpdateFadeToBlack(state->progress);
                return true;
            }
            state->transitionState = TRANSITION_LOADING;
            break;
        }
        case TRANSITION_LOADING: {
            if (state->holdTimer > 0) {
                state->holdTimer--;
                return true;
            }
            LoadLevel(state->nextLevelIndex, state->nextScreenIndex, state->nextDirection);
            state->transitionState = TRANSITION_FADE_IN;
            break;
        }
        case TRANSITION_FADE_IN: {
            if (state->progress > 0.0f) {
                state->progress -= 0.10f;
                UpdateFadeToBlack(state->progress);
                return true;
            }
            state->transitionState = TRANSITION_COMPLETE;
            break;
        }
        default:
            return false;
        }
    }

    static void HandleLevelExit() {
        const Actor* pPlayer = actors.Get(playerHandle);
        if (pPlayer == nullptr) {
            return;
        }

        bool shouldExit = false;
        u8 exitDirection = 0;
        u8 enterDirection = 0;
        u32 xScreen = 0;
        u32 yScreen = 0;

        // Left side of screen is ugly, so trigger transition earlier
        if (pPlayer->position.x < 0.5f) {
            shouldExit = true;
            exitDirection = SCREEN_EXIT_DIR_LEFT;
            enterDirection = SCREEN_EXIT_DIR_RIGHT;
            xScreen = 0;
            yScreen = glm::clamp(s32(pPlayer->position.y / VIEWPORT_HEIGHT_METATILES), 0, pCurrentLevel->pTilemap->height);
        }
        else if (pPlayer->position.x >= pCurrentLevel->pTilemap->width * VIEWPORT_WIDTH_METATILES) {
            shouldExit = true;
            exitDirection = SCREEN_EXIT_DIR_RIGHT;
            enterDirection = SCREEN_EXIT_DIR_LEFT;
            xScreen = pCurrentLevel->pTilemap->width - 1;
            yScreen = glm::clamp(s32(pPlayer->position.y / VIEWPORT_HEIGHT_METATILES), 0, pCurrentLevel->pTilemap->height);
        }
        else if (pPlayer->position.y < 0) {
            shouldExit = true;
            exitDirection = SCREEN_EXIT_DIR_TOP;
            enterDirection = SCREEN_EXIT_DIR_BOTTOM;
            xScreen = glm::clamp(s32(pPlayer->position.x / VIEWPORT_WIDTH_METATILES), 0, pCurrentLevel->pTilemap->width);
            yScreen = 0;
        }
        else if (pPlayer->position.y >= pCurrentLevel->pTilemap->height * VIEWPORT_HEIGHT_METATILES) {
            shouldExit = true;
            exitDirection = SCREEN_EXIT_DIR_BOTTOM;
            enterDirection = SCREEN_EXIT_DIR_TOP;
            xScreen = glm::clamp(s32(pPlayer->position.x / VIEWPORT_WIDTH_METATILES), 0, pCurrentLevel->pTilemap->width);
            yScreen = pCurrentLevel->pTilemap->height - 1;
        }

        if (shouldExit) {
            const u32 screenIndex = xScreen + TILEMAP_MAX_DIM_SCREENS * yScreen;
            const TilemapScreen& screen = pCurrentLevel->pTilemap->screens[screenIndex];
            const LevelExit* exits = (LevelExit*)&screen.screenMetadata;

            const LevelExit& exit = exits[exitDirection];

            ScreenFadeState state = {
                .nextLevelIndex = exit.targetLevel,
                .nextScreenIndex = exit.targetScreen,
                .nextDirection = enterDirection,
                .progress = 0.0f
            };
            transitionCoroutine = StartCoroutine(ScreenFadeCoroutine, state);
        }
    }

    struct HealthAnimState {
        const u16& targetHealth;
        u16& currentHealth;

        u16 delay = 12;
        r32 progress = 0.0f;
    };

    static bool AnimateHealthCoroutine(void* userData) {
        HealthAnimState& state = *(HealthAnimState*)userData;

        if (state.delay > 0) {
            state.delay--;
            return true;
        }

        state.progress += 0.01f;
        const r32 t = glm::smoothstep(0.0f, 1.0f, state.progress);
        state.currentHealth = glm::mix(state.currentHealth, state.targetHealth, t);

        return state.currentHealth != state.targetHealth;
    }

    static void PlayerDie(Actor* pPlayer) {
        // TODO: drop xp in level
        playerExp = 0;

        // Restore life
        playerHealth = playerMaxHealth;
        playerDispRedHealth = playerMaxHealth;

        // Transition to checkpoint
        ScreenFadeState state = {
                .nextLevelIndex = lastCheckpoint.levelIndex,
                .nextScreenIndex = lastCheckpoint.screenIndex,
                .nextDirection = 0,
                .progress = 0.0f
        };
        transitionCoroutine = StartCoroutine(ScreenFadeCoroutine, state);
    }

    static void HandlePlayerEnemyCollision(Actor* pPlayer, Actor* pEnemy) {
        // If invulnerable
        if (pPlayer->playerState.damageCounter != 0) {
            return;
        }

        const u32 damage = Random::GenerateInt(1, 20);
        Audio::PlaySFX(&damageSfx, CHAN_ID_PULSE0);

        playerDispYellowHealth = playerHealth;
        if (!ActorTakeDamage(pPlayer, damage, playerHealth, pPlayer->playerState.damageCounter)) {
            PlayerDie(pPlayer);
        }

        playerDispRedHealth = playerHealth;
        HealthAnimState state = {
            .targetHealth = playerHealth,
            .currentHealth = playerDispYellowHealth
        };
        StartCoroutine(AnimateHealthCoroutine, state);

        // Recoil
        constexpr r32 recoilSpeed = 0.046875f; // Recoil speed from Zelda 2
        if (pEnemy->position.x > pPlayer->position.x) {
            pPlayer->flags.facingDir = 1;
            pPlayer->velocity.x = -recoilSpeed;
        }
        else {
            pPlayer->flags.facingDir = -1;
            pPlayer->velocity.x = recoilSpeed;
        }

    }

    static void PlayerInput(Actor* pPlayer) {
        constexpr r32 maxSpeed = 0.09375f; // Actual movement speed from Zelda 2
        constexpr r32 acceleration = maxSpeed / 24.f; // Acceleration from Zelda 2

        PlayerState& playerState = pPlayer->playerState;
        if (ButtonDown(BUTTON_DPAD_LEFT)) {
            pPlayer->velocity.x -= acceleration;
            if (pPlayer->flags.facingDir != ACTOR_FACING_LEFT) {
                pPlayer->velocity.x -= acceleration;
            }

            pPlayer->velocity.x = glm::clamp(pPlayer->velocity.x, -maxSpeed, maxSpeed);
            pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
        }
        else if (ButtonDown(BUTTON_DPAD_RIGHT)) {
            pPlayer->velocity.x += acceleration;
            if (pPlayer->flags.facingDir != ACTOR_FACING_RIGHT) {
                pPlayer->velocity.x += acceleration;
            }

            pPlayer->velocity.x = glm::clamp(pPlayer->velocity.x, -maxSpeed, maxSpeed);
            pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
        }
        else {
            // Deceleration if no direcion pressed
            if (!pPlayer->flags.inAir && pPlayer->velocity.x != 0.0f) {
                pPlayer->velocity.x -= acceleration * glm::sign(pPlayer->velocity.x);
            }
        }

        // Aim mode
        if (ButtonDown(BUTTON_DPAD_UP)) {
            playerState.flags.aimMode = PLAYER_AIM_UP;
        }
        else if (ButtonDown(BUTTON_DPAD_DOWN)) {
            playerState.flags.aimMode = PLAYER_AIM_DOWN;
        }
        else playerState.flags.aimMode = PLAYER_AIM_FWD;

        if (ButtonPressed(BUTTON_A) && (!pPlayer->flags.inAir || !playerState.flags.doubleJumped)) {
            pPlayer->velocity.y = -0.25f;
            if (pPlayer->flags.inAir) {
                playerState.flags.doubleJumped = true;
            }

            // Trigger new flap by taking wings out of falling position by advancing the frame index
            playerState.wingFrame = ++playerState.wingFrame % PLAYER_WING_FRAME_COUNT;

            Audio::PlaySFX(&jumpSfx, CHAN_ID_PULSE0);
        }

        if (pPlayer->velocity.y < 0 && ButtonReleased(BUTTON_A)) {
            pPlayer->velocity.y /= 2;
        }

        if (ButtonDown(BUTTON_A) && pPlayer->velocity.y > 0) {
            playerState.flags.slowFall = true;
        }

        if (ButtonReleased(BUTTON_B)) {
            playerState.shootCounter = 0.0f;
        }

        if (ButtonPressed(BUTTON_SELECT)) {
            if (playerWeapon == PLAYER_WEAPON_LAUNCHER) {
                playerWeapon = PLAYER_WEAPON_BOW;
            }
            else playerWeapon = PLAYER_WEAPON_LAUNCHER;
        }
    }

    static void PlayerShoot(Actor* pPlayer) {
        constexpr s32 shootDelay = 10;

        PlayerState& playerState = pPlayer->playerState;
        UpdateCounter(playerState.shootCounter);

        if (ButtonDown(BUTTON_B) && playerState.shootCounter == 0) {
            playerState.shootCounter = shootDelay;

            const s32 prototypeIndex = playerWeapon == PLAYER_WEAPON_LAUNCHER ? playerGrenadePrototypeIndex : playerArrowPrototypeIndex;
            Actor* pBullet = SpawnActor(prototypeIndex, pPlayer->position);
            if (pBullet == nullptr) {
                return;
            }

            const glm::vec2 fwdOffset = glm::vec2{ 0.375f * pPlayer->flags.facingDir, -0.25f };
            const glm::vec2 upOffset = glm::vec2{ 0.1875f * pPlayer->flags.facingDir, -0.5f };
            const glm::vec2 downOffset = glm::vec2{ 0.25f * pPlayer->flags.facingDir, -0.125f };

            constexpr r32 bulletVel = 0.625f;
            constexpr r32 bulletVelSqrt2 = 0.45f; // vel / sqrt(2)

            if (playerState.flags.aimMode == PLAYER_AIM_FWD) {
                pBullet->position = pBullet->position + fwdOffset;
                pBullet->velocity.x = bulletVel * pPlayer->flags.facingDir;
            }
            else {
                pBullet->velocity.x = bulletVelSqrt2 * pPlayer->flags.facingDir;
                pBullet->velocity.y = (playerState.flags.aimMode == PLAYER_AIM_UP) ? -bulletVelSqrt2 : bulletVelSqrt2;
                pBullet->position = pBullet->position + ((playerState.flags.aimMode == PLAYER_AIM_UP) ? upOffset : downOffset);
            }

            if (playerWeapon == PLAYER_WEAPON_LAUNCHER) {
                pBullet->velocity = pBullet->velocity * 0.75f;
                Audio::PlaySFX(&gunSfx, CHAN_ID_NOISE);
            }

        }
    }

    static bool DrawPlayerGun(Actor* pPlayer, r32 vOffset) {
        glm::ivec2 drawPos = WorldPosToScreenPixels(pPlayer->position);
        drawPos.y += vOffset;

        // Draw weapon first
        glm::ivec2 weaponOffset;
        u8 weaponFrameBankOffset;
        u32 weaponMetaspriteIndex;
        switch (playerWeapon) {
        case PLAYER_WEAPON_BOW: {
            weaponOffset = playerBowOffsets[pPlayer->playerState.flags.aimMode];
            weaponFrameBankOffset = playerBowFrameBankOffsets[pPlayer->playerState.flags.aimMode];
            weaponMetaspriteIndex = pPlayer->playerState.flags.aimMode == PLAYER_AIM_FWD ? playerBowFwdMetaspriteIndex : playerBowDiagMetaspriteIndex;
            break;
        }
        case PLAYER_WEAPON_LAUNCHER: {
            weaponOffset = playerLauncherOffsets[pPlayer->playerState.flags.aimMode];
            weaponFrameBankOffset = playerLauncherFrameBankOffsets[pPlayer->playerState.flags.aimMode];
            weaponMetaspriteIndex = pPlayer->playerState.flags.aimMode == PLAYER_AIM_FWD ? playerLauncherFwdMetaspriteIndex : playerLauncherDiagMetaspriteIndex;
            break;
        }
        default:
            break;
        }
        weaponOffset.x *= pPlayer->flags.facingDir;

        Rendering::Util::CopyChrTiles(playerBank.tiles + weaponFrameBankOffset, pChr[1].tiles + playerWeaponFrameChrOffset, playerWeaponFrameTileCount);

        const Metasprite* bowMetasprite = Metasprites::GetMetasprite(weaponMetaspriteIndex);

        SpriteLayer& layer = spriteLayers[SPRITE_LAYER_FG];
        Sprite* outSprites = GetNextFreeSprite(&layer, bowMetasprite->spriteCount);
        if (outSprites == nullptr) {
            return false;
        }

        Rendering::Util::CopyMetasprite(bowMetasprite->spritesRelativePos, outSprites, bowMetasprite->spriteCount, drawPos + weaponOffset, pPlayer->flags.facingDir == ACTOR_FACING_LEFT, false);

        return true;
    }

    static void DrawPlayer(Actor* pPlayer) {
        PlayerState& playerState = pPlayer->playerState;

        // Animate chr sheet using player bank
        const bool jumping = pPlayer->velocity.y < 0;
        const bool descending = !jumping && pPlayer->velocity.y > 0;
        const bool falling = descending && !playerState.flags.slowFall;
        const bool moving = glm::abs(pPlayer->velocity.x) > 0;
        const bool takingDamage = playerState.damageCounter > 0;

        s32 headFrameIndex = PLAYER_HEAD_IDLE;
        if (takingDamage) {
            headFrameIndex = PLAYER_HEAD_DMG;
        }
        else if (falling) {
            headFrameIndex = PLAYER_HEAD_FALL;
        }
        else if (moving) {
            headFrameIndex = PLAYER_HEAD_FWD;
        }
        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerHeadFrameBankOffsets[playerState.flags.aimMode * 4 + headFrameIndex],
            pChr[1].tiles + playerHeadFrameChrOffset,
            playerHeadFrameTileCount
        );

        s32 legsFrameIndex = PLAYER_LEGS_IDLE;
        if (descending || takingDamage) {
            legsFrameIndex = PLAYER_LEGS_FALL;
        }
        else if (jumping) {
            legsFrameIndex = PLAYER_LEGS_JUMP;
        }
        else if (moving) {
            legsFrameIndex = PLAYER_LEGS_FWD;
        }
        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerLegsFrameBankOffsets[legsFrameIndex],
            pChr[1].tiles + playerLegsFrameChrOffset,
            playerLegsFrameTileCount
        );

        // When jumping or falling, wings get into proper position and stay there for the duration of the jump/fall
        const bool wingsInPosition = (jumping && playerState.wingFrame == PLAYER_WINGS_ASCEND) || (falling && playerState.wingFrame == PLAYER_WINGS_DESCEND);

        // Wings flap faster to get into proper position
        const u16 wingAnimFrameLength = (jumping || falling) ? 6 : 12;

        if (!wingsInPosition) {
            AdvanceAnimation(playerState.wingCounter, playerState.wingFrame, PLAYER_WING_FRAME_COUNT, wingAnimFrameLength, 0);
        }

        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerWingFrameBankOffsets[playerState.wingFrame],
            pChr[1].tiles + playerWingFrameChrOffset,
            playerWingFrameTileCount
        );

        // Setup draw data
        s32 vOffset = 0;
        if (pPlayer->velocity.y == 0) {
            vOffset = playerState.wingFrame > PLAYER_WINGS_FLAP_START ? -1 : 0;
        }

        DrawPlayerGun(pPlayer, vOffset);
        const s32 paletteOverride = GetDamagePaletteOverride(playerState.damageCounter);
        pPlayer->frameIndex = playerState.flags.aimMode;
        DrawActor(pPlayer, SPRITE_LAYER_FG, { 0, vOffset }, pPlayer->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }

    static void UpdatePlayerSidescroller(Actor* pActor) {
        UpdateCounter(pActor->playerState.entryDelayCounter);
        UpdateCounter(pActor->playerState.damageCounter);
        
        // Reset slow fall
        pActor->playerState.flags.slowFall = false;

        const bool enteringLevel = pActor->playerState.entryDelayCounter > 0;
        const bool stunned = pActor->playerState.damageCounter > 0;
        if (!enteringLevel && !stunned && !dialogue.active) {
            PlayerInput(pActor);
            PlayerShoot(pActor);
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            pActor->velocity.x = 0.0f;
        }

        constexpr r32 playerGravity = 0.01f;
        constexpr r32 playerSlowGravity = playerGravity / 4;

        const r32 gravity = pActor->playerState.flags.slowFall ? playerSlowGravity : playerGravity;
        ApplyGravity(pActor, gravity);

        // Reset in air flag
        pActor->flags.inAir = true;

        if (ActorMoveVertical(pActor, hit)) {
            pActor->velocity.y = 0.0f;

            if (hit.impactNormal.y < 0.0f) {
                pActor->flags.inAir = false;
                pActor->playerState.flags.doubleJumped = false;
            }
        }

        DrawPlayer(pActor);
    }
#pragma endregion

#pragma region Bullets
    static void BulletDie(Actor* pBullet, const glm::vec2& effectPos) {
        pBullet->flags.pendingRemoval = true;
        SpawnActor(pBullet->pPrototype->bulletData.spawnOnDeath, effectPos);
    }

    static void HandleBulletEnemyCollision(Actor* pBullet, Actor* pEnemy) {
        BulletDie(pBullet, pBullet->position);

        const u32 damage = Random::GenerateInt(1, 2);
        if (!ActorTakeDamage(pEnemy, damage, pEnemy->npcState.health, pEnemy->npcState.damageCounter)) {
            NPCDie(pEnemy);
        }
    }

    static void BulletCollision(Actor* pActor) {
        if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_FRIENDLY) {
            ForEachActorCollision(pActor, ACTOR_TYPE_NPC, ACTOR_ALIGNMENT_HOSTILE, HandleBulletEnemyCollision);
        }
        else if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_HOSTILE) {
            Actor* pPlayer = actors.Get(playerHandle);
            if (ActorCollidesWithPlayer(pActor, pPlayer)) {
                HandlePlayerEnemyCollision(pPlayer, pActor);
                BulletDie(pActor, pActor->position);
            }
            // TODO: Collision with friendly NPC:s? Does this happen in the game?
        }
    }

    static void UpdateDefaultBullet(Actor* pActor) {
        if (!UpdateCounter(pActor->bulletState.lifetimeCounter)) {
            BulletDie(pActor, pActor->position);
            return;
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            BulletDie(pActor, hit.impactPoint);
            return;
        }

        if (ActorMoveVertical(pActor, hit)) {
            BulletDie(pActor, hit.impactPoint);
            return;
        }

        BulletCollision(pActor);

        GetAnimFrameFromDirection(pActor);
        DrawActor(pActor);
    }

    static void BulletRicochet(glm::vec2& velocity, const glm::vec2& normal) {
        velocity = glm::reflect(velocity, normal);
        //Audio::PlaySFX(&ricochetSfx, CHAN_ID_PULSE0);
    }

    static void UpdateGrenade(Actor* pActor) {
        if (!UpdateCounter(pActor->bulletState.lifetimeCounter)) {
            BulletDie(pActor, pActor->position);
            return;
        }

        constexpr r32 grenadeGravity = 0.04f;
        ApplyGravity(pActor, grenadeGravity);

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            BulletRicochet(pActor->velocity, hit.impactNormal);
        }

        if (ActorMoveVertical(pActor, hit)) {
            BulletRicochet(pActor->velocity, hit.impactNormal);
        }

        BulletCollision(pActor);

        GetAnimFrameFromDirection(pActor);
        DrawActor(pActor);
    }

    static void UpdateFireball(Actor* pActor) {
        if (!UpdateCounter(pActor->bulletState.lifetimeCounter)) {
            BulletDie(pActor, pActor->position);
            return;
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            BulletDie(pActor, hit.impactPoint);
            return;
        }

        if (ActorMoveVertical(pActor, hit)) {
            BulletDie(pActor, hit.impactPoint);
            return;
        }

        BulletCollision(pActor);

        AdvanceCurrentAnimation(pActor);

        DrawActor(pActor);
    }
#pragma endregion

#pragma region NPC
    static void UpdateSlimeEnemy(Actor* pActor) {
        UpdateCounter(pActor->npcState.damageCounter);

        if (!pActor->flags.inAir) {
            const bool shouldJump = Random::GenerateInt(0, 127) == 0;
            if (shouldJump) {
                pActor->velocity.y = -0.25f;
                ActorFacePlayer(pActor);
                pActor->velocity.x = 0.15625f * pActor->flags.facingDir;
            }
            else {
                pActor->velocity.x = 0.00625f * pActor->flags.facingDir;
            }
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            pActor->velocity.x = 0.0f;
            pActor->flags.facingDir = (s8)hit.impactNormal.x;
        }

        ApplyGravity(pActor);

        // Reset in air flag
        pActor->flags.inAir = true;

        if (ActorMoveVertical(pActor, hit)) {
            pActor->velocity.y = 0.0f;

            if (hit.impactNormal.y < 0.0f) {
                pActor->flags.inAir = false;
            }
        }

        Actor* pPlayer = actors.Get(playerHandle);
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

        const s32 paletteOverride = GetDamagePaletteOverride(pActor->npcState.damageCounter);
        DrawActor(pActor, SPRITE_LAYER_FG, {0,0}, pActor->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }

    static void UpdateSkullEnemy(Actor* pActor) {
        UpdateCounter(pActor->npcState.damageCounter);

        ActorFacePlayer(pActor);

        static const r32 amplitude = 4.0f;
        const r32 sineTime = glm::sin(gameplayFramesElapsed / 60.f);
        pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

        // Shoot fireballs
        const bool shouldFire = Random::GenerateInt(0, 127) == 0;
        if (shouldFire) {

            Actor* pPlayer = actors.Get(playerHandle);
            if (pPlayer != nullptr) {
                Actor* pBullet = SpawnActor(enemyFireballPrototypeIndex, pActor->position);
                if (pBullet == nullptr) {
                    return;
                }

                const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
                pBullet->velocity = playerDir * 0.0625f;
            }
        }

        Actor* pPlayer = actors.Get(playerHandle);
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

        const s32 paletteOverride = GetDamagePaletteOverride(pActor->npcState.damageCounter);
        DrawActor(pActor, SPRITE_LAYER_FG, { 0,0 }, pActor->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }
#pragma endregion

#pragma region Pickups
    static void UpdateExpHalo(Actor* pActor) {
        Actor* pPlayer = actors.Get(playerHandle);

        const glm::vec2 playerVec = pPlayer->position - pActor->position;
        const glm::vec2 playerDir = glm::normalize(playerVec);
        const r32 playerDist = glm::length(playerVec);

        // Wait for a while before homing towards player
        if (!UpdateCounter(pActor->pickupState.lingerCounter)) {
            constexpr r32 trackingFactor = 0.1f; // Adjust to control homing strength

            glm::vec2 desiredVelocity = (playerVec * trackingFactor) + pPlayer->velocity;
            pActor->velocity = glm::mix(pActor->velocity, desiredVelocity, trackingFactor); // Smooth velocity transition

        }
        else {
            // Slow down after initial explosion
            r32 speed = glm::length(pActor->velocity);
            if (speed != 0) {
                const glm::vec2 dir = glm::normalize(pActor->velocity);

                constexpr r32 deceleration = 0.01f;
                speed = glm::clamp(speed - deceleration, 0.0f, 1.0f); // Is 1.0 a good max value?
                pActor->velocity = dir * speed;
            }
        }

        pActor->position += pActor->velocity;

        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            Audio::PlaySFX(&expSfx, CHAN_ID_PULSE0);
            pActor->flags.pendingRemoval = true;

            playerExp += pActor->pickupState.value;
            ExpAnimState state = {
                .targetExp = playerExp,
                .currentExp = playerDispExp,
            };
            StartCoroutine(AnimateExpCoroutine, state);

            return;
        }

        // Smoothstep animation when inside specified radius from player
        const Animation& currentAnim = pActor->pPrototype->animations[0];
        constexpr r32 animRadius = 4.0f;
        pActor->frameIndex = glm::roundEven((1.0f - glm::smoothstep(0.0f, animRadius, playerDist)) * currentAnim.frameCount);

        DrawActor(pActor, SPRITE_LAYER_FG, {0,0}, pActor->flags.facingDir == ACTOR_FACING_LEFT);
    }

#pragma endregion

#pragma region Effects
    static void UpdateDefaultEffect(Actor* pActor) {
        if (!UpdateCounter(pActor->effectState.lifetimeCounter)) {
            pActor->flags.pendingRemoval = true;
        }
    }

    static void UpdateExplosion(Actor* pActor) {
        UpdateDefaultEffect(pActor);

        AdvanceCurrentAnimation(pActor);
        DrawActor(pActor, SPRITE_LAYER_FX);
    }

    // Calls itoa, but adds a plus sign if value is positive
    static u32 ItoaSigned(s16 value, char* str) {
        s32 i = 0;
        if (value > 0) {
            str[i++] = '+';
        }

        itoa(value, str + i, 10);

        return strlen(str);
    }

    static void UpdateNumbers(Actor* pActor) {
        UpdateDefaultEffect(pActor);

        pActor->position.y += pActor->velocity.y;

        static char numberStr[16]{};
        const u32 strLength = ItoaSigned(pActor->effectState.value, numberStr);

        // Ascii character '*' = 0x2A
        // There are a couple extra characters (star, comma, period) that could be used for special symbols
        constexpr u8 chrOffset = 0x2A;

        SpriteLayer& layer = spriteLayers[SPRITE_LAYER_FX];

        const Animation& currentAnim = pActor->pPrototype->animations[0];
        for (u32 c = 0; c < strLength; c++) {
            // TODO: What about metasprite frames? Handle this more rigorously!
            Sprite* outSprite = GetNextFreeSprite(&layer);
            if (outSprite == nullptr) {
                break;
            }

            const s32 frameIndex = (numberStr[c] - chrOffset) % currentAnim.frameCount;
            const u8 tileId = Metasprites::GetMetasprite(currentAnim.metaspriteIndex)->spritesRelativePos[frameIndex].tileId;

            const glm::ivec2 pixelPos = WorldPosToScreenPixels(pActor->position);
            *outSprite = {
                u16(pixelPos.y),
                u16(pixelPos.x + c * 5),
                tileId,
                1
            };
        }
    }
#pragma endregion
    static void UpdatePlayer(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        case PLAYER_SUBTYPE_SIDESCROLLER: {
            UpdatePlayerSidescroller(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdateNPC(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        // Enemies
        case NPC_SUBTYPE_ENEMY_SLIME: {
            UpdateSlimeEnemy(pActor);
            break;
        }
        case NPC_SUBTYPE_ENEMY_SKULL: {
            UpdateSkullEnemy(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdateBullet(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        case BULLET_SUBTYPE_DEFAULT: {
            UpdateDefaultBullet(pActor);
            break;
        }
        case BULLET_SUBTYPE_GRENADE: {
            UpdateGrenade(pActor);
            break;
        }
        case BULLET_SUBTYPE_FIREBALL: {
            UpdateFireball(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdatePickup(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        case PICKUP_SUBTYPE_HALO: {
            UpdateExpHalo(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdateEffect(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        case EFFECT_SUBTYPE_EXPLOSION: {
            UpdateExplosion(pActor);
            break;
        }
        case EFFECT_SUBTYPE_NUMBERS: {
            UpdateNumbers(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdateActors() {
        
        actorRemoveList.Clear();

        for (u32 i = 0; i < actors.Count(); i++)
        {
            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pActor = actors.Get(handle);

            if (pActor == nullptr) {
                continue;
            }

            if (pActor->flags.pendingRemoval) {
                actorRemoveList.Add(handle);
                continue;
            }

            if (!pActor->flags.active) {
                continue;
            }

            switch (pActor->pPrototype->type) {
            case ACTOR_TYPE_PLAYER: {
                UpdatePlayer(pActor);
                break;
            }
            case ACTOR_TYPE_NPC: {
                UpdateNPC(pActor);
                break;
            }
            case ACTOR_TYPE_BULLET: {
                UpdateBullet(pActor);
                break;
            }
            case ACTOR_TYPE_PICKUP: {
                UpdatePickup(pActor);
                break;
            }
            case ACTOR_TYPE_EFFECT: {
                UpdateEffect(pActor);
                break;
            }
            default:
                break;
            }
        }

        for (u32 i = 0; i < actorRemoveList.Count(); i++) {
            auto handle = *actorRemoveList.Get(actorRemoveList.GetHandle(i));
            actors.Remove(handle);
        }
    }

    static void UpdateCoroutines() {
        coroutineRemoveList.Clear();
        
        for (u32 i = 0; i < coroutines.Count(); i++) {
            PoolHandle<Coroutine> handle = coroutines.GetHandle(i);
            Coroutine* pCoroutine = coroutines.Get(handle);

            if (!StepCoroutine(pCoroutine)) {
                coroutineRemoveList.Add(handle);

                if (pCoroutine->callback) {
                    pCoroutine->callback();
                }
            }
        }

        for (u32 i = 0; i < coroutineRemoveList.Count(); i++) {
            auto handle = *coroutineRemoveList.Get(coroutineRemoveList.GetHandle(i));
            coroutines.Remove(handle);
        }
    }

#pragma region Dialogue
    static void EndDialogue() {
        dialogue.active = false;
    }

    static void AdvanceDialogue() {
        if (!dialogue.active) {
            return;
        }

        // Stop the previous coroutine
        if (coroutines.Get(dialogue.currentLineCoroutine)) {
            coroutines.Remove(dialogue.currentLineCoroutine);
        }

        if (dialogue.currentLine >= dialogue.lineCount) {
            // Close dialogue box, then end dialogue
            BgBoxAnimState state{
                    .viewportPos = glm::ivec2(8,3),
                    .width = 16,
                    .maxHeight = 4,
                    .palette = 3,
                    .direction = -1,

                    .height = 4
            };
            StartCoroutine(AnimBgBoxCoroutine, state, EndDialogue);
            return;
        }
        else {
            ClearBgText(glm::ivec2(8, 3), glm::ivec2(16, 4));
            AnimTextState state{
                .pText = dialogue.pDialogueLines[dialogue.currentLine],
                .boxViewportPos = glm::ivec2(8,3),
                .boxSize = glm::ivec2(16,4),
            };
            dialogue.currentLineCoroutine = StartCoroutine(AnimTextCoroutine, state);
        }

        dialogue.currentLine++;
    }

    static void BeginDialogue(const char* const* pLines, u32 count) {
        if (dialogue.active) {
            return;
        }

        dialogue.active = true;
        dialogue.currentLine = 0;
        dialogue.pDialogueLines = pLines;
        dialogue.lineCount = count;

        BgBoxAnimState state{
                    .viewportPos = glm::ivec2(8,3),
                    .width = 16,
                    .maxHeight = 4,
                    .palette = 3,
                    .direction = 1,
        };
        StartCoroutine(AnimBgBoxCoroutine, state, AdvanceDialogue);
    }
#pragma endregion

    static void Step() {
        previousInput = currentInput;
        currentInput = Input::GetControllerState();

        static char textBuffer[256]{};

        ClearSpriteLayers(spriteLayers);

        // TODO: There should be a better way to check handle validity
        bool transitioning = coroutines.Get(transitionCoroutine);

        if (!paused) {
            gameplayFramesElapsed++;

            UpdateCoroutines();

            if (!transitioning) {
                UpdateActors();
                UpdateViewport();
                HandleLevelExit();
            }

            // Draw HUD
            DrawPlayerHealthBar();
            DrawExpCounter();
        }

        UpdateScreenScroll();

        /*if (ButtonPressed(BUTTON_START)) {
            if (!musicPlaying) {
                Audio::PlayMusic(&bgm, true);
            }
            else {
                Audio::StopMusic();
            }
            musicPlaying = !musicPlaying;
        }*/

        static constexpr const char* lines[] = {
                "What a horrible night to have a curse, am I right fellas???",
                "I can print multiple lines of dialogue here.",
                "This is the second to last\nline, getting closer to the\nend...",
                "This is the last line of\ndialogue, thxbye!"
        };

        if (ButtonPressed(BUTTON_START)) {
            if (!dialogue.active) {
                BeginDialogue(lines, 4);
            }
            else AdvanceDialogue();
        }

        // Animate color palette hue
        /*s32 hueShift = (s32)glm::roundEven(gameplayFramesElapsed / 12.f);
        for (u32 i = 0; i < PALETTE_MEMORY_SIZE; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            s32 hue = baseColor & 0b1111;

            s32 newHue = hue + hueShift;
            newHue &= 0b1111;

            u8 newColor = (baseColor & 0b1110000) | newHue;
            ((u8*)pPalettes)[i] = newColor;
        }*/
    }

#pragma region Public API
    void LoadLevel(u32 index, s32 screenIndex, u8 direction, bool refresh) {
        if (index >= MAX_LEVEL_COUNT) {
            DEBUG_ERROR("Level count exceeded!");
        }

        pCurrentLevel = Levels::GetLevelsPtr() + index;

        ReloadLevel(screenIndex, direction, refresh);
    }

    void UnloadLevel(bool refresh) {
        if (pCurrentLevel == nullptr) {
            return;
        }

        viewport.x = 0;
        viewport.y = 0;
        UpdateScreenScroll();

        // Clear actors
        actors.Clear();

        if (refresh) {
            RefreshViewport(&viewport, pNametables, pCurrentLevel->pTilemap);
        }
    }

    void ReloadLevel(s32 screenIndex, u8 direction, bool refresh) {
        if (pCurrentLevel == nullptr) {
            return;
        }

        UnloadLevel(refresh);

        // Spawn player in sidescrolling level
        if (pCurrentLevel->flags.type == LEVEL_TYPE_SIDESCROLLER) {
            SpawnPlayerAtEntrance(pCurrentLevel, screenIndex, direction);
            UpdateViewport();
            UpdateScreenScroll();
        }

        for (u32 i = 0; i < pCurrentLevel->actors.Count(); i++)
        {
            auto handle = pCurrentLevel->actors.GetHandle(i);
            const Actor* pActor = pCurrentLevel->actors.Get(handle);

            SpawnActor(pActor);
        }

        gameplayFramesElapsed = 0;

        if (refresh) {
            RefreshViewport(&viewport, pNametables, pCurrentLevel->pTilemap);
        }
    }

    void Initialize() {
        // Rendering data
        pRenderSettings = Rendering::GetSettingsPtr();
        pChr = Rendering::GetChrPtr(0);
        pPalettes = Rendering::GetPalettePtr(0);
        pNametables = Rendering::GetNametablePtr(0);
        pScanlines = Rendering::GetScanlinePtr(0);

        ClearSpriteLayers(spriteLayers, true);

        // Init chr memory
        // TODO: Pre-process these instead of loading from bitmap at runtime!
        ChrSheet temp;
        Rendering::Util::CreateChrSheet("assets/chr000.bmp", &temp);
        Rendering::Util::CopyChrTiles(temp.tiles, pChr[0].tiles, CHR_SIZE_TILES);
        Rendering::Util::CreateChrSheet("assets/chr001.bmp", &temp);
        Rendering::Util::CopyChrTiles(temp.tiles, pChr[1].tiles, CHR_SIZE_TILES);

        Rendering::Util::CreateChrSheet("assets/player.bmp", &playerBank);

        //u8 paletteColors[8 * 8];
        Rendering::Util::LoadPaletteColorsFromFile("assets/palette.dat", basePaletteColors);

        for (u32 i = 0; i < PALETTE_MEMORY_SIZE; i++) {
            memcpy(pPalettes, basePaletteColors, PALETTE_MEMORY_SIZE);
        }

        Tiles::LoadTileset("assets/forest.til");
        Metasprites::Load("assets/meta.spr");
        Levels::LoadLevels("assets/levels.lev");
        Actors::LoadPrototypes("assets/actors.prt");

        // Initialize scanline state
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = { 0, 0 };
        }

        viewport.x = 0.0f;
        viewport.y = 0.0f;

        // TEMP SOUND STUFF
        jumpSfx = Audio::LoadSound("assets/jump.nsf");
        gunSfx = Audio::LoadSound("assets/gun1.nsf");
        ricochetSfx = Audio::LoadSound("assets/ricochet.nsf");
        damageSfx = Audio::LoadSound("assets/damage.nsf");
        expSfx = Audio::LoadSound("assets/exp.nsf");
        enemyDieSfx = Audio::LoadSound("assets/enemydie.nsf");
        //bgm = Audio::LoadSound("assets/music.nsf");

        // TODO: Level should load palettes and tileset?
        LoadLevel(0);
    }

    void Free() {
        Audio::StopMusic();

        Audio::FreeSound(&jumpSfx);
        Audio::FreeSound(&gunSfx);
        Audio::FreeSound(&ricochetSfx);
        Audio::FreeSound(&damageSfx);
        Audio::FreeSound(&expSfx);
        Audio::FreeSound(&enemyDieSfx);
        //Audio::FreeSound(&bgm);
    }

    void Update(r64 dt) {
        secondsElapsed += dt;
        while (secondsElapsed >= CLOCK_PERIOD) {
            clockCounter++;
            secondsElapsed -= CLOCK_PERIOD;

            // One frame
            if (clockCounter == FRAME_CLOCK) {
                Step();
                clockCounter = 0;
            }
        }
    }

    bool IsPaused() {
        return paused;
    }
    void SetPaused(bool p) {
        paused = p;
    }

    Viewport* GetViewport() {
        return &viewport;
    }
    Level* GetLevel() {
        return pCurrentLevel;
    }
    DynamicActorPool* GetActors() {
        return &actors;
    }
#pragma endregion
}