#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"
#include "level.h"
#include "random.h"
#include <gtc/constants.hpp>

// TODO: Define in editor in game settings or similar
constexpr s32 dmgNumberPrototypeIndex = 5;

static Pool<Actor, MAX_DYNAMIC_ACTOR_COUNT> actors;
static Pool<ActorHandle, MAX_DYNAMIC_ACTOR_COUNT> actorRemoveList;

PoolHandle<Actor> playerHandle;

static void InitializeActor(Actor* pActor) {
	const ActorPrototype* pPrototype = pActor->pPrototype;

	pActor->flags.facingDir = ACTOR_FACING_RIGHT;
	pActor->flags.inAir = true;
	pActor->flags.active = true;
	pActor->flags.pendingRemoval = false;

	pActor->initialPosition = pActor->position;
	pActor->initialVelocity = pActor->velocity;

	pActor->drawState = ActorDrawState{};

	pActor->pUpdateFn = nullptr;
	pActor->pDrawFn = nullptr;

	const PersistedActorData persistData = Game::GetPersistedActorData(pActor->id);

	switch (pPrototype->type) {
	case ACTOR_TYPE_PLAYER: {
		Game::InitializePlayer(pActor, persistData);
		break;
	}
	case ACTOR_TYPE_NPC: {
		Game::InitializeNPC(pActor, persistData);
		break;
	}
	case ACTOR_TYPE_BULLET: {
		Game::InitializeBullet(pActor, persistData);
		break;
	}
	case ACTOR_TYPE_PICKUP: {
		Game::InitializePickup(pActor, persistData);
		break;
	}
	case ACTOR_TYPE_EFFECT: {
		Game::InitializeEffect(pActor, persistData);
		break;
	}
	case ACTOR_TYPE_CHECKPOINT: {
		Game::InitializeCheckpoint(pActor, persistData);
		break;
	}
	default:
		break;
	}
}

Actor* Game::SpawnActor(const Actor* pTemplate) {
	if (actors.Count() >= MAX_DYNAMIC_ACTOR_COUNT) {
		return nullptr;
	}

	if (pTemplate == nullptr) {
		return nullptr;
	}

	Actor actor = *pTemplate;
	InitializeActor(&actor);

	const ActorHandle handle = actors.Add(actor);

	if (actor.pPrototype->type == ACTOR_TYPE_PLAYER) {
		playerHandle = handle;
	}

	return actors.Get(handle);
}
Actor* Game::SpawnActor(const s32 prototypeIndex, const glm::vec2& position, const glm::vec2& velocity) {
	if (actors.Count() >= MAX_DYNAMIC_ACTOR_COUNT) {
		return nullptr;
	}

	const ActorPrototype* pPrototype = Assets::GetActorPrototype(prototypeIndex);
	if (pPrototype == nullptr) {
		return nullptr;
	}

	Actor actor{};
	actor.id = Random::GenerateUUID();
	actor.pPrototype = pPrototype;
	actor.position = position;
	actor.velocity = velocity;

	InitializeActor(&actor);
	
	const ActorHandle handle = actors.Add(actor);

	if (pPrototype->type == ACTOR_TYPE_PLAYER) {
		playerHandle = handle;
	}

	return actors.Get(handle);
}

void Game::ClearActors() {
	actors.Clear();
	actorRemoveList.Clear();
}

bool Game::ActorValid(const Actor* pActor) {
	return pActor != nullptr && pActor->flags.active && !pActor->flags.pendingRemoval;
}

bool Game::ActorsColliding(const Actor* pActor, const Actor* pOther) {
	const AABB& hitbox = pActor->pPrototype->hitbox;
	const AABB& hitboxOther = pOther->pPrototype->hitbox;

	return Collision::BoxesOverlap(hitbox, pActor->position, hitboxOther, pOther->position);
}

