#include "player.h"
#include "actors.h"
#include "actor_prototype_types.h"
#include "game_rendering.h"
#include "game_input.h"
#include "game_state.h"
#include "dialog.h"
#include "room_types.h"
#include "dungeon_types.h"
#include "overworld_types.h"
#include "anim_types.h"
#include "audio.h"
#include "asset_manager.h"
#include "tilemap.h"

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

// Input mapping
// TODO: Define in settings
u16 jumpButton = BUTTON_A;
u16 interactButton = BUTTON_X;
u16 dodgeButton = BUTTON_B;
u16 switchWeaponButton = BUTTON_SELECT;

// Constants
constexpr r32 maxSpeed = 0.09375f; // Actual movement speed from Zelda 2
constexpr r32 acceleration = maxSpeed / 24.f; // Acceleration from Zelda 2
constexpr u16 wingFrameLength = 12;
constexpr u16 wingFrameLengthFast = 6;
constexpr u16 deathDelay = 240;
constexpr u16 sitDelay = 12;
constexpr u16 staminaRecoveryDelay = 120;
constexpr r32 jumpSpeed = 0.25f;
constexpr r32 dodgeSpeed = maxSpeed * 2;
constexpr u16 dodgeDuration = 16;
constexpr u16 dodgeStaminaCost = 16;

constexpr u16 standAnimIndex = 0;
constexpr u16 sitAnimIndex = 1;
constexpr u16 deathAnimIndex = 2;

// TODO: These should be set in editor somehow
constexpr ChrBankHandle playerBankId(17419163809364512109);
constexpr ActorPrototypeHandle playerGrenadePrototypeId(5433896513301451046);
constexpr ActorPrototypeHandle playerArrowPrototypeId(13929813062463858187);
constexpr ActorPrototypeHandle featherPrototypeId(4150894991750855816);

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
constexpr MetaspriteHandle playerBowFwdMetaspriteId(14903895514375038294);
constexpr MetaspriteHandle playerBowDiagMetaspriteId(13305472537268631013);

constexpr glm::ivec2 playerLauncherOffsets[3] = { { 5, -5 }, { 7, -12 }, { 8, 1 } };
constexpr MetaspriteHandle playerLauncherFwdMetaspriteId(11359313821138079876);
constexpr MetaspriteHandle playerLauncherDiagMetaspriteId(10921545440614489200);

static void AnimateWings(Actor* pPlayer, u16 frameLength) {
    pPlayer->state.playerState.wingCounter = glm::clamp(pPlayer->state.playerState.wingCounter, u16(0), frameLength);

    Game::AdvanceAnimation(pPlayer->state.playerState.wingCounter, pPlayer->state.playerState.wingFrame, PLAYER_WING_FRAME_COUNT, frameLength, 0);
    Game::Rendering::CopyBankTiles(playerBankId, playerWingFrameBankOffsets[pPlayer->state.playerState.wingFrame], 4, playerWingFrameChrOffset, playerWingFrameTileCount);
}

static void AnimateWingsToTargetPosition(Actor* pPlayer, u8 targetFrame, u16 frameLength) {
    if (pPlayer->state.playerState.wingFrame != targetFrame) {
        return AnimateWings(pPlayer, frameLength);
    }
}

static void SetWingFrame(Actor* pPlayer, u16 frame, u16 frameLength) {
    pPlayer->state.playerState.wingCounter = frameLength;
    pPlayer->state.playerState.wingFrame = frame;
    Game::Rendering::CopyBankTiles(playerBankId, playerWingFrameBankOffsets[pPlayer->state.playerState.wingFrame], 4, playerWingFrameChrOffset, playerWingFrameTileCount);
}

