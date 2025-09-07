#include "actors.h"
#include "game_rendering.h"
#include "game_state.h"
#include "audio.h"

static void OnPickup(Actor* pActor) {
    pActor->flags.pendingRemoval = true;
    if (pActor->data.pickup.pickupSound != SoundHandle::Null()) {
        Audio::PlaySFX(pActor->data.pickup.pickupSound, 0);
    }
}

static void UpdateExpHalo(Actor* pActor) {
    Actor* pPlayer = Game::GetPlayer();

    const glm::vec2 playerVec = pPlayer->position - pActor->position;
    const glm::vec2 playerDir = glm::normalize(playerVec);
    const r32 playerDist = glm::length(playerVec);

    // Wait for a while before homing towards player
    if (!Game::UpdateCounter(pActor->data.pickup.lingerCounter)) {
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
        OnPickup(pActor);

        Game::AddPlayerExp(pActor->data.pickup.value);

        return;
    }

    // Smoothstep animation when inside specified radius from player
    const Animation* pCurrentAnim = Game::GetActorCurrentAnim(pActor);
    constexpr r32 animRadius = 4.0f;

    pActor->drawState.frameIndex = glm::floor((1.0f - glm::smoothstep(0.0f, animRadius, playerDist)) * pCurrentAnim->frameCount);
    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
}

static void UpdateExpRemnant(Actor* pActor) {
    Actor* pPlayer = Game::GetPlayer();
    if (pPlayer && Game::ActorsColliding(pActor, pPlayer)) {
        OnPickup(pActor);

        Game::ClearExpRemnant();
        Game::AddPlayerExp(pActor->data.pickup.value);

        return;
    }
}

static void UpdateHealing(Actor* pActor) {
    Game::ApplyGravity(pActor);
    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        pActor->velocity = glm::reflect(pActor->velocity, hit.impactNormal);
        pActor->velocity *= 0.5f; // Apply damping
    }

    // Reset in air flag
    pActor->flags.inAir = true;

    if (Game::ActorMoveVertical(pActor, hit)) {
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
        OnPickup(pActor);

        const u16 newHealth = Game::ActorHeal(pPlayer, pActor->data.pickup.value, Game::GetPlayerHealth(), Game::GetPlayerMaxHealth());
        Game::SetPlayerHealth(newHealth);

        return;
    }
}

static void InitializePickup(Actor* pActor, const PersistedActorData* pPersistData) {
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