void Game::ForEachActorCollision(Actor* pActor, void (*callback)(Actor*, Actor*), bool (*filter)(const Actor*)) {
	if (!ActorValid(pActor)) {
		return;
	}

	for (u32 i = 0; i < actors.Count(); i++)
	{
		ActorHandle handle = actors.GetHandle(i);
		Actor* pOther = actors.Get(handle);

		if (!ActorValid(pOther)) {
			continue;
		}

		if (filter != nullptr && !filter(pOther)) {
			continue;
		}

		if (ActorsColliding(pActor, pOther)) {
			callback(pActor, pOther);
		}
	}
}
Actor* Game::GetFirstActorCollision(Actor* pActor, bool (*filter)(const Actor*)) {
	if (!ActorValid(pActor)) {
		return nullptr;
	}

	for (u32 i = 0; i < actors.Count(); i++)
	{
		ActorHandle handle = actors.GetHandle(i);
		Actor* pOther = actors.Get(handle);

		if (!ActorValid(pOther)) {
			continue;
		}

		if (filter != nullptr && !filter(pOther)) {
			continue;
		}

		if (ActorsColliding(pActor, pOther)) {
			return pOther;
		}
	}

	return nullptr;
}

void Game::ForEachActor(void (*callback)(Actor* pActor), bool (*filter)(const Actor*)) {
	for (u32 i = 0; i < actors.Count(); i++)
	{
		ActorHandle handle = actors.GetHandle(i);
		Actor* pActor = actors.Get(handle);

		if (!ActorValid(pActor)) {
			continue;
		}

		if (filter != nullptr && !filter(pActor)) {
			continue;
		}

		callback(pActor);
	}

}
Actor* Game::GetFirstActor(bool (*filter)(const Actor*)) {
	for (u32 i = 0; i < actors.Count(); i++)
	{
		ActorHandle handle = actors.GetHandle(i);
		Actor* pActor = actors.Get(handle);

		if (!ActorValid(pActor)) {
			continue;
		}

		if (filter != nullptr && !filter(pActor)) {
			continue;
		}

		return pActor;
	}

	return nullptr;

}

Actor* Game::GetPlayer() {
	return actors.Get(playerHandle);
}

#pragma region Actor utils
// Returns false if counter stops, true if keeps running
bool Game::UpdateCounter(u16& counter) {
	if (counter == 0) {
		return false;
	}

	if (--counter == 0) {
		return false;
	}

	return true;
}

void Game::SetDamagePaletteOverride(Actor* pActor, u16 damageCounter) {
	if (damageCounter > 0) {
		pActor->drawState.useCustomPalette = true;
		pActor->drawState.palette = (GetFramesElapsed() / 3) % 4;
	}
	else {
		pActor->drawState.useCustomPalette = false;
	}
}

void Game::GetAnimFrameFromDirection(Actor* pActor) {
	const glm::vec2 dir = glm::normalize(pActor->velocity);
	const r32 angle = glm::atan(dir.y, dir.x);

	const Animation& currentAnim = pActor->pPrototype->animations[0];
	pActor->drawState.frameIndex = (s32)glm::roundEven(((angle + glm::pi<r32>()) / (glm::pi<r32>() * 2)) * currentAnim.frameCount) % currentAnim.frameCount;
}

void Game::AdvanceAnimation(u16& animCounter, u16& frameIndex, u16 frameCount, u8 frameLength, s16 loopPoint) {
	const bool loop = loopPoint != -1;
	if (animCounter == 0) {
		// End of anim reached
		if (frameIndex == frameCount - 1) {
			if (loop) {
				frameIndex = loopPoint;
			}
			else return;
		}
		else frameIndex++;

		animCounter = frameLength;
		return;
	}
	animCounter--;
}

void Game::AdvanceCurrentAnimation(Actor* pActor) {
	const Animation& currentAnim = pActor->pPrototype->animations[0];
	AdvanceAnimation(pActor->drawState.animCounter, pActor->drawState.frameIndex, currentAnim.frameCount, currentAnim.frameLength, currentAnim.loopPoint);
}
#pragma endregion

#pragma region Movement
void Game::ActorFacePlayer(Actor* pActor) {
	pActor->flags.facingDir = ACTOR_FACING_RIGHT;

	Actor* pPlayer = GetPlayer();
	if (pPlayer == nullptr) {
		return;
	}

	if (pPlayer->position.x < pActor->position.x) {
		pActor->flags.facingDir = ACTOR_FACING_LEFT;
	}
}

bool Game::ActorMoveHorizontal(Actor* pActor, HitResult& outHit) {
	const AABB& hitbox = pActor->pPrototype->hitbox;

	const r32 dx = pActor->velocity.x;

	Collision::SweepBoxHorizontal(GetCurrentLevel()->pTilemap, hitbox, pActor->position, dx, outHit);
	pActor->position.x = outHit.location.x;
	return outHit.blockingHit;
}

