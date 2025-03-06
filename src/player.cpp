#include "player.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_input.h"
#include "game_state.h"
#include "dialog.h"
#include "level.h"

enum PlayerHeadFrame : u8 {
    PLAYER_HEAD_IDLE,
    PLAYER_HEAD_FWD,
    PLAYER_HEAD_FALL,
    PLAYER_HEAD_DMG
};

enum PlayerLegsFrame : u8 {
    PLAYER_LEGS_IDLE,
    PLAYER_LEGS_FWD,
    PLAYER_LEGS_JUMP,
    PLAYER_LEGS_FALL
};

enum PlayerWingsFrame : u8 {
    PLAYER_WINGS_DESCEND,
    PLAYER_WINGS_FLAP_START,
    PLAYER_WINGS_ASCEND,
    PLAYER_WINGS_FLAP_END,

    PLAYER_WING_FRAME_COUNT
};

enum PlayerAimFrame : u8 {
    PLAYER_AIM_FWD,
    PLAYER_AIM_UP,
    PLAYER_AIM_DOWN
};

enum PlayerSitDownState : u8 {
    PLAYER_STANDING = 0b00,
    PLAYER_SITTING = 0b01,
    PLAYER_SIT_TO_STAND = 0b10,
    PLAYER_STAND_TO_SIT = 0b11
};

// TODO: These should be set in editor somehow
constexpr s32 playerGrenadePrototypeIndex = 1;
constexpr s32 playerArrowPrototypeIndex = 4;
constexpr s32 featherPrototypeIndex = 0x0e;

// TODO: Would it be possible to define these in the editor?
constexpr u8 playerWingFrameBankOffsets[4] = { 0x00, 0x08, 0x10, 0x18 };
constexpr u8 playerHeadFrameBankOffsets[12] = { 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C };
constexpr u8 playerLegsFrameBankOffsets[4] = { 0x50, 0x54, 0x58, 0x5C };
constexpr u8 playerSitBankOffsets[2] = { 0x60, 0x68 };
constexpr u8 playerDeadBankOffsets[2] = { 0x70, 0x78 };
constexpr u8 playerBowFrameBankOffsets[3] = { 0x80, 0x88, 0x90 };
constexpr u8 playerLauncherFrameBankOffsets[3] = { 0xA0, 0xA8, 0xB0 };

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

static void HandleLevelExit(const Actor* pPlayer) {
    if (pPlayer == nullptr) {
        return;
    }

    Level* pCurrentLevel = Game::GetCurrentLevel();
    if (pCurrentLevel == nullptr) {
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

        Game::TriggerLevelTransition(exit.targetLevel, exit.targetScreen, enterDirection);
    }
}

static void PlayerRevive() {
    // TODO: Animate standing up

    // Restore life
    Game::SetPlayerHealth(Game::GetPlayerMaxHealth());
}

static void PlayerDie(Actor* pPlayer) {
    Level* pCurrentLevel = Game::GetCurrentLevel();

    Game::SetExpRemnant(Levels::GetIndex(pCurrentLevel), pPlayer->position, Game::GetPlayerExp());
    Game::SetPlayerExp(0);

    // Transition to checkpoint
    const Checkpoint checkpoint = Game::GetCheckpoint();
    Game::TriggerLevelTransition(checkpoint.levelIndex, checkpoint.screenIndex, SCREEN_EXIT_DIR_DEATH_WARP, PlayerRevive);
}

static void SpawnFeathers(Actor* pPlayer, u32 count) {
    for (u32 i = 0; i < count; i++) {
        const glm::vec2 spawnOffset = {
            Random::GenerateReal(-1.0f, 1.0f),
            Random::GenerateReal(-1.0f, 1.0f)
        };

        const glm::vec2 velocity = Random::GenerateDirection() * 0.0625f;
        Actor* pSpawned = Game::SpawnActor(featherPrototypeIndex, pPlayer->position + spawnOffset, velocity);
        pSpawned->drawState.frameIndex = Random::GenerateInt(0, pSpawned->pPrototype->animations[0].frameCount - 1);
    }
}