static void AnimateSitting(Actor* pPlayer, u16 frameIdx) {
    Game::Rendering::CopyBankTiles(playerBankId, playerSitBankOffsets[frameIdx], 4, playerHeadFrameChrOffset, 8);

    // TODO: Custom wing graphics for sitting?
    AnimateWingsToTargetPosition(pPlayer, PLAYER_WINGS_FLAP_END, wingFrameLengthFast);

    pPlayer->drawState.animIndex = sitAnimIndex;
    pPlayer->drawState.frameIndex = 0;
    pPlayer->drawState.pixelOffset = { 0, 0 };
    pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
    pPlayer->drawState.useCustomPalette = false;
}

static void AnimateDeath(Actor* pPlayer) {
    u8 frameIdx = !pPlayer->flags.inAir;

    Game::Rendering::CopyBankTiles(playerBankId, playerDeadBankOffsets[frameIdx], 4, playerHeadFrameChrOffset, 8);

    pPlayer->drawState.animIndex = deathAnimIndex;
    pPlayer->drawState.frameIndex = frameIdx;
    pPlayer->drawState.pixelOffset = { 0, 0 };
    pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
    pPlayer->drawState.useCustomPalette = false;
}

static void AnimateStanding(Actor* pPlayer, u8 headFrameIndex, u8 legsFrameIndex, s16 vOffset) {
    const u16 aimFrameIndex = pPlayer->state.playerState.flags.aimMode;

    Game::Rendering::CopyBankTiles(playerBankId, playerHeadFrameBankOffsets[aimFrameIndex * playerHeadFrameTileCount + headFrameIndex], 4, playerHeadFrameChrOffset, playerHeadFrameTileCount);
    Game::Rendering::CopyBankTiles(playerBankId, playerLegsFrameBankOffsets[legsFrameIndex], 4, playerLegsFrameChrOffset, playerLegsFrameTileCount);

    pPlayer->drawState.animIndex = 0;
    pPlayer->drawState.frameIndex = aimFrameIndex;
    pPlayer->drawState.pixelOffset = { 0, vOffset };
    pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
    pPlayer->drawState.useCustomPalette = false;
}

static void AnimatePlayer(Actor* pPlayer) {
    const bool jumping = pPlayer->velocity.y < 0;
    const bool descending = !jumping && pPlayer->velocity.y > 0;
    const bool falling = descending && !pPlayer->state.playerState.flags.slowFall;
    const bool moving = pPlayer->velocity.x != 0; // NOTE: Floating point comparison!

    // When jumping or falling, wings get into proper position and stay there for the duration of the jump/fall
    if (jumping || falling) {
        const u16 targetWingFrame = jumping ? PLAYER_WINGS_ASCEND : PLAYER_WINGS_DESCEND;
        AnimateWingsToTargetPosition(pPlayer, targetWingFrame, wingFrameLengthFast);
    }
    else {
        AnimateWings(pPlayer, wingFrameLength);
    }

    u8 headFrameIndex = PLAYER_HEAD_IDLE;
    if (falling) {
        headFrameIndex = PLAYER_HEAD_FALL;
    }
    else if (moving) {
        headFrameIndex = PLAYER_HEAD_FWD;
    }

    u8 legsFrameIndex = PLAYER_LEGS_IDLE;
    if (descending) {
        legsFrameIndex = PLAYER_LEGS_FALL;
    }
    else if (jumping) {
        legsFrameIndex = PLAYER_LEGS_JUMP;
    }
    else if (moving) {
        legsFrameIndex = PLAYER_LEGS_FWD;
    }

    // If grounded, bob up and down based on wing frame
    const s16 vOffset = (pPlayer->flags.inAir || pPlayer->state.playerState.wingFrame <= PLAYER_WINGS_FLAP_START) ? 0 : -1;
    AnimateStanding(pPlayer, headFrameIndex, legsFrameIndex, vOffset);
}


