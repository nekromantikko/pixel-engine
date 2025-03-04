#include "game.h"
#include "system.h"
#include "game_input.h"
#include "game_state.h"
#include <cstring>
#include <cstdio>
#include "level.h"
#include "game_rendering.h"
#include "collision.h"
#include "metasprite.h"
#include "tiles.h"
#include <imgui.h>
#include <vector>
#include "audio.h"
#include "nes_timing.h"
#include <gtc/constants.hpp>
#include "random.h"
#include "fixed_hash_map.h"
#include "coroutines.h"
#include "dialog.h"
#include "game_ui.h"
#include "actor_prototypes.h"

namespace Game {
    r64 secondsElapsed = 0.0f;
    u32 clockCounter = 0;

    // Rendering data
    RenderSettings* pRenderSettings;
    ChrSheet* pChr;
    Nametable* pNametables;
    Scanline* pScanlines;
    Palette* pPalettes;

    bool paused = false;

    Sound jumpSfx;
    Sound gunSfx;
    Sound ricochetSfx;
    Sound damageSfx;
    Sound expSfx;
    Sound enemyDieSfx;

    //Sound bgm;
    //bool musicPlaying = false;

    // TODO: Try to eliminate as much of this as possible
    constexpr s32 enemyFireballPrototypeIndex = 8;
    constexpr s32 haloSmallPrototypeIndex = 0x0a;
    constexpr s32 haloLargePrototypeIndex = 0x0b;
    
#pragma region Custom draw functions
    // Calls itoa, but adds a plus sign if value is positive
    static u32 ItoaSigned(s16 value, char* str) {
        s32 i = 0;
        if (value > 0) {
            str[i++] = '+';
        }

        itoa(value, str + i, 10);

        return strlen(str);
    }

    static void DrawNumbers(Actor* pActor) {
        const Animation& currentAnim = pActor->pPrototype->animations[pActor->drawState.animIndex];
        if (currentAnim.type != ANIMATION_TYPE_SPRITES) {
            return;
        }

        static char numberStr[16]{};
        const u32 strLength = ItoaSigned(pActor->state.effectState.value, numberStr);

        // Ascii character '*' = 0x2A
        constexpr u8 chrOffset = 0x2A;

        const s32 frameCount = currentAnim.frameCount;
        const glm::i16vec2 pixelPos = Rendering::WorldPosToScreenPixels(pActor->position);

        for (u32 c = 0; c < strLength; c++) {
			glm::i16vec2 drawPos = pixelPos + glm::i16vec2{ c * 5, 0 };
            const s32 frameIndex = (numberStr[c] - chrOffset) % frameCount;

			Rendering::DrawMetaspriteSprite(pActor->drawState.layer, currentAnim.metaspriteIndex, frameIndex, drawPos, false, false, -1);
        }
    }
#pragma endregion

#pragma region Collision
    
    static bool ActorCollidesWithPlayer(Actor* pActor, Actor* pPlayer) {
        if (pPlayer == nullptr || GetPlayerHealth() == 0) {
            return false;
        }

        return ActorsColliding(pActor, pPlayer);
    }
#pragma endregion



#pragma region Damage
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

            const r32 speed = Random::GenerateReal(0.1f, 0.3f);
            const glm::vec2 velocity = Random::GenerateDirection() * speed;

            Actor* pSpawned = SpawnActor(prototypeIndex, state.position, velocity);

            pSpawned->state.pickupState.lingerCounter = 30;
            pSpawned->flags.facingDir = (s8)Random::GenerateInt(-1, 1);
            pSpawned->state.pickupState.value = pSpawned->pPrototype->data.pickupData.value;

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

        PersistedActorData persistData = GetPersistedActorData(pActor->id);
        persistData.dead = true;
        SetPersistedActorData(pActor->id, persistData);

        Audio::PlaySFX(&enemyDieSfx, CHAN_ID_NOISE);
        SpawnActor(pActor->pPrototype->data.npcData.spawnOnDeath, pActor->position);

        // Spawn exp halos
        const u16 totalExpValue = pActor->pPrototype->data.npcData.expValue;
        if (totalExpValue > 0) {
            SpawnExpState coroutineState = {
                .position = pActor->position,
                .remainingValue = totalExpValue
            };
            StartCoroutine(SpawnExpCoroutine, coroutineState);
        }
    }