static void PlayerMortalHit(Actor* pPlayer) {
    Game::TriggerScreenShake(2, 30, true);
    pPlayer->velocity.y = -0.25f;
    pPlayer->state.playerState.deathCounter = 240;
}



static void PlayerSitDown(Actor* pPlayer) {
    pPlayer->state.playerState.flags.sitState = PLAYER_STAND_TO_SIT;
    pPlayer->state.playerState.sitCounter = 15;
}

static void PlayerStandUp(Actor* pPlayer) {
    pPlayer->state.playerState.flags.sitState = PLAYER_SIT_TO_STAND;
    pPlayer->state.playerState.sitCounter = 15;
}

static void TriggerInteraction(Actor* pPlayer, Actor* pInteractable) {
    if (pInteractable == nullptr) {
        return;
    }

    if (pInteractable->pPrototype->subtype == INTERACTABLE_TYPE_CHECKPOINT) {
        pInteractable->state.checkpointState.activated = true;

        Game::ActivateCheckpoint(pInteractable);

        // Sit down
        PlayerSitDown(pPlayer);

        // Add dialogue
        static constexpr const char* lines[] = {
            "I just put a regular dialogue box here, but it would\nnormally be a level up menu.",
        };

        if (!Game::IsDialogActive()) {
            Game::OpenDialog(lines, 1);
        }
    }
}

static void PlayerShoot(Actor* pPlayer) {
    constexpr s32 shootDelay = 10;

    PlayerState& playerState = pPlayer->state.playerState;
    Game::UpdateCounter(playerState.shootCounter);

    if (Game::Input::ButtonDown(BUTTON_B) && playerState.shootCounter == 0) {
        playerState.shootCounter = shootDelay;

        const u16 playerWeapon = Game::GetPlayerWeapon();
        const s32 prototypeIndex = playerWeapon == PLAYER_WEAPON_LAUNCHER ? playerGrenadePrototypeIndex : playerArrowPrototypeIndex;
        Actor* pBullet = Game::SpawnActor(prototypeIndex, pPlayer->position);
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
            //Audio::PlaySFX(&gunSfx, CHAN_ID_NOISE);
        }

    }
}