static void HandleLevelExit(const Actor* pPlayer) {
    if (pPlayer == nullptr) {
        return;
    }

    bool shouldExit = false;
    u8 nextDirection = 0;
    glm::i8vec2 nextGridCell = Game::GetDungeonGridCell(pPlayer->position);

    const glm::i8vec2 roomMax = Game::GetCurrentPlayAreaSize();

    if (pPlayer->position.x < 0) {
        shouldExit = true;
        nextDirection = SCREEN_EXIT_DIR_LEFT;
        nextGridCell.x--;
    }
    else if (pPlayer->position.x >= roomMax.x) {
        shouldExit = true;
        nextDirection = SCREEN_EXIT_DIR_RIGHT;
        nextGridCell.x++;
    }
    else if (pPlayer->position.y < 0) {
        shouldExit = true;
        nextDirection = SCREEN_EXIT_DIR_TOP;
        nextGridCell.y--;
    }
    else if (pPlayer->position.y >= roomMax.y) {
        shouldExit = true;
        nextDirection = SCREEN_EXIT_DIR_BOTTOM;
        nextGridCell.y++;
    }

    if (shouldExit) {
        Game::TriggerLevelTransition(Game::GetCurrentDungeon(), nextGridCell, nextDirection);
    }
}

static void HandleScreenDiscovery(const Actor* pPlayer) {
    static glm::i8vec2 previousGridCell = { -1, -1 };

    const glm::i8vec2 gridCell = Game::GetDungeonGridCell(pPlayer->position);
    if (gridCell != previousGridCell) {
        Game::DiscoverScreen(gridCell);
        previousGridCell = gridCell;
    }
}

static void PlayerDie(Actor* pPlayer) {

    Game::SetExpRemnant(pPlayer->position, Game::GetPlayerExp());
    Game::SetPlayerExp(0);

    // Transition to checkpoint
    const Checkpoint checkpoint = Game::GetCheckpoint();
    Game::TriggerLevelTransition(checkpoint.dungeonId, checkpoint.gridOffset, SCREEN_EXIT_DIR_DEATH_WARP);
}

static void SpawnFeathers(Actor* pPlayer, u32 count) {
    for (u32 i = 0; i < count; i++) {
        const glm::vec2 spawnOffset = {
            Random::GenerateReal(-1.0f, 1.0f),
            Random::GenerateReal(-1.0f, 1.0f)
        };

        const glm::vec2 velocity = Random::GenerateDirection() * 0.0625f;
        Actor* pSpawned = Game::SpawnActor(featherPrototypeId, pPlayer->position + spawnOffset, velocity);
        const ActorPrototype* pSpawnedPrototype = Game::GetActorPrototype(pSpawned);
        const Animation* pSpawnedCurrentAnim = Game::GetActorCurrentAnim(pSpawned, pSpawnedPrototype);
        if (pSpawnedCurrentAnim) {
            pSpawned->drawState.frameIndex = Random::GenerateInt(0, pSpawnedCurrentAnim->frameCount - 1);
        }
    }
}

static void PlayerMortalHit(Actor* pPlayer) {
    Game::TriggerScreenShake(2, 30, true);
    pPlayer->velocity.y = -0.25f;
    pPlayer->state.playerState.flags.mode = PLAYER_MODE_DYING;
    pPlayer->state.playerState.modeTransitionCounter = deathDelay;
}

static void PlayerSitDown(Actor* pPlayer) {
    pPlayer->state.playerState.flags.mode = PLAYER_MODE_STAND_TO_SIT;
    pPlayer->state.playerState.modeTransitionCounter = sitDelay;
    pPlayer->velocity.x = 0.0f;
}

static void PlayerStandUp(Actor* pPlayer) {
    pPlayer->state.playerState.flags.mode = PLAYER_MODE_SIT_TO_STAND;
    pPlayer->state.playerState.modeTransitionCounter = sitDelay;
}

