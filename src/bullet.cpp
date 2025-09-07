#include "actors.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

static void BulletDie(Actor* pBullet, const glm::vec2& effectPos) {
    pBullet->flags.pendingRemoval = true;
    Game::SpawnActor(pBullet->data.bullet.deathEffect, effectPos);
}

static void HandleBulletEnemyCollision(Actor* pBullet, Actor* pEnemy) {
    const ActorPrototype* pEnemyPrototype = Game::GetActorPrototype(pEnemy);

    if (pEnemy == nullptr || pEnemyPrototype->subtype == ENEMY_TYPE_FIREBALL) {
        return;
    }

    BulletDie(pBullet, pBullet->position);

    // TODO: Use value from weapon data
    constexpr u16 baseDamage = 1;
	const Damage damage = Game::CalculateDamage(pEnemy, baseDamage);

    const u16 newHealth = Game::ActorTakeDamage(pEnemy, damage, pEnemy->data.enemy.health, pEnemy->data.enemy.damageCounter);
    if (newHealth == 0) {
        Game::EnemyDie(pEnemy, pEnemyPrototype);
    }
    pEnemy->data.enemy.health = newHealth;
}

static void UpdateDefaultBullet(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->data.bullet.lifetime)) {
        BulletDie(pActor, pActor->position);
        return;
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        BulletDie(pActor, hit.impactPoint);
        return;
    }

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        BulletDie(pActor, hit.impactPoint);
        return;
    }

    Actor* pEnemy = Game::GetFirstActorCollision(pActor, ACTOR_TYPE_ENEMY);
    if (pEnemy) {
        HandleBulletEnemyCollision(pActor, pEnemy);
    }

    Game::GetAnimFrameFromDirection(pActor, pPrototype);
}

static void BulletRicochet(glm::vec2& velocity, const glm::vec2& normal) {
    velocity = glm::reflect(velocity, normal);
}

static void UpdateGrenade(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->data.bullet.lifetime)) {
        BulletDie(pActor, pActor->position);
        return;
    }

    constexpr r32 grenadeGravity = 0.04f;
    Game::ApplyGravity(pActor, grenadeGravity);

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    Actor* pEnemy = Game::GetFirstActorCollision(pActor, ACTOR_TYPE_ENEMY);
    if (pEnemy) {
        HandleBulletEnemyCollision(pActor, pEnemy);
    }

    Game::GetAnimFrameFromDirection(pActor, pPrototype);
}

static void InitializeBullet(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    pActor->drawState.layer = SPRITE_LAYER_FG;
}

constexpr ActorInitFn Game::bulletInitTable[BULLET_TYPE_COUNT] = {
    InitializeBullet,
    InitializeBullet
};

constexpr ActorUpdateFn Game::bulletUpdateTable[BULLET_TYPE_COUNT] = {
    UpdateDefaultBullet,
    UpdateGrenade,
};

constexpr ActorDrawFn Game::bulletDrawTable[BULLET_TYPE_COUNT] = {
    Game::DrawActorDefault,
    Game::DrawActorDefault,
};