static void PlayerInput(Actor* pPlayer) {
    constexpr r32 maxSpeed = 0.09375f; // Actual movement speed from Zelda 2
    constexpr r32 acceleration = maxSpeed / 24.f; // Acceleration from Zelda 2

    const bool dead = Game::GetPlayerHealth() == 0;
    const bool enteringLevel = pPlayer->state.playerState.entryDelayCounter > 0;
    const bool stunned = pPlayer->state.playerState.damageCounter > 0;
    const bool sitting = pPlayer->state.playerState.flags.sitState != PLAYER_STANDING;

    const bool inputDisabled = dead || enteringLevel || stunned || sitting || Game::IsDialogActive();

    PlayerState& playerState = pPlayer->state.playerState;
    if (!inputDisabled && Game::Input::ButtonDown(BUTTON_DPAD_LEFT)) {
        pPlayer->velocity.x -= acceleration;
        if (pPlayer->flags.facingDir != ACTOR_FACING_LEFT) {
            pPlayer->velocity.x -= acceleration;
        }

        pPlayer->velocity.x = glm::clamp(pPlayer->velocity.x, -maxSpeed, maxSpeed);
        pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
    }
    else if (!inputDisabled && Game::Input::ButtonDown(BUTTON_DPAD_RIGHT)) {
        pPlayer->velocity.x += acceleration;
        if (pPlayer->flags.facingDir != ACTOR_FACING_RIGHT) {
            pPlayer->velocity.x += acceleration;
        }

        pPlayer->velocity.x = glm::clamp(pPlayer->velocity.x, -maxSpeed, maxSpeed);
        pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
    }
    else if (!enteringLevel && !pPlayer->flags.inAir && pPlayer->velocity.x != 0.0f) { // Decelerate
        pPlayer->velocity.x -= acceleration * glm::sign(pPlayer->velocity.x);
    }

    // Interaction / Shooting
    if (Game::IsDialogActive() && Game::Input::ButtonPressed(BUTTON_B)) {
        Game::AdvanceDialogText();
    }
    else if (!inputDisabled) {
        Actor* pInteractable = Game::GetFirstActorCollision(pPlayer, ACTOR_TYPE_INTERACTABLE);
        if (pInteractable && Game::Input::ButtonPressed(BUTTON_B)) {
            TriggerInteraction(pPlayer, pInteractable);
        }
        else PlayerShoot(pPlayer);
    }

    if (inputDisabled) {
        if (!Game::IsDialogActive() && pPlayer->state.playerState.flags.sitState == PLAYER_SITTING && Game::Input::AnyButtonDown()) {
            PlayerStandUp(pPlayer);
        }

        return;
    }

    // Aim mode
    if (Game::Input::ButtonDown(BUTTON_DPAD_UP)) {
        playerState.flags.aimMode = PLAYER_AIM_UP;
    }
    else if (Game::Input::ButtonDown(BUTTON_DPAD_DOWN)) {
        playerState.flags.aimMode = PLAYER_AIM_DOWN;
    }
    else playerState.flags.aimMode = PLAYER_AIM_FWD;

    if (Game::Input::ButtonPressed(BUTTON_A) && (!pPlayer->flags.inAir || !playerState.flags.doubleJumped)) {
        pPlayer->velocity.y = -0.25f;
        if (pPlayer->flags.inAir) {
            playerState.flags.doubleJumped = true;
        }

        // Trigger new flap by taking wings out of falling position by advancing the frame index
        playerState.wingFrame = ++playerState.wingFrame % PLAYER_WING_FRAME_COUNT;

        //Audio::PlaySFX(&jumpSfx, CHAN_ID_PULSE0);
    }

    if (pPlayer->velocity.y < 0 && Game::Input::ButtonReleased(BUTTON_A)) {
        pPlayer->velocity.y /= 2;
    }

    if (Game::Input::ButtonDown(BUTTON_A) && pPlayer->velocity.y > 0) {
        playerState.flags.slowFall = true;
    }

    if (Game::Input::ButtonReleased(BUTTON_B)) {
        playerState.shootCounter = 0.0f;
    }

    if (Game::Input::ButtonPressed(BUTTON_SELECT)) {
        u16 playerWeapon = Game::GetPlayerWeapon();
        if (playerWeapon == PLAYER_WEAPON_LAUNCHER) {
            Game::SetPlayerWeapon(PLAYER_WEAPON_BOW);
        }
        else Game::SetPlayerWeapon(PLAYER_WEAPON_LAUNCHER);
    }
}

static void AnimatePlayerDead(Actor* pPlayer) {
    u8 frameIdx = !pPlayer->flags.inAir;

    Game::Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerDeadBankOffsets[frameIdx], 1, playerHeadFrameChrOffset, 8);

    pPlayer->drawState.animIndex = 2;
    pPlayer->drawState.frameIndex = frameIdx;
    pPlayer->drawState.pixelOffset = { 0, 0 };
    pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
    pPlayer->drawState.useCustomPalette = false;
}

static void AnimatePlayerSitting(Actor* pPlayer) {
    u8 frameIdx = 1;

    // If in transition state
    if (pPlayer->state.playerState.flags.sitState & 0b10) {
        frameIdx = ((pPlayer->state.playerState.flags.sitState & 0b01) ^ (pPlayer->state.playerState.sitCounter >> 3)) & 1;
    }

    Game::Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerSitBankOffsets[frameIdx], 1, playerHeadFrameChrOffset, 8);

    // Get wings in sitting position
    const bool wingsInPosition = pPlayer->state.playerState.wingFrame == PLAYER_WINGS_FLAP_END;
    const u16 wingAnimFrameLength = 6;

    if (!wingsInPosition) {
        Game::AdvanceAnimation(pPlayer->state.playerState.wingCounter, pPlayer->state.playerState.wingFrame, PLAYER_WING_FRAME_COUNT, wingAnimFrameLength, 0);
    }

    Game::Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerWingFrameBankOffsets[pPlayer->state.playerState.wingFrame], 1, playerWingFrameChrOffset, playerWingFrameTileCount);

    pPlayer->drawState.animIndex = 1;
    pPlayer->drawState.frameIndex = 0;
    pPlayer->drawState.pixelOffset = { 0, 0 };
    pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
    pPlayer->drawState.useCustomPalette = false;
}

