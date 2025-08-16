#include "actors.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

static void BulletDie(Actor* pBullet, const ActorPrototype* pPrototype, const glm::vec2& effectPos) {
    pBullet->flags.pendingRemoval = true;
    Game::SpawnActor(pPrototype->data.bulletData.deathEffect, effectPos);
}

static void HandleBulletTileCollision(Actor* pBullet, const ActorPrototype* pPrototype, const HitResult& hit) {
    // Check if we hit a destructible tile
    if (hit.tileType == TILE_DESTRUCTIBLE) {
        Game::DestroyTileAt(hit.tileCoord, hit.impactPoint);
    }
    
    // Bullet dies on any tile collision (solid or destructible)
    BulletDie(pBullet, pPrototype, hit.impactPoint);
}

static void HandleBulletEnemyCollision(Actor* pBullet, const ActorPrototype* pPrototype, Actor* pEnemy) {
    const ActorPrototype* pEnemyPrototype = Game::GetActorPrototype(pEnemy);

    if (pEnemy == nullptr || pEnemyPrototype->subtype == ENEMY_TYPE_FIREBALL) {
        return;
    }

    BulletDie(pBullet, pPrototype, pBullet->position);

    // TODO: Use value from weapon data
    constexpr u16 baseDamage = 1;
	const Damage damage = Game::CalculateDamage(pEnemy, baseDamage);

    const u16 newHealth = Game::ActorTakeDamage(pEnemy, damage, pEnemy->state.enemyState.health, pEnemy->state.enemyState.damageCounter);
    if (newHealth == 0) {
        Game::EnemyDie(pEnemy, pEnemyPrototype);
    }
    pEnemy->state.enemyState.health = newHealth;
}

static void UpdateDefaultBullet(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
        BulletDie(pActor, pPrototype, pActor->position);
        return;
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        HandleBulletTileCollision(pActor, pPrototype, hit);
        return;
    }

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        HandleBulletTileCollision(pActor, pPrototype, hit);
        return;
    }

    Actor* pEnemy = Game::GetFirstActorCollision(pActor, ACTOR_TYPE_ENEMY);
    if (pEnemy) {
        HandleBulletEnemyCollision(pActor, pPrototype, pEnemy);
    }

    Game::GetAnimFrameFromDirection(pActor, pPrototype);
}

static void BulletRicochet(glm::vec2& velocity, const glm::vec2& normal) {
    velocity = glm::reflect(velocity, normal);
}

static void UpdateGrenade(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
        BulletDie(pActor, pPrototype, pActor->position);
        return;
    }

    constexpr r32 grenadeGravity = 0.04f;
    Game::ApplyGravity(pActor, grenadeGravity);

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        if (hit.tileType == TILE_DESTRUCTIBLE) {
            Game::DestroyTileAt(hit.tileCoord, hit.impactPoint);
        }
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        if (hit.tileType == TILE_DESTRUCTIBLE) {
            Game::DestroyTileAt(hit.tileCoord, hit.impactPoint);
        }
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    Actor* pEnemy = Game::GetFirstActorCollision(pActor, ACTOR_TYPE_ENEMY);
    if (pEnemy) {
        HandleBulletEnemyCollision(pActor, pPrototype, pEnemy);
    }

    Game::GetAnimFrameFromDirection(pActor, pPrototype);
}

static void InitializeBullet(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    pActor->state.bulletState.lifetimeCounter = pPrototype->data.bulletData.lifetime;
    pActor->drawState.layer = SPRITE_LAYER_FG;;
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