static void TriggerInteraction(Actor* pPlayer, Actor* pInteractable) {
    if (pInteractable == nullptr) {
        return;
    }

    const ActorPrototype* pInteractablePrototype = Game::GetActorPrototype(pInteractable);
    if (pInteractablePrototype->subtype == INTERACTABLE_TYPE_CHECKPOINT) {
        pInteractable->state.checkpointState.activated = true;

        Game::ActivateCheckpoint(pInteractable);

        // Sit down
        PlayerSitDown(pPlayer);
        Game::SetPlayerHealth(Game::GetPlayerMaxHealth());

        // Add dialogue
        /*static constexpr const char* lines[] = {
            "I just put a regular dialogue box here, but it would\nnormally be a level up menu.",
        };

        if (!Game::IsDialogActive()) {
            Game::OpenDialog(lines, 1);
        }*/
    }
}

static void PlayerShoot(Actor* pPlayer, const PlayerData& data) {
    constexpr s32 shootDelay = 10;

    PlayerState& playerState = pPlayer->state.playerState;
    Game::UpdateCounter(playerState.shootCounter);

    if (Game::Input::ButtonDown(interactButton) && playerState.shootCounter == 0) {
        playerState.shootCounter = shootDelay;

        const u16 playerWeapon = Game::GetPlayerWeapon();
        const ActorPrototypeHandle prototypeHandle = playerWeapon == PLAYER_WEAPON_LAUNCHER ? playerGrenadePrototypeId : playerArrowPrototypeId;
        Actor* pBullet = Game::SpawnActor(prototypeHandle, pPlayer->position);
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
            if (data.gunSound != SoundHandle::Null()) {
                Audio::PlaySFX(data.gunSound);
            }
        }

    }
}

static void PlayerDecelerate(Actor* pPlayer) {
    if (!pPlayer->flags.inAir && pPlayer->velocity.x != 0.0f) {
        pPlayer->velocity.x -= acceleration * glm::sign(pPlayer->velocity.x);
    }
}

static bool PlayerJump(Actor* pPlayer, const PlayerData& data) {
    if (Game::Input::ButtonPressed(jumpButton) && (!pPlayer->flags.inAir || !pPlayer->state.playerState.flags.doubleJumped)) {
        pPlayer->velocity.y = -0.25f;

        // Trigger new flap by taking wings out of the jumping position by advancing the frame index
        if (pPlayer->state.playerState.wingFrame == PLAYER_WINGS_ASCEND) {
            SetWingFrame(pPlayer, PLAYER_WINGS_FLAP_END, wingFrameLengthFast);
        }
        if (data.jumpSound != SoundHandle::Null()) {
            Audio::PlaySFX(data.jumpSound);
        }

        if (pPlayer->flags.inAir) {
            pPlayer->state.playerState.flags.doubleJumped = true;
        }

        return true;
    }

    return false;
}

static bool PlayerDodge(Actor* pPlayer) {
    if (Game::Input::ButtonPressed(dodgeButton) && Game::GetPlayerStamina() > 0 && (!pPlayer->flags.inAir || !pPlayer->state.playerState.flags.airDodged)) {
        Game::AddPlayerStamina(-dodgeStaminaCost);
        pPlayer->state.playerState.staminaRecoveryCounter = staminaRecoveryDelay;
        pPlayer->state.playerState.modeTransitionCounter = dodgeDuration;

        pPlayer->velocity.x += (pPlayer->flags.facingDir == ACTOR_FACING_LEFT) ? -dodgeSpeed : dodgeSpeed;
        pPlayer->velocity.y = 0.0f;

        if (pPlayer->flags.inAir) {
            pPlayer->state.playerState.flags.airDodged = true;
        }

        return true;
    }

    return false;
}

