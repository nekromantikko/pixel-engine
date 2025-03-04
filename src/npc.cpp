#include "npc.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

// TODO: Define in prototype data
constexpr s32 enemyFireballPrototypeIndex = 8;

static void UpdateSlimeEnemy(Actor* pActor) {
    Game::UpdateCounter(pActor->state.npcState.damageCounter);

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
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        pActor->velocity.x = 0.0f;
        pActor->flags.facingDir = (s8)hit.impactNormal.x;
    }

    Game::ApplyGravity(pActor);

    // Reset in air flag
    pActor->flags.inAir = true;

    if (Game::ActorMoveVertical(pActor, hit)) {
        pActor->velocity.y = 0.0f;

        if (hit.impactNormal.y < 0.0f) {
            pActor->flags.inAir = false;
        }
    }

    Actor* pPlayer = Game::GetPlayer();
    if (pPlayer && Game::GetPlayerHealth() != 0 && Game::ActorsColliding(pActor, pPlayer)) {
        Game::HandlePlayerEnemyCollision(pPlayer, pActor);
    }

    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    Game::SetDamagePaletteOverride(pActor, pActor->state.npcState.damageCounter);
}

static void UpdateSkullEnemy(Actor* pActor) {
    Game::UpdateCounter(pActor->state.npcState.damageCounter);

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

            Game::SpawnActor(enemyFireballPrototypeIndex, pActor->position, velocity);
        }
    }

    Actor* pPlayer = Game::GetPlayer();
    if (pPlayer && Game::GetPlayerHealth() != 0 && Game::ActorsColliding(pActor, pPlayer)) {
        Game::HandlePlayerEnemyCollision(pPlayer, pActor);
    }

    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    Game::SetDamagePaletteOverride(pActor, pActor->state.npcState.damageCounter);
}

#pragma region Public API
void Game::InitializeNPC(Actor* pActor, const PersistedActorData& persistData) {
	pActor->state.npcState.health = pActor->pPrototype->data.npcData.health;
	pActor->state.npcState.damageCounter = 0;
	pActor->drawState.layer = SPRITE_LAYER_FG;

    switch (pActor->pPrototype->subtype) {
        // Enemies
    case NPC_SUBTYPE_ENEMY_SLIME: {
        pActor->pUpdateFn = UpdateSlimeEnemy;
        break;
    }
    case NPC_SUBTYPE_ENEMY_SKULL: {
        pActor->pUpdateFn = UpdateSkullEnemy;
        break;
    }
    default:
        pActor->pUpdateFn = nullptr;
        break;
    }

	pActor->pDrawFn = nullptr;
}

void Game::NPCDie(Actor* pActor) {
    pActor->flags.pendingRemoval = true;

    PersistedActorData persistData = GetPersistedActorData(pActor->id);
    persistData.dead = true;
    SetPersistedActorData(pActor->id, persistData);

    //Audio::PlaySFX(&enemyDieSfx, CHAN_ID_NOISE);
    SpawnActor(pActor->pPrototype->data.npcData.spawnOnDeath, pActor->position);

    // Spawn exp halos
    /*const u16 totalExpValue = pActor->pPrototype->data.npcData.expValue;
    if (totalExpValue > 0) {
        SpawnExpState coroutineState = {
            .position = pActor->position,
            .remainingValue = totalExpValue
        };
        StartCoroutine(SpawnExpCoroutine, coroutineState);
    }*/
}
#pragma endregion