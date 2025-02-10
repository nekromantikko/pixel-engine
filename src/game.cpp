#include "game.h"
#include "system.h"
#include "input.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include "rendering_util.h"
#include "level.h"
#include "viewport.h"
#include "collision.h"
#include "metasprite.h"
#include "tiles.h"
#include <imgui.h>
#include "math.h"
#include <vector>

namespace Game {
    r64 secondsElapsed = 0.0f;

    // Seconds elapsed while not paused
    r64 gameplaySecondsElapsed = 0.0f;

    // Rendering data
    RenderSettings* pRenderSettings;
    Sprite* pSprites;
    ChrSheet* pChr;
    Nametable* pNametables;
    Scanline* pScanlines;
    Palette* pPalettes;

    // Viewport
    Viewport viewport;

    Level* pCurrentLevel = nullptr;

    Pool<Actor> actors;
    Actor* pViewportTarget = nullptr;

    ChrSheet playerBank;

    bool paused = false;

    constexpr r32 damageDelay = 0.5f;

#pragma region Actors
    static void InitializeActor(Actor* pActor) {
        const u32 flags = pActor->pPrototype->behaviour;

        if (flags & ACTOR_BEHAVIOUR_PLAYER_SIDESCROLLER) {
            pViewportTarget = pActor;
        }

        pActor->initialPosition = pActor->position;

        pActor->damageCounter = 0;
        pActor->lifetimeCounter = pActor->lifetime;

        pActor->drawData = ActorDrawData{};
    }

    static Actor* SpawnActor(const Actor* pTemplate) {
        PoolHandle<Actor> handle = actors.Add();
        Actor* pActor = actors.Get(handle);

        if (pActor == nullptr) {
            return nullptr;
        }

        *pActor = *pTemplate;
        return pActor;
    }

    static Actor* SpawnActor(u32 presetIndex) {
        PoolHandle<Actor> handle = actors.Add();
        Actor* pActor = actors.Get(handle);

        if (pActor == nullptr) {
            return nullptr;
        }

        pActor->pPrototype = Actors::GetPrototype(presetIndex);
        return pActor;
    }
#pragma endregion