static void PlayerInput(Actor* pPlayer, const PlayerData& data) {
    PlayerState& playerState = pPlayer->state.playerState;
    if (Game::Input::ButtonDown(BUTTON_DPAD_LEFT)) {
        pPlayer->velocity.x -= acceleration;
        if (pPlayer->flags.facingDir != ACTOR_FACING_LEFT) {
            pPlayer->velocity.x -= acceleration;
        }

        pPlayer->velocity.x = glm::clamp(pPlayer->velocity.x, -maxSpeed, maxSpeed);
        pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
    }
    else if (Game::Input::ButtonDown(BUTTON_DPAD_RIGHT)) {
        pPlayer->velocity.x += acceleration;
        if (pPlayer->flags.facingDir != ACTOR_FACING_RIGHT) {
            pPlayer->velocity.x += acceleration;
        }

        pPlayer->velocity.x = glm::clamp(pPlayer->velocity.x, -maxSpeed, maxSpeed);
        pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
    }
    else {
        PlayerDecelerate(pPlayer);
    }

    // Dodge
    if (PlayerDodge(pPlayer)) {
        pPlayer->state.playerState.flags.mode = PLAYER_MODE_DODGE;
        return;
    }

    // Interaction / Shooting
    Actor* pInteractable = Game::GetFirstActorCollision(pPlayer, ACTOR_TYPE_INTERACTABLE);
    if (pInteractable && !pPlayer->flags.inAir && Game::Input::ButtonPressed(interactButton)) {
        TriggerInteraction(pPlayer, pInteractable);
    }
    else PlayerShoot(pPlayer, data);

    // Aim mode
    if (Game::Input::ButtonDown(BUTTON_DPAD_UP)) {
        playerState.flags.aimMode = PLAYER_AIM_UP;
    }
    else if (Game::Input::ButtonDown(BUTTON_DPAD_DOWN)) {
        playerState.flags.aimMode = PLAYER_AIM_DOWN;
    }
    else playerState.flags.aimMode = PLAYER_AIM_FWD;

    PlayerJump(pPlayer, data);

    if (pPlayer->velocity.y < 0 && Game::Input::ButtonReleased(jumpButton)) {
        pPlayer->velocity.y *= 0.5f;
    }

    if (Game::Input::ButtonDown(jumpButton) && pPlayer->velocity.y > 0.0f) {
        const u16 currentStamina = Game::GetPlayerStamina();
        if (currentStamina > 0) {
            Game::AddPlayerStamina(-1);
            pPlayer->state.playerState.staminaRecoveryCounter = staminaRecoveryDelay;
            playerState.flags.slowFall = true;
        }
    }

    if (Game::Input::ButtonReleased(interactButton)) {
        playerState.shootCounter = 0.0f;
    }

    if (Game::Input::ButtonPressed(switchWeaponButton)) {
        u16 playerWeapon = Game::GetPlayerWeapon();
        if (playerWeapon == PLAYER_WEAPON_LAUNCHER) {
            Game::SetPlayerWeapon(PLAYER_WEAPON_BOW);
        }
        else Game::SetPlayerWeapon(PLAYER_WEAPON_LAUNCHER);
    }
}

static void TriggerModeTransition(Actor* pActor) {
    PlayerState& state = pActor->state.playerState;

    switch (state.flags.mode) {
    case PLAYER_MODE_STAND_TO_SIT: {
        state.flags.mode = PLAYER_MODE_SITTING;
        break;
    }
    case PLAYER_MODE_SIT_TO_STAND: {
        state.flags.mode = PLAYER_MODE_NORMAL;
        break;
    }
    case PLAYER_MODE_DAMAGED: {
        state.flags.mode = PLAYER_MODE_NORMAL;
        break;
    }
    case PLAYER_MODE_DYING: {
        PlayerDie(pActor);
        break;
    }
    case PLAYER_MODE_ENTERING: {
        state.flags.mode = PLAYER_MODE_NORMAL;
        break;
    }
    case PLAYER_MODE_DODGE: {
        state.flags.mode = PLAYER_MODE_NORMAL;
        pActor->velocity.x = glm::clamp(pActor->velocity.x, -maxSpeed, maxSpeed);
        break;
    }
    default:
        break;
    }
}

