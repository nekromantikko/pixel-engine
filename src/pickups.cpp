#include "pickups.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"

static void UpdateExpHalo(Actor* pActor) {
    Actor* pPlayer = Game::GetPlayer();

    const glm::vec2 playerVec = pPlayer->position - pActor->position;
    const glm::vec2 playerDir = glm::normalize(playerVec);
    const r32 playerDist = glm::length(playerVec);

    // Wait for a while before homing towards player
    if (!Game::UpdateCounter(pActor->state.pickupState.lingerCounter)) {
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

    if (pPlayer && Game::ActorsColliding(pActor, pPlayer)) {
        //Audio::PlaySFX(&expSfx, CHAN_ID_PULSE0);
        pActor->flags.pendingRemoval = true;

        Game::AddPlayerExp(pActor->state.pickupState.value);

        return;
    }

    // Smoothstep animation when inside specified radius from player
    const Animation& currentAnim = pActor->pPrototype->animations[0];
    constexpr r32 animRadius = 4.0f;

    pActor->drawState.frameIndex = glm::floor((1.0f - glm::smoothstep(0.0f, animRadius, playerDist)) * currentAnim.frameCount);
    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
}

static void UpdateExpRemnant(Actor* pActor) {
    Actor* pPlayer = Game::GetPlayer();
    if (pPlayer && Game::ActorsColliding(pActor, pPlayer)) {
        //Audio::PlaySFX(&expSfx, CHAN_ID_PULSE0);
        pActor->flags.pendingRemoval = true;

        Game::ClearExpRemnant();
        Game::AddPlayerExp(pActor->state.pickupState.value);

        return;
    }
}

#pragma region Public API
void Game::InitializePickup(Actor* pActor, const PersistedActorData& persistData) {
	pActor->drawState.layer = SPRITE_LAYER_FG;

    switch (pActor->pPrototype->subtype) {
    case PICKUP_SUBTYPE_HALO: {
        pActor->pUpdateFn = UpdateExpHalo;
        break;
    }
    case PICKUP_SUBTYPE_XP_REMNANT: {
        pActor->pUpdateFn = UpdateExpRemnant;
        break;
    }
    default:
        pActor->pUpdateFn = nullptr;
        break;
    }
}
#pragma endregion