    static void UpdateScreenScroll() {
        const Scanline state = {
            (s32)(viewport.x * METATILE_DIM_PIXELS),
            (s32)(viewport.y * METATILE_DIM_PIXELS)
        };
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = state;
        }
    }

    void LoadLevel(u32 index, s32 screenIndex, bool refresh) {
        if (index >= MAX_LEVEL_COUNT) {
            DEBUG_ERROR("Level count exceeded!");
        }

        pCurrentLevel = Levels::GetLevelsPtr() + index;

        viewport.x = 0;
        viewport.y = 0;
        UpdateScreenScroll();

        if (refresh) {
            RefreshViewport(&viewport, pNametables, &pCurrentLevel->tilemap);
        }
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
        pViewportTarget = nullptr;

        if (refresh) {
            RefreshViewport(&viewport, pNametables, &pCurrentLevel->tilemap);
        }
    }

    void ReloadLevel(bool refresh) {
        if (pCurrentLevel == nullptr) {
            return;
        }

        UnloadLevel(refresh);
        
        for (u32 i = 0; i < pCurrentLevel->actors.Count(); i++)
        {
            PoolHandle<Actor> handle = pCurrentLevel->actors.GetHandle(i);
            const Actor* pActor = pCurrentLevel->actors.Get(handle);

            Actor* pSpawned = SpawnActor(pActor);
            InitializeActor(pSpawned);
        }

        gameplaySecondsElapsed = 0.0f;

        if (refresh) {
            RefreshViewport(&viewport, pNametables, &pCurrentLevel->tilemap);
        }
    }

    u8 basePaletteColors[PALETTE_MEMORY_SIZE];

	void Initialize() {
        // Rendering data
        pRenderSettings = Rendering::GetSettingsPtr();
        pChr = Rendering::GetChrPtr(0);
        pPalettes = Rendering::GetPalettePtr(0);
        pSprites = Rendering::GetSpritesPtr(0);
        pNametables = Rendering::GetNametablePtr(0);
        pScanlines = Rendering::GetScanlinePtr(0);

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

        Rendering::Util::ClearSprites(pSprites, MAX_SPRITE_COUNT);

        Tiles::LoadTileset("assets/forest.til");
        Metasprites::Load("assets/meta.spr");
        Levels::Init();
        Levels::LoadLevels("assets/levels.lev");
        Actors::LoadPrototypes("assets/actors.prt");

        // Initialize scanline state
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = { 0, 0 };
        }

        viewport.x = 0.0f;
        viewport.y = 0.0f;

        actors.Init(512);

        // TODO: Level should load palettes and tileset?
        LoadLevel(0);
	}

    void Free() {
        
    }


    // TODO: Try to eliminate as much of this as possible
    constexpr s32 playerGrenadePrototypeIndex = 1;
    constexpr s32 playerArrowPrototypeIndex = 4;
    constexpr s32 dmgNumberPrototypeIndex = 5;
    constexpr s32 hitPrototypeIndex = 6;

    constexpr u8 playerWingFrameBankOffsets[4] = { 0x00, 0x08, 0x10, 0x18 };
    constexpr u8 playerHeadFrameBankOffsets[9] = { 0x20, 0x24, 0x28, 0x30, 0x34, 0x38, 0x40, 0x44, 0x48 };
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

    constexpr IVec2 playerBowOffsets[3] = { { 10, -4 }, { 9, -14 }, { 10, 4 } };
    constexpr u32 playerBowFwdMetaspriteIndex = 8;
    constexpr u32 playerBowDiagMetaspriteIndex = 9;
    constexpr u32 playerBowArrowFwdMetaspriteIndex = 3;
    constexpr u32 playerBowArrowDiagMetaspriteIndex = 4;

    constexpr IVec2 playerLauncherOffsets[3] = { { 5, -5 }, { 7, -12 }, { 8, 1 } };
    constexpr u32 playerLauncherFwdMetaspriteIndex = 10;
    constexpr u32 playerLauncherDiagMetaspriteIndex = 11;
    constexpr u32 playerLauncherGrenadeMetaspriteIndex = 12;

    constexpr Vec2 viewportScrollThreshold = { 4.0f, 3.0f };

    static void UpdateViewport(bool loadTiles = true) {
        if (pViewportTarget == nullptr) {
            return;
        }

        Vec2 viewportCenter = Vec2{ viewport.x + VIEWPORT_WIDTH_METATILES / 2.0f, viewport.y + VIEWPORT_HEIGHT_METATILES / 2.0f };
        Vec2 targetOffset = pViewportTarget->position - viewportCenter;

        Vec2 delta = { 0.0f, 0.0f };
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

        MoveViewport(&viewport, pNametables, &pCurrentLevel->tilemap, delta.x, delta.y, loadTiles);
    }

    static bool PositionInViewportBounds(Vec2 pos) {
        return pos.x >= viewport.x &&
            pos.x < viewport.x + VIEWPORT_WIDTH_METATILES &&
            pos.y >= viewport.y &&
            pos.y < viewport.y + VIEWPORT_HEIGHT_METATILES;
    }

    static IVec2 WorldPosToScreenPixels(Vec2 pos) {
        return IVec2{
            (s32)round((pos.x - viewport.x) * METATILE_DIM_PIXELS),
            (s32)round((pos.y - viewport.y) * METATILE_DIM_PIXELS)
        };
    }

    static void DrawActor(const Actor* pActor, Sprite** ppNextSprite) {
        // Culling
        if (!PositionInViewportBounds(pActor->position)) {
            return;
        }

        const ActorDrawData& drawData = pActor->drawData;
        IVec2 drawPos = WorldPosToScreenPixels(pActor->position) + drawData.pixelOffset;
        const ActorAnimFrame& frame = pActor->pPrototype->frames[drawData.frameIndex];

        switch (pActor->pPrototype->animMode) {
        case ACTOR_ANIM_MODE_SPRITES: {
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(frame.metaspriteIndex);
            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos + frame.spriteIndex, *ppNextSprite, 1, drawPos, drawData.hFlip, drawData.vFlip, drawData.paletteOverride);
            (*ppNextSprite)++;
            break;
        }
        case ACTOR_ANIM_MODE_METASPRITES: {
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(frame.metaspriteIndex);
            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos, *ppNextSprite, pMetasprite->spriteCount, drawPos, drawData.hFlip, drawData.vFlip, drawData.paletteOverride);
            *ppNextSprite += pMetasprite->spriteCount;
            break;
        }
        default:
            break;
        }
    }

    /*void DrawShield(Sprite** ppNextSprite, r64 dt) {
        // Draw shield around player
        static const r32 shieldRadius = 2.0f;
        static const u32 shieldCount = 4;
        static r32 shieldRot = 0.0f;

        const r32 tangentialVel = playerState.direction * 2.0f + playerState.hSpeed;
        shieldRot += tangentialVel / shieldRadius * dt;

        const Metasprite* shieldMetasprite = Metasprites::GetMetasprite(7);
        const r32 angleOffset = pi * 2 / shieldCount;
        for (u32 i = 0; i < shieldCount; i++) {
            const r32 angle = shieldRot + angleOffset * i;
            Vec2 pos = { cos(angle) * shieldRadius + playerState.x, sin(angle) * shieldRadius + playerState.y };
            IVec2 drawPos = WorldPosToScreenPixels(pos);

            Rendering::Util::CopyMetasprite(shieldMetasprite->spritesRelativePos, *ppNextSprite, shieldMetasprite->spriteCount, drawPos, false, false);
            *ppNextSprite += shieldMetasprite->spriteCount;
        }
    }*/

    static void PlayerInput(Actor* pPlayer, r64 dt) {
        PlayerState& playerState = pPlayer->playerState;
        if (Input::ButtonDown(BUTTON_DPAD_LEFT)) {
            playerState.direction = DirLeft;
            pPlayer->velocity.x = -6.25f;
        }
        else if (Input::ButtonDown(BUTTON_DPAD_RIGHT)) {
            playerState.direction = DirRight;
            pPlayer->velocity.x = 6.25f;
        }
        else {
            pPlayer->velocity.x = 0;
        }

        // Aim mode
        if (Input::ButtonDown(BUTTON_DPAD_UP)) {
            playerState.aimMode = PLAYER_AIM_UP;
        }
        else if (Input::ButtonDown(BUTTON_DPAD_DOWN)) {
            playerState.aimMode = PLAYER_AIM_DOWN;
        }
        else playerState.aimMode = PLAYER_AIM_FWD;

        if (Input::ButtonPressed(BUTTON_START)) {
            pRenderSettings->useCRTFilter = !pRenderSettings->useCRTFilter;
        }

        if (Input::ButtonPressed(BUTTON_A) && (!playerState.inAir || !playerState.doubleJumped)) {
            pPlayer->velocity.y = -15.625f;
            if (playerState.inAir) {
                playerState.doubleJumped = true;
            }

            // Trigger new flap
            playerState.wingFrame++;
        }

        if (pPlayer->velocity.y < 0 && Input::ButtonReleased(BUTTON_A)) {
            pPlayer->velocity.y /= 2;
        }

        playerState.slowFall = true;
        if (Input::ButtonUp(BUTTON_A) || pPlayer->velocity.y < 0) {
            playerState.slowFall = false;
        }

        if (Input::ButtonReleased(BUTTON_B)) {
            playerState.shootCounter = 0.0f;
        }

        if (Input::ButtonPressed(BUTTON_SELECT)) {
            if (playerState.weapon == WpnLauncher) {
                playerState.weapon = WpnBow;
            }
            else playerState.weapon = WpnLauncher;
        }
    }

    static void PlayerShoot(Actor* pPlayer, r64 dt) {
        constexpr r32 shootDelay = 0.16f;

        PlayerState& playerState = pPlayer->playerState;
        if (playerState.shootCounter > dt) {
            playerState.shootCounter -= dt;
        }
        else playerState.shootCounter = 0.0f;

        if (Input::ButtonDown(BUTTON_B) && playerState.shootCounter <= 0.0f) {
            playerState.shootCounter += shootDelay;

            const s32 prototypeIndex = playerState.weapon == WpnLauncher ? playerGrenadePrototypeIndex : playerArrowPrototypeIndex;
            Actor* pBullet = SpawnActor(prototypeIndex);
            if (pBullet != nullptr) {
                const Vec2 fwdOffset = Vec2{ 0.375f * playerState.direction, -0.25f };
                const Vec2 upOffset = Vec2{ 0.1875f * playerState.direction, -0.5f };
                const Vec2 downOffset = Vec2{ 0.25f * playerState.direction, -0.125f };

                pBullet->position = pPlayer->position;
                pBullet->velocity = Vec2{};
                pBullet->gravity = 140;
                pBullet->lifetime = 3.0f;

                if (playerState.aimMode == PLAYER_AIM_FWD) {
                    pBullet->position = pBullet->position + fwdOffset;
                    pBullet->velocity.x = 40.0f * playerState.direction;
                }
                else {
                    pBullet->velocity.x = 28.28f * playerState.direction;
                    pBullet->velocity.y = (playerState.aimMode == PLAYER_AIM_UP) ? -28.28f : 28.28f;
                    pBullet->position = pBullet->position + ((playerState.aimMode == PLAYER_AIM_UP) ? upOffset : downOffset);
                }

                if (playerState.weapon == WpnLauncher) {
                pBullet->velocity = pBullet->velocity * 0.75f;
                }
                pBullet->velocity = pBullet->velocity + pPlayer->velocity * dt;

                InitializeActor(pBullet);
            }
        }
    }

    static void PlayerAnimate(Actor* pPlayer, r64 dt) {
        PlayerState& playerState = pPlayer->playerState;

        // Animate chr sheet using player bank
        const bool jumping = pPlayer->velocity.y < 0;
        const bool descending = !jumping && pPlayer->velocity.y > 0;
        const bool falling = descending && !playerState.slowFall;
        const bool moving = abs(pPlayer->velocity.x) > 0;

        s32 headFrameIndex = PLAYER_HEAD_IDLE;
        if (falling) {
            headFrameIndex = PLAYER_HEAD_FALL;
        }
        else if (moving) {
            headFrameIndex = PLAYER_HEAD_FWD;
        }
        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerHeadFrameBankOffsets[playerState.aimMode * 3 + headFrameIndex],
            pChr[1].tiles + playerHeadFrameChrOffset,
            playerHeadFrameTileCount
        );

        s32 legsFrameIndex = PLAYER_LEGS_IDLE;
        if (jumping) {
            legsFrameIndex = PLAYER_LEGS_JUMP;
        }
        else if (descending) {
            legsFrameIndex = PLAYER_LEGS_FALL;
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
        const r32 wingAnimFrameLength = (jumping || falling) ? 0.09f : 0.18f;
        playerState.wingCounter += dt / wingAnimFrameLength;
        while (playerState.wingCounter > 1.0f) {
            // If wings not in position, keep flapping
            if (!wingsInPosition) {
                playerState.wingFrame++;
            }
            playerState.wingCounter -= 1.0f;
        }
        playerState.wingFrame %= 4;

        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerWingFrameBankOffsets[playerState.wingFrame],
            pChr[1].tiles + playerWingFrameChrOffset,
            playerWingFrameTileCount
        );

        // Setup draw data
        if (pPlayer->velocity.y == 0) {
            pPlayer->drawData.pixelOffset.y = playerState.wingFrame > PLAYER_WINGS_FLAP_START ? -1 : 0;
        }
        else {
            pPlayer->drawData.pixelOffset.y = 0;
        }
        pPlayer->drawData.hFlip = playerState.direction == DirLeft;
        pPlayer->drawData.frameIndex = playerState.aimMode;
    }

    static void DrawPlayerGun(Actor* pPlayer, Sprite** ppNextSprite) {
        ActorDrawData& drawData = pPlayer->drawData;

        IVec2 drawPos = WorldPosToScreenPixels(pPlayer->position);
        drawPos = drawPos + drawData.pixelOffset;

        // Draw weapon first
        IVec2 weaponOffset;
        u8 weaponFrameBankOffset;
        u32 weaponMetaspriteIndex;
        switch (pPlayer->playerState.weapon) {
        case WpnBow: {
            weaponOffset = playerBowOffsets[pPlayer->playerState.aimMode];
            weaponFrameBankOffset = playerBowFrameBankOffsets[pPlayer->playerState.aimMode];
            weaponMetaspriteIndex = pPlayer->playerState.aimMode == PLAYER_AIM_FWD ? playerBowFwdMetaspriteIndex : playerBowDiagMetaspriteIndex;
            break;
        }
        case WpnLauncher: {
            weaponOffset = playerLauncherOffsets[pPlayer->playerState.aimMode];
            weaponFrameBankOffset = playerLauncherFrameBankOffsets[pPlayer->playerState.aimMode];
            weaponMetaspriteIndex = pPlayer->playerState.aimMode == PLAYER_AIM_FWD ? playerLauncherFwdMetaspriteIndex : playerLauncherDiagMetaspriteIndex;
            break;
        }
        default:
            break;
        }
        weaponOffset.x *= pPlayer->playerState.direction;

        Rendering::Util::CopyChrTiles(playerBank.tiles + weaponFrameBankOffset, pChr[1].tiles + playerWeaponFrameChrOffset, playerWeaponFrameTileCount);

        const Metasprite* bowMetasprite = Metasprites::GetMetasprite(weaponMetaspriteIndex);
        Rendering::Util::CopyMetasprite(bowMetasprite->spritesRelativePos, *ppNextSprite, bowMetasprite->spriteCount, drawPos + weaponOffset, drawData.hFlip, drawData.vFlip);
        *ppNextSprite += bowMetasprite->spriteCount;

    }

    static void ActorDie(const PoolHandle<Actor>& handle, std::vector<PoolHandle<Actor>>& removeList) {
        Actor* pActor = actors.Get(handle);
        const u32 flags = pActor->pPrototype->behaviour;

        if (pActor == nullptr) {
            return;
        }

        if (flags & ACTOR_BEHAVIOUR_EXPLODE) {
            // TODO: Make the prototype a struct member
            Actor* pHit = SpawnActor(hitPrototypeIndex);
            if (pHit != nullptr) {
                pHit->position = pActor->position;
                pHit->lifetime = 0.16f;
                pHit->velocity = Vec2{};
                InitializeActor(pHit);
            }
        }

        removeList.push_back(handle);
    }

    static void UpdateActors(Sprite** ppNextSprite, r64 dt) {
        // TODO: Temporarily using a vector for this, change later
        static std::vector<PoolHandle<Actor>> removeList;
        removeList.clear();

        for (u32 i = 0; i < actors.Count(); i++)
        {
            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pActor = actors.Get(handle);
            const u32 flags = pActor->pPrototype->behaviour;

            // Can this ever bet true?
            if (pActor == nullptr) {
                continue;
            }

            // Advance counters:
            if (flags & ACTOR_BEHAVIOUR_HEALTH) {
                if (pActor->damageCounter > dt) {
                    pActor->damageCounter -= dt;
                }
                else pActor->damageCounter = 0.0f;
            }
            if (flags & ACTOR_BEHAVIOUR_LIFETIME) {
                if (pActor->lifetimeCounter > dt) {
                    pActor->lifetimeCounter -= dt;
                }
                else ActorDie(handle, removeList);
            }

            if (flags & ACTOR_BEHAVIOUR_GRAVITY) {
                r32 acc = pActor->gravity;
                if (pActor->playerState.slowFall) {
                    acc /= 4;
                }

                pActor->velocity.y += acc * dt;
            }

            if (flags & ACTOR_BEHAVIOUR_PLAYER_SIDESCROLLER) {
                PlayerInput(pActor, dt);
                PlayerShoot(pActor, dt);
            }

            // TODO: If ignore tile collision?
            const AABB& hitbox = pActor->pPrototype->hitbox;

            const r32 dx = pActor->velocity.x * dt;
            const r32 dy = pActor->velocity.y * dt;

            // TODO: Lots of copypasta here, how to reduce?
            HitResult hit{};
            Collision::SweepBoxHorizontal(&pCurrentLevel->tilemap, hitbox, pActor->position, dx, hit);
            pActor->position.x = hit.location.x;
            if (hit.blockingHit) {
                if (flags & ACTOR_BEHAVIOUR_BOUNCY) {
                    pActor->velocity = pActor->velocity - 2 * DotProduct(pActor->velocity, hit.impactNormal) * hit.impactNormal;
                }
                else if (flags & ACTOR_BEHAVIOUR_FRAGILE) {
                    ActorDie(handle, removeList);
                }
                else {
                    pActor->velocity.x = 0;
                }
            }

            Collision::SweepBoxVertical(&pCurrentLevel->tilemap, hitbox, pActor->position, dy, hit);
            pActor->position.y = hit.location.y;
            if (hit.blockingHit) {
                // Hit ground
                if (DotProduct(hit.impactNormal, { 0, -1 }) > 0.0f) {
                    pActor->playerState.inAir = false;
                    pActor->playerState.doubleJumped = false;
                }

                if (flags & ACTOR_BEHAVIOUR_BOUNCY) {
                    pActor->velocity = pActor->velocity - 2 * DotProduct(pActor->velocity, hit.impactNormal) * hit.impactNormal;
                }
                else if (flags & ACTOR_BEHAVIOUR_FRAGILE) {
                    ActorDie(handle, removeList);
                }
                else {
                    pActor->velocity.y = 0;
                }
            }
            else {
                // TODO: Add coyote time
                pActor->playerState.inAir = true;
            }
        }

        // Naive collision detection
        // TODO: Some kind of spatial partition like a quadtree
        for (u32 i = 0; i < actors.Count(); i++)
        {
            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pActor = actors.Get(handle);
            const u32 flags = pActor->pPrototype->behaviour;

            for (int e = 0; e < actors.Count(); e++) {
                PoolHandle<Actor> otherHandle = actors.GetHandle(e);
                Actor* pOther = actors.Get(otherHandle);
                if (pOther == nullptr) {
                    continue;
                }

                // Don't collide with self
                if (pOther == pActor) {
                    continue;
                }

                const u32 thisLayer = pActor->pPrototype->collisionLayer;
                const u32 otherLayer = pOther->pPrototype->collisionLayer;

                // TODO: Do I want to allow actors on the same layer to collide at some point?
                if (thisLayer == otherLayer) {
                    continue;
                }

                if (thisLayer == ACTOR_COLLISION_LAYER_NONE || otherLayer == ACTOR_COLLISION_LAYER_NONE) {
                    continue;
                }

                // TODO: Figure out a better way to do this
                if ((thisLayer == ACTOR_COLLISION_LAYER_PLAYER && otherLayer == ACTOR_COLLISION_LAYER_PROJECTILE_FRIENDLY) || 
                    (thisLayer == ACTOR_COLLISION_LAYER_PROJECTILE_FRIENDLY && otherLayer == ACTOR_COLLISION_LAYER_PLAYER)) {
                    continue;
                }
                if ((thisLayer == ACTOR_COLLISION_LAYER_ENEMY && otherLayer == ACTOR_COLLISION_LAYER_PROJECTILE_HOSTILE) ||
                    (thisLayer == ACTOR_COLLISION_LAYER_PROJECTILE_HOSTILE && otherLayer == ACTOR_COLLISION_LAYER_ENEMY)) {
                    continue;
                }

                if (thisLayer == ACTOR_COLLISION_LAYER_ENEMY && otherLayer == ACTOR_COLLISION_LAYER_PLAYER) {
                    continue;
                }

                const AABB& hitbox = pActor->pPrototype->hitbox;
                const AABB& hitboxOther = pOther->pPrototype->hitbox;
                if (Collision::BoxesOverlap(hitbox, pActor->position, hitboxOther, pOther->position)) {
                    if (flags & ACTOR_BEHAVIOUR_FRAGILE) {
                        ActorDie(handle, removeList);
                    }
                    // Take damage
                    else if (flags & ACTOR_BEHAVIOUR_HEALTH) {
                        // TODO: Implement iframes
                        const u32 damage = (rand() % 2) + 1;
                        pActor->health -= damage;
                        pActor->damageCounter = damageDelay;

                        // Spawn damage numbers
                        Actor* pDmg = SpawnActor(dmgNumberPrototypeIndex);
                        if (pDmg != nullptr) {
                            pDmg->drawNumber = damage;

                            // Random point inside hitbox
                            const Vec2 randomPointInsideHitbox = {
                                ((r32)rand() / RAND_MAX) * (hitbox.x2 - hitbox.x1) + hitbox.x1,
                                ((r32)rand() / RAND_MAX)* (hitbox.y2 - hitbox.y1) + hitbox.y1
                            };
                            pDmg->position = pActor->position + randomPointInsideHitbox;

                            pDmg->lifetime = 1.0f;
                            pDmg->velocity = { 0, -1.5f };

                            InitializeActor(pDmg);
                        }

                        if (pActor->health <= 0) {
                            ActorDie(handle, removeList);
                        }
                    }
                }
            }
        }

        for (u32 i = 0; i < actors.Count(); i++)
        {
            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pActor = actors.Get(handle);
            const u32 flags = pActor->pPrototype->behaviour;
            ActorDrawData& drawData = pActor->drawData;
            const u32 frameCount = pActor->pPrototype->frameCount;

            // Numbers drawn separately from the actor itself, so it can be invisible
            if (flags & ACTOR_BEHAVIOUR_NUMBERS) {
                static char numberStr[16]{};

                _itoa_s(pActor->drawNumber, numberStr, 10);
                const u32 strLength = strlen(numberStr);

                // Ascii character '0' = 0x30
                constexpr u8 chrOffset = 0x30;

                for (u32 c = 0; c < strLength; c++) {
                    // TODO: What about metasprite frames? Handle this more rigorously!
                    const s32 frameIndex = (numberStr[c] - chrOffset) % frameCount;
                    const ActorAnimFrame& frame = pActor->pPrototype->frames[frameIndex];
                    const u8 tileId = Metasprites::GetMetasprite(frame.metaspriteIndex)->spritesRelativePos[frame.spriteIndex].tileId;

                    const IVec2 pixelPos = WorldPosToScreenPixels(pActor->position);
                    const Sprite sprite = {
                        pixelPos.y,
                        pixelPos.x + c * 5,
                        tileId,
                        1
                    };
                    *((*ppNextSprite)++) = sprite;
                }
            }

            if (flags & ACTOR_BEHAVIOUR_RENDERABLE) {
                if (flags & ACTOR_BEHAVIOUR_PLAYER_SIDESCROLLER) {
                    PlayerAnimate(pActor, dt);
                    DrawPlayerGun(pActor, ppNextSprite);
                } 
                else if (flags & ACTOR_BEHAVIOUR_ANIM_LIFE) {
                    drawData.frameIndex = std::min((u32)roundf((pActor->lifetimeCounter / pActor->lifetime) * frameCount), frameCount - 1);
                }
                else if (flags & ACTOR_BEHAVIOUR_ANIM_DIR) {
                    const Vec2 dir = pActor->velocity.Normalize();
                    const r32 angle = atan2f(dir.y, dir.x);
                    drawData.frameIndex = (s32)roundf(((angle + pi) / (pi * 2)) * frameCount) % frameCount;
                }

                ActorDrawData& drawData = pActor->drawData;
                drawData.paletteOverride = (pActor->damageCounter > 0) ? (s32)(gameplaySecondsElapsed * 20) % 4 : -1;
                DrawActor(pActor, ppNextSprite);
            }
        }

        for (auto& handle : removeList) {
            actors.Remove(handle);
        }
    }

    void Step(r64 dt) {
        secondsElapsed += dt;

        static Sprite* pNextSprite = pSprites;

        const u32 spritesToClear = pNextSprite - pSprites;
        Rendering::Util::ClearSprites(pSprites, spritesToClear);
        pNextSprite = pSprites;

        if (!paused) {
            gameplaySecondsElapsed += dt;

            UpdateActors(&pNextSprite, dt);

            UpdateViewport();
        }

        UpdateScreenScroll();

        // Animate color palette brightness
        /*r32 deltaBrightness = sin(gameplaySecondsElapsed * 1.5f);
        for (u32 i = 0; i < Rendering::paletteCount * Rendering::paletteColorCount; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            s32 brightness = (baseColor & 0b1110000) >> 4;
            s32 d = (s32)roundf(deltaBrightness * 8);

            s32 newBrightness = brightness + d;
            newBrightness = (newBrightness < 0) ? 0 : (newBrightness > 7) ? 7 : newBrightness;

            u8 newColor = (baseColor & 0b0001111) | (newBrightness << 4);
            ((u8*)pPalettes)[i] = newColor;
        }*/

        // Animate color palette hue
        /*s32 hueShift = (s32)roundf(gameplaySecondsElapsed * 5.0f);
        for (u32 i = 0; i < Rendering::paletteCount * Rendering::paletteColorCount; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            s32 hue = baseColor & 0b1111;

            s32 newHue = hue + hueShift;
            newHue &= 0b1111;

            u8 newColor = (baseColor & 0b1110000) | newHue;
            ((u8*)pPalettes)[i] = newColor;
        }*/

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
    Pool<Actor>* GetActors() {
        return &actors;
    }
}