static void UpdateSidescrollerMode(Actor* pActor, const PlayerData& data) {
    PlayerState& state = pActor->state.playerState;

    if (state.modeTransitionCounter != 0 && !Game::UpdateCounter(state.modeTransitionCounter)) {
        TriggerModeTransition(pActor);
    }

    const bool inputDisabled = Game::IsDialogActive();

    switch (state.flags.mode) {
    case PLAYER_MODE_NORMAL: {
        if (!inputDisabled) {
            PlayerInput(pActor, data);
        }
        else {
            PlayerDecelerate(pActor);
        }

        AnimatePlayer(pActor);

        break;
    }
    case PLAYER_MODE_STAND_TO_SIT: {
        const r32 animProgress = r32(state.modeTransitionCounter) / sitDelay;
        const u8 frameIdx = glm::mix(0, 1, animProgress);

        AnimateSitting(pActor, frameIdx);
        break;
    }
    case PLAYER_MODE_SITTING: {
        if (!inputDisabled && Game::Input::AnyButtonPressed()) {
            PlayerStandUp(pActor);
        }

        AnimateSitting(pActor, 1);
        break;
    }
    case PLAYER_MODE_SIT_TO_STAND: {
        const r32 animProgress = r32(state.modeTransitionCounter) / sitDelay;
        const u8 frameIdx = glm::mix(1, 0, animProgress);
        AnimateSitting(pActor, frameIdx);
        break;
    }
    case PLAYER_MODE_DAMAGED: {
        PlayerDecelerate(pActor);
        AnimateStanding(pActor, PLAYER_HEAD_DMG, PLAYER_LEGS_FALL, 0);
        Game::SetDamagePaletteOverride(pActor, state.modeTransitionCounter);
        break;
    }
    case PLAYER_MODE_DYING: {
        PlayerDecelerate(pActor);
        AnimateDeath(pActor);
        break;
    }
    case PLAYER_MODE_ENTERING: {
        AnimatePlayer(pActor);
        break;
    }
    case PLAYER_MODE_DODGE: {
        // Dash cancel
        if (PlayerJump(pActor, data)) {
            state.flags.mode = PLAYER_MODE_NORMAL;
            state.modeTransitionCounter = 0;
            pActor->velocity.x = glm::clamp(pActor->velocity.x, -maxSpeed, maxSpeed);
        }

        break;
    }
    default:
        break;
    }
}

static void UpdatePlayerSidescroller(Actor* pActor, const ActorPrototype* pPrototype) {
    // Reset slow fall
    pActor->state.playerState.flags.slowFall = false;

    if (!Game::UpdateCounter(pActor->state.playerState.staminaRecoveryCounter)) {
        Game::AddPlayerStamina(1);
    }

    UpdateSidescrollerMode(pActor, pPrototype->data.playerData);

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        pActor->velocity.x = 0.0f;
    }

    constexpr r32 playerGravity = 0.01f;
    constexpr r32 playerSlowGravity = playerGravity / 4;

    if (pActor->state.playerState.flags.mode != PLAYER_MODE_DODGE) {
        const r32 gravity = pActor->state.playerState.flags.slowFall ? playerSlowGravity : playerGravity;
        Game::ApplyGravity(pActor, gravity);
    }

    // Reset in air flag
    pActor->flags.inAir = true;

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        pActor->velocity.y = 0.0f;

        if (hit.impactNormal.y < 0.0f) {
            pActor->flags.inAir = false;
            pActor->state.playerState.flags.doubleJumped = false;
            pActor->state.playerState.flags.airDodged = false;
        }
    }

    if (pActor->state.playerState.flags.mode != PLAYER_MODE_DYING) {
        HandleScreenDiscovery(pActor);
        HandleLevelExit(pActor);
    }
}

