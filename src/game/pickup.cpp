#include "pickup.h"
#include "actors.h"
#include "actor_prototype_types.h"
#include "game_rendering.h"
#include "game_state.h"
#include "anim_types.h"
#include "audio.h"

static void OnPickup(Actor* pActor, const PickupData& data) {
    pActor->flags.pendingRemoval = true;
    if (data.pickupSound != SoundHandle::Null()) {
        Audio::PlaySFX(data.pickupSound);
    }
}

static void UpdateExpHalo(Actor* pActor, const ActorPrototype* pPrototype) {
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
        OnPickup(pActor, pPrototype->data.pickupData);

        Game::AddPlayerExp(pActor->state.pickupState.value);

        return;
    }

    // Smoothstep animation when inside specified radius from player
    const Animation* pCurrentAnim = Game::GetActorCurrentAnim(pActor, pPrototype);
    constexpr r32 animRadius = 4.0f;

    pActor->drawState.frameIndex = glm::floor((1.0f - glm::smoothstep(0.0f, animRadius, playerDist)) * pCurrentAnim->frameCount);
    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
}

static void UpdateExpRemnant(Actor* pActor, const ActorPrototype* pPrototype) {
    Actor* pPlayer = Game::GetPlayer();
    if (pPlayer && Game::ActorsColliding(pActor, pPlayer)) {
        OnPickup(pActor, pPrototype->data.pickupData);

        Game::ClearExpRemnant();
        Game::AddPlayerExp(pActor->state.pickupState.value);

        return;
    }
}

static void UpdateHealing(Actor* pActor, const ActorPrototype* pPrototype) {
    Game::ApplyGravity(pActor);
    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        pActor->velocity = glm::reflect(pActor->velocity, hit.impactNormal);
        pActor->velocity *= 0.5f; // Apply damping
    }

    // Reset in air flag
    pActor->flags.inAir = true;

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        pActor->velocity = glm::reflect(pActor->velocity, hit.impactNormal);
        pActor->velocity *= 0.5f; // Apply damping

        if (hit.impactNormal.y < 0.0f) {
            pActor->flags.inAir = false;
        }
    }

    constexpr r32 deceleration = 0.001953125f;
    if (!pActor->flags.inAir && pActor->velocity.x != 0.0f) { // Decelerate
        pActor->velocity.x -= deceleration * glm::sign(pActor->velocity.x);
    }

    Actor* pPlayer = Game::GetPlayer();
    if (pPlayer && Game::ActorsColliding(pActor, pPlayer)) {
        OnPickup(pActor, pPrototype->data.pickupData);

        const u16 newHealth = Game::ActorHeal(pPlayer, pPrototype->data.pickupData.value, Game::GetPlayerHealth(), Game::GetPlayerMaxHealth());
        Game::SetPlayerHealth(newHealth);

        return;
    }
}

static void InitializePickup(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    pActor->drawState.layer = SPRITE_LAYER_FG;
}

constexpr ActorInitFn Game::pickupInitTable[PICKUP_TYPE_COUNT] = {
    InitializePickup,
    InitializePickup,
    InitializePickup,
};

constexpr ActorUpdateFn Game::pickupUpdateTable[PICKUP_TYPE_COUNT] = {
    UpdateExpHalo,
    UpdateExpRemnant,
    UpdateHealing,
};

constexpr ActorDrawFn Game::pickupDrawTable[PICKUP_TYPE_COUNT] = {
    Game::DrawActorDefault,
    Game::DrawActorDefault,
    Game::DrawActorDefault,
};