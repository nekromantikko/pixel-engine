#include "bullets.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

static void BulletDie(Actor* pBullet, const glm::vec2& effectPos) {
    pBullet->flags.pendingRemoval = true;
    Game::SpawnActor(pBullet->pPrototype->data.bulletData.spawnOnDeath, effectPos);
}

static void HandleBulletEnemyCollision(Actor* pBullet, Actor* pEnemy) {
    BulletDie(pBullet, pBullet->position);

    const u32 damage = Random::GenerateInt(1, 2);
    const u16 newHealth = Game::ActorTakeDamage(pEnemy, damage, pEnemy->state.npcState.health, pEnemy->state.npcState.damageCounter);
    if (newHealth == 0) {
        Game::NPCDie(pEnemy);
    }
    pEnemy->state.npcState.health = newHealth;
}

static bool ActorIsHostileNPC(const Actor* pActor) {
    return pActor->pPrototype->type == ACTOR_TYPE_NPC && pActor->pPrototype->alignment == ACTOR_ALIGNMENT_HOSTILE;
}

static void BulletCollision(Actor* pActor) {
    if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_FRIENDLY) {
        Game::ForEachActorCollision(pActor, HandleBulletEnemyCollision, ActorIsHostileNPC);
    }
    else if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_HOSTILE) {
        Actor* pPlayer = Game::GetPlayer();
        if (pPlayer && Game::GetPlayerHealth() != 0 && Game::ActorsColliding(pActor, pPlayer)) {
            Game::HandlePlayerEnemyCollision(pPlayer, pActor);
            BulletDie(pActor, pActor->position);
        }
        // TODO: Collision with friendly NPC:s? Does this happen in the game?
    }
}

static void UpdateDefaultBullet(Actor* pActor) {
    if (!Game::UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
        BulletDie(pActor, pActor->position);
        return;
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        BulletDie(pActor, hit.impactPoint);
        return;
    }

    if (Game::ActorMoveVertical(pActor, hit)) {
        BulletDie(pActor, hit.impactPoint);
        return;
    }

    BulletCollision(pActor);

    Game::GetAnimFrameFromDirection(pActor);
}

static void BulletRicochet(glm::vec2& velocity, const glm::vec2& normal) {
    velocity = glm::reflect(velocity, normal);
    //Audio::PlaySFX(&ricochetSfx, CHAN_ID_PULSE0);
}

static void UpdateGrenade(Actor* pActor) {
    if (!Game::UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
        BulletDie(pActor, pActor->position);
        return;
    }

    constexpr r32 grenadeGravity = 0.04f;
    Game::ApplyGravity(pActor, grenadeGravity);

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    if (Game::ActorMoveVertical(pActor, hit)) {
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    BulletCollision(pActor);

    Game::GetAnimFrameFromDirection(pActor);
}

static void UpdateFireball(Actor* pActor) {
    if (!Game::UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
        BulletDie(pActor, pActor->position);
        return;
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        BulletDie(pActor, hit.impactPoint);
        return;
    }

    if (Game::ActorMoveVertical(pActor, hit)) {
        BulletDie(pActor, hit.impactPoint);
        return;
    }

    BulletCollision(pActor);

    Game::AdvanceCurrentAnimation(pActor);
}

#pragma region Public API
void Game::InitializeBullet(Actor* pActor, const PersistedActorData& persistData) {
	pActor->state.bulletState.lifetime = pActor->pPrototype->data.bulletData.lifetime;
	pActor->state.bulletState.lifetimeCounter = pActor->pPrototype->data.bulletData.lifetime;
	pActor->drawState.layer = SPRITE_LAYER_FG;


    switch (pActor->pPrototype->subtype) {
    case BULLET_SUBTYPE_DEFAULT: {
        pActor->pUpdateFn = UpdateDefaultBullet;
        break;
    }
    case BULLET_SUBTYPE_GRENADE: {
        pActor->pUpdateFn = UpdateGrenade;
        break;
    }
    case BULLET_SUBTYPE_FIREBALL: {
        pActor->pUpdateFn = UpdateFireball;
        break;
    }
    default:
        pActor->pUpdateFn = nullptr;
        break;
    }

	pActor->pDrawFn == nullptr;
}
#pragma endregion