static void AnimatePlayer(Actor* pPlayer) {
    PlayerState& playerState = pPlayer->state.playerState;

    if (Game::GetPlayerHealth() == 0) {
        AnimatePlayerDead(pPlayer);
        return;
    }

    if (playerState.flags.sitState != PLAYER_STANDING) {
        AnimatePlayerSitting(pPlayer);
        return;
    }

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
    Game::Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerHeadFrameBankOffsets[playerState.flags.aimMode * 4 + headFrameIndex], 1, playerHeadFrameChrOffset, playerHeadFrameTileCount);

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
    Game::Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerLegsFrameBankOffsets[legsFrameIndex], 1, playerLegsFrameChrOffset, playerLegsFrameTileCount);

    // When jumping or falling, wings get into proper position and stay there for the duration of the jump/fall
    const bool wingsInPosition = (jumping && playerState.wingFrame == PLAYER_WINGS_ASCEND) || (falling && playerState.wingFrame == PLAYER_WINGS_DESCEND);

    // Wings flap faster to get into proper position
    const u16 wingAnimFrameLength = (jumping || falling) ? 6 : 12;

    if (!wingsInPosition) {
        Game::AdvanceAnimation(playerState.wingCounter, playerState.wingFrame, PLAYER_WING_FRAME_COUNT, wingAnimFrameLength, 0);
    }

    Game::Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerWingFrameBankOffsets[playerState.wingFrame], 1, playerWingFrameChrOffset, playerWingFrameTileCount);

    // Setup draw data
    s32 vOffset = 0;
    if (pPlayer->velocity.y == 0) {
        vOffset = playerState.wingFrame > PLAYER_WINGS_FLAP_START ? -1 : 0;
    }

    pPlayer->drawState.animIndex = 0;
    pPlayer->drawState.frameIndex = playerState.flags.aimMode;
    pPlayer->drawState.pixelOffset = { 0, vOffset };
    pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
    Game::SetDamagePaletteOverride(pPlayer, playerState.damageCounter);
}

static void UpdatePlayerSidescroller(Actor* pActor) {
    Game::UpdateCounter(pActor->state.playerState.entryDelayCounter);
    Game::UpdateCounter(pActor->state.playerState.damageCounter);

    if (!Game::UpdateCounter(pActor->state.playerState.sitCounter)) {
        pActor->state.playerState.flags.sitState &= 1;
    }

    if (pActor->state.playerState.deathCounter != 0) {
        if (!Game::UpdateCounter(pActor->state.playerState.deathCounter)) {
            PlayerDie(pActor);
        }
    }

    // Reset slow fall
    pActor->state.playerState.flags.slowFall = false;

    PlayerInput(pActor);

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        pActor->velocity.x = 0.0f;
    }

    constexpr r32 playerGravity = 0.01f;
    constexpr r32 playerSlowGravity = playerGravity / 4;

    const r32 gravity = pActor->state.playerState.flags.slowFall ? playerSlowGravity : playerGravity;
    Game::ApplyGravity(pActor, gravity);

    // Reset in air flag
    pActor->flags.inAir = true;

    if (Game::ActorMoveVertical(pActor, hit)) {
        pActor->velocity.y = 0.0f;

        if (hit.impactNormal.y < 0.0f) {
            pActor->flags.inAir = false;
            pActor->state.playerState.flags.doubleJumped = false;
        }
    }

    AnimatePlayer(pActor);

    HandleLevelExit(pActor);
}

static void UpdatePlayerOverworld(Actor* pPlayer) {

}

