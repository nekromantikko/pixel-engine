#include "actors.h"
#include "game_rendering.h"
#include "game_input.h"
#include "game_state.h"
#include "dialog.h"
#include "audio.h"
#include "asset_manager.h"
#include "tilemap.h"

enum PlayerAnimation : u8 {
    PLAYER_ANIM_IDLE = 0,
    PLAYER_ANIM_FLY,
    PLAYER_ANIM_JUMP,
    PLAYER_ANIM_FALL,
    PLAYER_ANIM_DAMAGED,
    PLAYER_ANIM_SIT_DOWN,
	PLAYER_ANIM_STAND_UP,
    PLAYER_ANIM_DEATH
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

// TODO: These should be set in editor somehow
constexpr ChrBankHandle playerBankId(17419163809364512109);
constexpr ActorPrototypeHandle playerGrenadePrototypeId(5433896513301451046);
constexpr ActorPrototypeHandle playerArrowPrototypeId(13929813062463858187);
constexpr ActorPrototypeHandle featherPrototypeId(4150894991750855816);

constexpr AnimationHandle playerBowAnimationId(18043323496187879979);
constexpr AnimationHandle playerLauncherAnimationId(2436774277981009799);

static void AnimateWings(Actor* pPlayer, u16 frameLength) {
    pPlayer->state.playerState.wingCounter = glm::clamp(pPlayer->state.playerState.wingCounter, u16(0), frameLength);

    Game::AdvanceAnimation(pPlayer->state.playerState.wingCounter, pPlayer->state.playerState.wingFrame, PLAYER_WING_FRAME_COUNT, frameLength, 0);
}

static void AnimateWingsToTargetPosition(Actor* pPlayer, u8 targetFrame, u16 frameLength) {
    if (pPlayer->state.playerState.wingFrame != targetFrame) {
        return AnimateWings(pPlayer, frameLength);
    }
}

static void SetWingFrame(Actor* pPlayer, u16 frame, u16 frameLength) {
    pPlayer->state.playerState.wingCounter = frameLength;
    pPlayer->state.playerState.wingFrame = frame;
}

static bool DrawWings(const Actor* pPlayer, const ActorPrototype* pPrototype, s16 vOffset = 0) {
	Animation* pWingAnim = pPrototype->data.playerData.wingAnimation == AnimationHandle::Null() ? nullptr : AssetManager::GetAsset(pPrototype->data.playerData.wingAnimation);
    if (!pWingAnim) {
        return false;
    }

    const u8 wingFrame = pPlayer->state.playerState.wingFrame;
    MetaspriteHandle metaspriteHandle = pWingAnim->GetFrames()[wingFrame].metaspriteId;
    glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels(pPlayer->position) + pPlayer->drawState.pixelOffset;
	drawPos.y += vOffset;
	s32 paletteOverride = pPlayer->drawState.useCustomPalette ? pPlayer->drawState.palette : -1;
    return Game::Rendering::DrawMetasprite(SPRITE_LAYER_FG, metaspriteHandle, drawPos, pPlayer->drawState.hFlip, pPlayer->drawState.vFlip, paletteOverride);
}

static void AnimateDeath(Actor* pPlayer) {
    u8 frameIdx = !pPlayer->flags.inAir;

    pPlayer->drawState.animIndex = PLAYER_ANIM_DEATH;
    pPlayer->drawState.frameIndex = frameIdx;
    pPlayer->drawState.pixelOffset = { 0, 0 };
    pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
    pPlayer->drawState.useCustomPalette = false;
}

static void AnimateStanding(Actor* pPlayer, u8 animIndex, s16 vOffset) {
    const u16 aimFrameIndex = pPlayer->state.playerState.flags.aimMode;

    pPlayer->drawState.animIndex = animIndex;
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

	u8 animIndex = PLAYER_ANIM_IDLE;
    if (descending) {
		animIndex = PLAYER_ANIM_FALL;
    }
    else if (jumping) {
		animIndex = PLAYER_ANIM_JUMP;
    }
    else if (moving) {
		animIndex = PLAYER_ANIM_FLY;
    }

    // If grounded, bob up and down based on wing frame
    const s16 vOffset = (pPlayer->flags.inAir || pPlayer->state.playerState.wingFrame <= PLAYER_WINGS_FLAP_START) ? 0 : -1;
    AnimateStanding(pPlayer, animIndex, vOffset);
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

    pPlayer->drawState.animIndex = PLAYER_ANIM_SIT_DOWN;
    pPlayer->drawState.frameIndex = 0;
    pPlayer->drawState.pixelOffset = { 0, 0 };

    pPlayer->velocity.x = 0.0f;
}

static void PlayerStandUp(Actor* pPlayer) {
    pPlayer->state.playerState.flags.mode = PLAYER_MODE_SIT_TO_STAND;
    pPlayer->state.playerState.modeTransitionCounter = sitDelay;

    pPlayer->drawState.animIndex = PLAYER_ANIM_STAND_UP;
    pPlayer->drawState.frameIndex = 0;
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
                Audio::PlaySFX(data.gunSound, 1);
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
		pPlayer->drawState.animIndex = PLAYER_ANIM_FLY;
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

		// Need to check if mode didn't change in PlayerInput (This is a bit stupid)
        if (state.flags.mode == PLAYER_MODE_NORMAL) {
            AnimatePlayer(pActor);
        }

        break;
    }
    case PLAYER_MODE_STAND_TO_SIT: {
        AnimateWingsToTargetPosition(pActor, PLAYER_WINGS_FLAP_END, wingFrameLengthFast);
        ActorPrototype* pPrototype = AssetManager::GetAsset(pActor->prototypeHandle);
        Game::AdvanceCurrentAnimation(pActor, pPrototype);
        break;
    }
    case PLAYER_MODE_SITTING: {
        if (!inputDisabled && Game::Input::AnyButtonPressed()) {
            PlayerStandUp(pActor);
        }

        AnimateWingsToTargetPosition(pActor, PLAYER_WINGS_FLAP_END, wingFrameLengthFast);
        ActorPrototype* pPrototype = AssetManager::GetAsset(pActor->prototypeHandle);
        Game::AdvanceCurrentAnimation(pActor, pPrototype);
        break;
    }
    case PLAYER_MODE_SIT_TO_STAND: {
        AnimateWingsToTargetPosition(pActor, PLAYER_WINGS_FLAP_END, wingFrameLengthFast);
        ActorPrototype* pPrototype = AssetManager::GetAsset(pActor->prototypeHandle);
        Game::AdvanceCurrentAnimation(pActor, pPrototype);
        break;
    }
    case PLAYER_MODE_DAMAGED: {
        PlayerDecelerate(pActor);
        AnimateStanding(pActor, PLAYER_ANIM_DAMAGED, 0);
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

    AnimationHandle weaponAnimId = AnimationHandle::Null();
    const u16 playerWeapon = Game::GetPlayerWeapon();
    switch (playerWeapon) {
    case PLAYER_WEAPON_BOW: {
		weaponAnimId = playerBowAnimationId;
        break;
    }
    case PLAYER_WEAPON_LAUNCHER: {
		weaponAnimId = playerLauncherAnimationId;
        break;
    }
    default:
        return false;
    }
    
    Animation* pAnim = AssetManager::GetAsset(weaponAnimId);
	if (!pAnim) {
		return false;
	}
	MetaspriteHandle weaponMetaspriteId = pAnim->GetFrames()[pPlayer->state.playerState.flags.aimMode].metaspriteId;
    return Game::Rendering::DrawMetasprite(SPRITE_LAYER_FG, weaponMetaspriteId, drawPos, drawState.hFlip, drawState.vFlip, -1);
}

static s16 GetWingsVerticalOffset(const Actor* pPlayer, const ActorPrototype* pPrototype) {
    s16 vOffset = 0;
    if (pPlayer->state.playerState.flags.mode == PLAYER_MODE_SITTING ||
        pPlayer->state.playerState.flags.mode == PLAYER_MODE_STAND_TO_SIT ||
        pPlayer->state.playerState.flags.mode == PLAYER_MODE_SIT_TO_STAND) {
		AnimationHandle animHandle = pPrototype->GetAnimations()[pPlayer->drawState.animIndex];
        const Animation* pAnim = AssetManager::GetAsset(animHandle);
        if (pAnim) {
			r32 animProgress = pPlayer->drawState.frameIndex / (r32)pAnim->frameCount;
            if (pPlayer->state.playerState.flags.mode == PLAYER_MODE_SIT_TO_STAND) {
                animProgress = 1.0f - animProgress;
            }
			vOffset = glm::roundEven(animProgress * 16.0f); // Sit down offset
        }
    }
    return vOffset;
}

static bool DrawPlayerSidescroller(const Actor* pPlayer, const ActorPrototype* pPrototype) {
    bool result = true;
    if (pPlayer->state.playerState.flags.mode == PLAYER_MODE_NORMAL || 
        pPlayer->state.playerState.flags.mode == PLAYER_MODE_DAMAGED ||
        pPlayer->state.playerState.flags.mode == PLAYER_MODE_ENTERING) {
        result &= DrawPlayerGun(pPlayer);
    }
    result &= Game::DrawActorDefault(pPlayer, pPrototype);
    if (pPlayer->state.playerState.flags.mode != PLAYER_MODE_DYING) {
        s16 vOffset = GetWingsVerticalOffset(pPlayer, pPrototype);
        result &= DrawWings(pPlayer, pPrototype, vOffset);
    }
    return result;
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
void Game::PlayerRespawnAtCheckpoint(Actor* pPlayer) {
	PlayerSitDown(pPlayer);
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