static glm::ivec2 GetOverworldInputDir() {
    glm::ivec2 result(0);

    const u8 inputDir = (Game::Input::GetCurrentState() >> 8) & 0xf;
    if (!inputDir) {
        return result;
    }

    const u8 horizontalDir = inputDir & 3;
    const u8 verticalDir = (inputDir >> 2) & 3;

    if (horizontalDir && horizontalDir != 3) {
        const bool signBit = (horizontalDir >> 1) & 1;
        result.x = signBit ? -1 : 1;
    }
    else if (verticalDir && verticalDir != 3) {
        const bool signBit = (verticalDir >> 1) & 1;
        result.y = signBit ? -1 : 1;
    }

    return result;
}

static void UpdatePlayerOverworld(Actor* pPlayer, const ActorPrototype* pPrototype) {
    static constexpr u16 movementRate = 1;
    static constexpr u16 movementSteps = METATILE_DIM_PIXELS * movementRate;
    static constexpr r32 movementStepLength = 1.0f / movementSteps;

    PlayerOverworldState& state = pPlayer->state.playerOverworld;
    
    if (state.movementCounter != 0) {
        pPlayer->position += pPlayer->velocity;

        if (!Game::UpdateCounter(state.movementCounter)) {
            const Overworld* pOverworld = Game::GetOverworld();
            for (u32 i = 0; i < MAX_OVERWORLD_KEY_AREA_COUNT; i++) {
                const OverworldKeyArea& area = pOverworld->keyAreas[i];

                if (area.position != glm::i8vec2(pPlayer->position)) {
                    continue;
                }

                Game::EnterOverworldArea(i, state.facingDir);
            }
        }
    }
    else {
        pPlayer->position = glm::roundEven(pPlayer->position);

        const glm::ivec2 inputDir = GetOverworldInputDir();
        if (inputDir.x != 0 || inputDir.y != 0) {
            state.facingDir = inputDir;
            const glm::ivec2 targetPos = glm::ivec2(pPlayer->position) + state.facingDir;
            pPlayer->velocity = glm::vec2(state.facingDir) * movementStepLength;

			const Tilemap* pTilemap = Game::GetCurrentTilemap();
			const Tileset* pTileset = AssetManager::GetAsset(pTilemap->tilesetHandle);

			if (!pTilemap || !pTileset) {
				return;
			}

			const s32 targetTileIndex = Tiles::GetTilesetTileIndex(pTilemap, targetPos);
			const TilesetTile* pNextTile = targetTileIndex >= 0 ? &pTileset->tiles[targetTileIndex] : nullptr;

            if (!pNextTile || pNextTile->type != TILE_SOLID) {
                state.movementCounter = movementSteps;
            }
        }
    }

    if (state.facingDir.x) {
        pPlayer->drawState.animIndex = state.facingDir.x > 0 ? 0 : 1;
    }
    else {
        pPlayer->drawState.animIndex = state.facingDir.y > 0 ? 2 : 3;
    }

    if ((state.movementCounter & 7) == 7) {
        pPlayer->drawState.frameIndex++;
        pPlayer->drawState.frameIndex &= 1;
    }
}

static bool DrawPlayerGun(const Actor* pPlayer) {
    const ActorDrawState& drawState = pPlayer->drawState;
    glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels(pPlayer->position) + drawState.pixelOffset;

    // Draw weapon first
    glm::i16vec2 weaponOffset{};
    u8 weaponFrameBankOffset;
    MetaspriteHandle weaponMetaspriteId;
    const u16 playerWeapon = Game::GetPlayerWeapon();
    switch (playerWeapon) {
    case PLAYER_WEAPON_BOW: {
        weaponOffset = playerBowOffsets[pPlayer->state.playerState.flags.aimMode];
        weaponFrameBankOffset = playerBowFrameBankOffsets[pPlayer->state.playerState.flags.aimMode];
        weaponMetaspriteId = pPlayer->state.playerState.flags.aimMode == PLAYER_AIM_FWD ? playerBowFwdMetaspriteId : playerBowDiagMetaspriteId;
        break;
    }
    case PLAYER_WEAPON_LAUNCHER: {
        weaponOffset = playerLauncherOffsets[pPlayer->state.playerState.flags.aimMode];
        weaponFrameBankOffset = playerLauncherFrameBankOffsets[pPlayer->state.playerState.flags.aimMode];
        weaponMetaspriteId = pPlayer->state.playerState.flags.aimMode == PLAYER_AIM_FWD ? playerLauncherFwdMetaspriteId : playerLauncherDiagMetaspriteId;
        break;
    }
    default:
        break;
    }
    weaponOffset.x *= pPlayer->flags.facingDir;

    Game::Rendering::CopyBankTiles(playerBankId, weaponFrameBankOffset, 4, playerWeaponFrameChrOffset, playerWeaponFrameTileCount);
    return Game::Rendering::DrawMetasprite(SPRITE_LAYER_FG, weaponMetaspriteId, drawPos + weaponOffset, drawState.hFlip, drawState.vFlip, -1);
}