static bool DrawPlayerGun(const Actor* pPlayer) {
    const ActorDrawState& drawState = pPlayer->drawState;
    glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels(pPlayer->position) + drawState.pixelOffset;

    // Draw weapon first
    glm::i16vec2 weaponOffset{};
    u8 weaponFrameBankOffset;
    u32 weaponMetaspriteIndex;
    const u16 playerWeapon = Game::GetPlayerWeapon();
    switch (playerWeapon) {
    case PLAYER_WEAPON_BOW: {
        weaponOffset = playerBowOffsets[pPlayer->state.playerState.flags.aimMode];
        weaponFrameBankOffset = playerBowFrameBankOffsets[pPlayer->state.playerState.flags.aimMode];
        weaponMetaspriteIndex = pPlayer->state.playerState.flags.aimMode == PLAYER_AIM_FWD ? playerBowFwdMetaspriteIndex : playerBowDiagMetaspriteIndex;
        break;
    }
    case PLAYER_WEAPON_LAUNCHER: {
        weaponOffset = playerLauncherOffsets[pPlayer->state.playerState.flags.aimMode];
        weaponFrameBankOffset = playerLauncherFrameBankOffsets[pPlayer->state.playerState.flags.aimMode];
        weaponMetaspriteIndex = pPlayer->state.playerState.flags.aimMode == PLAYER_AIM_FWD ? playerLauncherFwdMetaspriteIndex : playerLauncherDiagMetaspriteIndex;
        break;
    }
    default:
        break;
    }
    weaponOffset.x *= pPlayer->flags.facingDir;

    Game::Rendering::CopyBankTiles(PLAYER_BANK_INDEX, weaponFrameBankOffset, 1, playerWeaponFrameChrOffset, playerWeaponFrameTileCount);
    return Game::Rendering::DrawMetasprite(SPRITE_LAYER_FG, weaponMetaspriteIndex, drawPos + weaponOffset, drawState.hFlip, drawState.vFlip, -1);
}

static bool DrawPlayerSidescroller(const Actor* pPlayer) {
    if (Game::GetPlayerHealth() != 0 && pPlayer->state.playerState.flags.sitState == PLAYER_STANDING) {
        DrawPlayerGun(pPlayer);
    }
    return Game::DrawActorDefault(pPlayer);
}

static bool DrawPlayerOverworld(const Actor* pPlayer) {
    return false;
}

void InitPlayerSidescroller(Actor* pPlayer, const PersistedActorData* pPersistData) {
    pPlayer->state.playerState.entryDelayCounter = 0;
    pPlayer->state.playerState.deathCounter = 0;
    pPlayer->state.playerState.damageCounter = 0;
    pPlayer->state.playerState.sitCounter = 0;
    pPlayer->state.playerState.flags.aimMode = PLAYER_AIM_FWD;
    pPlayer->state.playerState.flags.doubleJumped = false;
    pPlayer->state.playerState.flags.sitState = PLAYER_STANDING;
    pPlayer->state.playerState.flags.slowFall = false;
    pPlayer->drawState.layer = SPRITE_LAYER_FG;
}

static void InitPlayerOverworld(Actor* pPlayer, const PersistedActorData* pPersistData) {

}

#pragma region Public API
void Game::HandlePlayerEnemyCollision(Actor* pPlayer, Actor* pEnemy) {
    u16 health = GetPlayerHealth();

    // If invulnerable, or dead
    if (pPlayer->state.playerState.damageCounter != 0 || health == 0) {
        return;
    }

    // TODO: Should be determined by enemy stats
    constexpr u16 baseDamage = 10;
    const Damage damage = Game::CalculateDamage(pPlayer, baseDamage);

    //Audio::PlaySFX(&damageSfx, CHAN_ID_PULSE0);

    u32 featherCount = Random::GenerateInt(1, 4);

    health = ActorTakeDamage(pPlayer, damage, health, pPlayer->state.playerState.damageCounter);
    if (health == 0) {
        PlayerMortalHit(pPlayer);
        featherCount = 8;
    }

    SpawnFeathers(pPlayer, featherCount);
    Game::SetPlayerHealth(health);

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
#pragma endregion

constexpr ActorInitFn Game::playerInitTable[PLAYER_TYPE_COUNT] = {
    InitPlayerSidescroller,
    InitPlayerOverworld
};

constexpr ActorUpdateFn Game::playerUpdateTable[PLAYER_TYPE_COUNT] = {
    UpdatePlayerSidescroller,
    UpdatePlayerOverworld
};

constexpr ActorDrawFn Game::playerDrawTable[PLAYER_TYPE_COUNT] = {
    DrawPlayerSidescroller,
    DrawPlayerOverworld
};

#ifdef EDITOR
const ActorEditorData Editor::playerEditorData(
    { "Sidescroller", "Overworld" },
    { {},{} }
);
#endif