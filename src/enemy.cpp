#include "enemy.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

// TODO: Should be determined by enemy stats
constexpr u16 baseDamage = 10;

static void UpdateSlimeEnemy(Actor* pActor, const ActorPrototype* pPrototype) {
    Game::UpdateCounter(pActor->state.enemyState.damageCounter);

    if (!pActor->flags.inAir) {
        const bool shouldJump = Random::GenerateInt(0, 127) == 0;
        if (shouldJump) {
            pActor->velocity.y = -0.25f;
            Game::ActorFacePlayer(pActor);
            pActor->velocity.x = 0.15625f * pActor->flags.facingDir;
        }
        else {
            pActor->velocity.x = 0.00625f * pActor->flags.facingDir;
        }
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        pActor->velocity.x = 0.0f;
        pActor->flags.facingDir = (s8)hit.impactNormal.x;
    }

    Game::ApplyGravity(pActor);

    // Reset in air flag
    pActor->flags.inAir = true;

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        pActor->velocity.y = 0.0f;

        if (hit.impactNormal.y < 0.0f) {
            pActor->flags.inAir = false;
        }
    }

    Actor* pPlayer = Game::GetPlayer();
    const Damage damage = Game::CalculateDamage(pPlayer, baseDamage);
    if (pPlayer && !Game::PlayerInvulnerable(pPlayer) && Game::ActorsColliding(pActor, pPlayer)) {
        Game::PlayerTakeDamage(pPlayer, damage, pActor->position);
    }

    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    Game::SetDamagePaletteOverride(pActor, pActor->state.enemyState.damageCounter);
}

static void UpdateSkullEnemy(Actor* pActor, const ActorPrototype* pPrototype) {
    Game::UpdateCounter(pActor->state.enemyState.damageCounter);

    Game::ActorFacePlayer(pActor);

    static const r32 amplitude = 4.0f;
    const r32 sineTime = glm::sin(Game::GetFramesElapsed() / 60.f);
    pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

    // Shoot fireballs
    const bool shouldFire = Random::GenerateInt(0, 127) == 0;
    if (shouldFire) {

        Actor* pPlayer = Game::GetPlayer();
        if (pPlayer != nullptr) {
            const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
            const glm::vec2 velocity = playerDir * 0.0625f;

            Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, velocity);
        }
    }


    Actor* pPlayer = Game::GetPlayer();
    const Damage damage = Game::CalculateDamage(pPlayer, baseDamage);
    if (pPlayer && !Game::PlayerInvulnerable(pPlayer) && Game::ActorsColliding(pActor, pPlayer)) {
        Game::PlayerTakeDamage(pPlayer, damage, pActor->position);
    }

    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    Game::SetDamagePaletteOverride(pActor, pActor->state.enemyState.damageCounter);
}

static void FireballDie(Actor* pActor, const ActorPrototype* pPrototype, const glm::vec2& effectPos) {
    pActor->flags.pendingRemoval = true;
    Game::SpawnActor(pPrototype->data.fireballData.deathEffect, effectPos);
}

static void UpdateFireball(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.fireballState.lifetimeCounter)) {
        return FireballDie(pActor, pPrototype, pActor->position);
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        return FireballDie(pActor, pPrototype, hit.impactPoint);

    }

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        return FireballDie(pActor, pPrototype, hit.impactPoint);
    }

    Actor* pPlayer = Game::GetPlayer();
    const Damage damage = Game::CalculateDamage(pPlayer, baseDamage);
    if (pPlayer && !Game::PlayerInvulnerable(pPlayer) && Game::ActorsColliding(pActor, pPlayer)) {
        Game::PlayerTakeDamage(pPlayer, damage, pActor->position);
        return FireballDie(pActor, pPrototype, pActor->position);
    }

    Game::AdvanceCurrentAnimation(pActor, pPrototype);
}

static void InitEnemy(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    pActor->state.enemyState.health = pPrototype->data.enemyData.health;
    pActor->state.enemyState.damageCounter = 0;
    pActor->drawState.layer = SPRITE_LAYER_FG;
}

#pragma region Public API
void Game::EnemyDie(Actor* pActor, const ActorPrototype* pPrototype) {
    pActor->flags.pendingRemoval = true;

    PersistedActorData* pPersistData = GetPersistedActorData(pActor->persistId);
    if (pPersistData) {
        pPersistData->dead = true;
    }
    else SetPersistedActorData(pActor->persistId, { .dead = true });

    SpawnActor(pPrototype->data.enemyData.deathEffect, pActor->position);

    // Spawn exp halos
    const u16 totalExpValue = pPrototype->data.enemyData.expValue;
    Actor* pExpSpawner = SpawnActor(pPrototype->data.enemyData.expSpawner, pActor->position);
    pExpSpawner->state.expSpawner.remainingValue = totalExpValue;

    // Spawn loot
    SpawnActor(pPrototype->data.enemyData.lootSpawner, pActor->position);
}
#pragma endregion

constexpr ActorInitFn Game::enemyInitTable[ENEMY_TYPE_COUNT] = {
    InitEnemy,
    InitEnemy,
    InitEnemy,
};
constexpr ActorUpdateFn Game::enemyUpdateTable[ENEMY_TYPE_COUNT] = {
    UpdateSlimeEnemy,
    UpdateSkullEnemy,
    UpdateFireball,
};
constexpr ActorDrawFn Game::enemyDrawTable[ENEMY_TYPE_COUNT] = {
    Game::DrawActorDefault,
    Game::DrawActorDefault,
    Game::DrawActorDefault,
};