static bool DrawPlayerSidescroller(const Actor* pPlayer, const ActorPrototype* pPrototype) {
    if (pPlayer->state.playerState.flags.mode == PLAYER_MODE_NORMAL || 
        pPlayer->state.playerState.flags.mode == PLAYER_MODE_DAMAGED ||
        pPlayer->state.playerState.flags.mode == PLAYER_MODE_ENTERING) {
        DrawPlayerGun(pPlayer);
    }
    return Game::DrawActorDefault(pPlayer, pPrototype);
}

static bool DrawPlayerOverworld(const Actor* pPlayer, const ActorPrototype* pPrototype) {
    return Game::DrawActorDefault(pPlayer, pPrototype);
}

static void InitPlayerSidescroller(Actor* pPlayer, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    pPlayer->state.playerState.modeTransitionCounter = 0;
    pPlayer->state.playerState.staminaRecoveryCounter = 0;

    pPlayer->state.playerState.flags.aimMode = PLAYER_AIM_FWD;
    pPlayer->state.playerState.flags.doubleJumped = false;
    pPlayer->state.playerState.flags.airDodged = false;
    pPlayer->state.playerState.flags.mode = PLAYER_MODE_NORMAL;
    pPlayer->state.playerState.flags.slowFall = false;
    pPlayer->drawState.layer = SPRITE_LAYER_FG;
}

static void InitPlayerOverworld(Actor* pPlayer, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    pPlayer->state.playerOverworld.movementCounter = 0;
    pPlayer->state.playerOverworld.facingDir = { 0, 1 };
    pPlayer->position = glm::roundEven(pPlayer->position);
}

#pragma region Public API
bool Game::PlayerInvulnerable(Actor* pPlayer) {
    return (pPlayer->state.playerState.flags.mode == PLAYER_MODE_DAMAGED ||
        pPlayer->state.playerState.flags.mode == PLAYER_MODE_DYING ||
        pPlayer->state.playerState.flags.mode == PLAYER_MODE_DODGE);
}

void Game::PlayerTakeDamage(Actor* pPlayer, const Damage& damage, const glm::vec2& enemyPos) {
    u16 health = GetPlayerHealth();

    ActorPrototype* pPrototype = AssetManager::GetAsset(pPlayer->prototypeHandle);
    if (pPrototype && pPrototype->data.playerData.damageSound != SoundHandle::Null()) {
        Audio::PlaySFX(pPrototype->data.playerData.damageSound);
    }

    u32 featherCount = Random::GenerateInt(1, 4);

    health = ActorTakeDamage(pPlayer, damage, health, pPlayer->state.playerState.modeTransitionCounter);
    if (health == 0) {
        PlayerMortalHit(pPlayer);
        featherCount = 8;
    }
    else {
        pPlayer->state.playerState.flags.mode = PLAYER_MODE_DAMAGED;
    }

    SpawnFeathers(pPlayer, featherCount);
    Game::SetPlayerHealth(health);

    // Recoil
    constexpr r32 recoilSpeed = 0.046875f; // Recoil speed from Zelda 2
    if (enemyPos.x > pPlayer->position.x) {
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