bool Game::ActorMoveVertical(Actor* pActor, HitResult& outHit) {
	const AABB& hitbox = pActor->pPrototype->hitbox;

	const r32 dy = pActor->velocity.y;

	Collision::SweepBoxVertical(GetCurrentLevel()->pTilemap, hitbox, pActor->position, dy, outHit);
	pActor->position.y = outHit.location.y;
	return outHit.blockingHit;
}

void Game::ApplyGravity(Actor* pActor, r32 gravity) {
	pActor->velocity.y += gravity;
}
#pragma endregion

#pragma region Damage
// Returns new health after taking damage
u16 Game::ActorTakeDamage(Actor* pActor, u32 dmgValue, u16 currentHealth, u16& damageCounter) {
	constexpr s32 damageDelay = 30;

	u16 newHealth = currentHealth;
	if (dmgValue > newHealth) {
		newHealth = 0;
	}
	else newHealth -= dmgValue;
	damageCounter = damageDelay;

	// Spawn damage numbers
	const AABB& hitbox = pActor->pPrototype->hitbox;
	// Random point inside hitbox
	const glm::vec2 randomPointInsideHitbox = {
		Random::GenerateReal(hitbox.x1, hitbox.x2),
		Random::GenerateReal(hitbox.y1, hitbox.y2)
	};
	const glm::vec2 spawnPos = pActor->position + randomPointInsideHitbox;

	constexpr glm::vec2 velocity = { 0, -0.03125f };
	Actor* pDmg = SpawnActor(dmgNumberPrototypeIndex, spawnPos, velocity);
	if (pDmg != nullptr) {
		pDmg->state.effectState.value = -dmgValue;
	}

	return newHealth;
}
#pragma endregion

DynamicActorPool* Game::GetActors() {
	return &actors;
}

void Game::UpdateActors() {
	for (u32 i = 0; i < actors.Count(); i++)
	{
		PoolHandle<Actor> handle = actors.GetHandle(i);
		Actor* pActor = actors.Get(handle);

		if (pActor == nullptr) {
			continue;
		}

		if (pActor->flags.pendingRemoval) {
			actorRemoveList.Add(handle);
			continue;
		}

		if (!pActor->flags.active) {
			continue;
		}

		if (pActor->pUpdateFn) {
			pActor->pUpdateFn(pActor);
		}
	}

	for (u32 i = 0; i < actorRemoveList.Count(); i++) {
		auto handle = *actorRemoveList.Get(actorRemoveList.GetHandle(i));
		actors.Remove(handle);
	}

	actorRemoveList.Clear();
}

bool Game::DrawActorDefault(const Actor* pActor) {
	const ActorDrawState& drawState = pActor->drawState;

	// Culling
	if (!Game::Rendering::PositionInViewportBounds(pActor->position) || !drawState.visible) {
		return false;
	}

	glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels(pActor->position) + drawState.pixelOffset;
	const u16 animIndex = drawState.animIndex % pActor->pPrototype->animCount;
	const Animation& currentAnim = pActor->pPrototype->animations[animIndex];
	const s32 customPalette = drawState.useCustomPalette ? drawState.palette : -1;

	switch (currentAnim.type) {
	case ANIMATION_TYPE_SPRITES: {
		Game::Rendering::DrawMetaspriteSprite(drawState.layer, currentAnim.metaspriteIndex, drawState.frameIndex, drawPos, drawState.hFlip, drawState.vFlip, customPalette);
		break;
	}
	case ANIMATION_TYPE_METASPRITES: {
		Game::Rendering::DrawMetasprite(drawState.layer, currentAnim.metaspriteIndex + drawState.frameIndex, drawPos, drawState.hFlip, drawState.vFlip, customPalette);
		break;
	}
	default:
		break;
	}

	return true;
}

void Game::DrawActors() {
	for (u32 i = 0; i < actors.Count(); i++)
	{
		Actor* pActor = actors.Get(actors.GetHandle(i));
		if (!ActorValid(pActor)) {
			continue;
		}

		if (pActor->pDrawFn) {
			pActor->pDrawFn(pActor);
		}
		else {
			DrawActorDefault(pActor);
		}
	}
}