#pragma endregion

#pragma region Bullets
    static void BulletDie(Actor* pBullet, const glm::vec2& effectPos) {
        pBullet->flags.pendingRemoval = true;
        SpawnActor(pBullet->pPrototype->data.bulletData.spawnOnDeath, effectPos);
    }

    static void HandleBulletEnemyCollision(Actor* pBullet, Actor* pEnemy) {
        BulletDie(pBullet, pBullet->position);

        const u32 damage = Random::GenerateInt(1, 2);
        const u16 newHealth = ActorTakeDamage(pEnemy, damage, pEnemy->state.npcState.health, pEnemy->state.npcState.damageCounter);
        if (newHealth == 0) {
            NPCDie(pEnemy);
        }
        pEnemy->state.npcState.health = newHealth;
    }

	static bool ActorIsHostileNPC(const Actor* pActor) {
		return pActor->pPrototype->type == ACTOR_TYPE_NPC && pActor->pPrototype->alignment == ACTOR_ALIGNMENT_HOSTILE;
	}

    static void BulletCollision(Actor* pActor) {
        if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_FRIENDLY) {
            ForEachActorCollision(pActor, HandleBulletEnemyCollision, ActorIsHostileNPC);
        }
        else if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_HOSTILE) {
            Actor* pPlayer = GetPlayer();
            if (ActorCollidesWithPlayer(pActor, pPlayer)) {
                HandlePlayerEnemyCollision(pPlayer, pActor);
                BulletDie(pActor, pActor->position);
            }
            // TODO: Collision with friendly NPC:s? Does this happen in the game?
        }
    }

    static void UpdateDefaultBullet(Actor* pActor) {
        if (!UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
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
    }

    static void BulletRicochet(glm::vec2& velocity, const glm::vec2& normal) {
        velocity = glm::reflect(velocity, normal);
        //Audio::PlaySFX(&ricochetSfx, CHAN_ID_PULSE0);
    }

    static void UpdateGrenade(Actor* pActor) {
        if (!UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
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
    }

    static void UpdateFireball(Actor* pActor) {
        if (!UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
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
    }
#pragma endregion

#pragma region NPC
    static void UpdateSlimeEnemy(Actor* pActor) {
        UpdateCounter(pActor->state.npcState.damageCounter);

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

        Actor* pPlayer = GetPlayer();
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

		pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
        SetDamagePaletteOverride(pActor, pActor->state.npcState.damageCounter);
    }

    static void UpdateSkullEnemy(Actor* pActor) {
        UpdateCounter(pActor->state.npcState.damageCounter);

        ActorFacePlayer(pActor);

        static const r32 amplitude = 4.0f;
        const r32 sineTime = glm::sin(GetFramesElapsed() / 60.f);
        pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

        // Shoot fireballs
        const bool shouldFire = Random::GenerateInt(0, 127) == 0;
        if (shouldFire) {

            Actor* pPlayer = GetPlayer();
            if (pPlayer != nullptr) {
                const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
                const glm::vec2 velocity = playerDir * 0.0625f;

                SpawnActor(enemyFireballPrototypeIndex, pActor->position, velocity);
            }
        }

        Actor* pPlayer = GetPlayer();
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

        pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
        SetDamagePaletteOverride(pActor, pActor->state.npcState.damageCounter);
    }
#pragma endregion

#pragma region Pickups
    static void UpdateExpHalo(Actor* pActor) {
        Actor* pPlayer = GetPlayer();

        const glm::vec2 playerVec = pPlayer->position - pActor->position;
        const glm::vec2 playerDir = glm::normalize(playerVec);
        const r32 playerDist = glm::length(playerVec);

        // Wait for a while before homing towards player
        if (!UpdateCounter(pActor->state.pickupState.lingerCounter)) {
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

            AddPlayerExp(pActor->state.pickupState.value);

            return;
        }

        // Smoothstep animation when inside specified radius from player
        const Animation& currentAnim = pActor->pPrototype->animations[0];
        constexpr r32 animRadius = 4.0f;

        pActor->drawState.frameIndex = glm::floor((1.0f - glm::smoothstep(0.0f, animRadius, playerDist)) * currentAnim.frameCount);
		pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    }

    static void UpdateExpRemnant(Actor* pActor) {
        Actor* pPlayer = GetPlayer();
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            Audio::PlaySFX(&expSfx, CHAN_ID_PULSE0);
            pActor->flags.pendingRemoval = true;

            ClearExpRemnant();
			AddPlayerExp(pActor->state.pickupState.value);

            return;
        }
    }
#pragma endregion

#pragma region Effects
    static void UpdateDefaultEffect(Actor* pActor) {
        if (!UpdateCounter(pActor->state.effectState.lifetimeCounter)) {
            pActor->flags.pendingRemoval = true;
        }
    }

    static void UpdateExplosion(Actor* pActor) {
        UpdateDefaultEffect(pActor);

        AdvanceCurrentAnimation(pActor);
    }

    static void UpdateNumbers(Actor* pActor) {
        UpdateDefaultEffect(pActor);

        pActor->position.y += pActor->velocity.y;

        DrawNumbers(pActor);
    }

    static void UpdateFeather(Actor* pActor) {
        UpdateDefaultEffect(pActor);

        constexpr r32 maxFallSpeed = 0.03125f;
        ApplyGravity(pActor, 0.005f);
        if (pActor->velocity.y > maxFallSpeed) {
            pActor->velocity.y = maxFallSpeed;
        }

        constexpr r32 amplitude = 2.0f;
        constexpr r32 timeMultiplier = 1 / 30.f;
        const u16 time = pActor->state.effectState.lifetimeCounter - pActor->state.effectState.lifetime;
        const r32 sineTime = glm::sin(time * timeMultiplier);
        pActor->velocity.x = pActor->initialVelocity.x * sineTime;

        pActor->position += pActor->velocity;
    }
#pragma endregion


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
        case PICKUP_SUBTYPE_XP_REMNANT: {
            UpdateExpRemnant(pActor);
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
        case EFFECT_SUBTYPE_FEATHER: {
            UpdateFeather(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdateCheckpoint(Actor* pActor) {
        if (pActor->state.checkpointState.activated) {
            pActor->drawState.animIndex = 1;
        }
        else {
            pActor->drawState.animIndex = 0;
        }
    }

    static void TempActorUpdateCallback(Actor* pActor) {
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
        case ACTOR_TYPE_CHECKPOINT: {
            UpdateCheckpoint(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void Step() {
        if (!paused) {
            StepFrame(TempActorUpdateCallback);
        }

        /*if (ButtonPressed(BUTTON_START)) {
            if (!musicPlaying) {
                Audio::PlayMusic(&bgm, true);
            }
            else {
                Audio::StopMusic();
            }
            musicPlaying = !musicPlaying;
        }*/

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
    void Initialize() {
        // Rendering data
        pRenderSettings = ::Rendering::GetSettingsPtr();
        pChr = ::Rendering::GetChrPtr(0);
        pPalettes = ::Rendering::GetPalettePtr(0);
        pNametables = ::Rendering::GetNametablePtr(0);
        pScanlines = ::Rendering::GetScanlinePtr(0);

        Rendering::Init();

        Tiles::LoadTileset("assets/forest.til");
        Metasprites::Load("assets/meta.spr");
        Levels::LoadLevels("assets/levels.lev");
        Assets::LoadActorPrototypes("assets/actors.prt");

        // TEMP SOUND STUFF
        jumpSfx = Audio::LoadSound("assets/jump.nsf");
        gunSfx = Audio::LoadSound("assets/gun1.nsf");
        ricochetSfx = Audio::LoadSound("assets/ricochet.nsf");
        damageSfx = Audio::LoadSound("assets/damage.nsf");
        expSfx = Audio::LoadSound("assets/exp.nsf");
        enemyDieSfx = Audio::LoadSound("assets/enemydie.nsf");
        //bgm = Audio::LoadSound("assets/music.nsf");

		InitGameData();
        InitGameState(GAME_STATE_PLAYING);

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
        Rendering::ClearSpriteLayers();
        paused = p;
    }
#pragma endregion
}