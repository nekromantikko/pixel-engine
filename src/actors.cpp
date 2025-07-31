#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"
#include "room.h"
#include "dungeon.h"
#include "random.h"
#include "asset_manager.h"
#include "animation.h"
#include <gtc/constants.hpp>

// TODO: Define in editor in game settings or similar
constexpr ActorPrototypeHandle dmgNumberPrototypeId(3893478668273870712);

static Pool<Actor, MAX_DYNAMIC_ACTOR_COUNT> actors;
static Pool<ActorHandle, MAX_DYNAMIC_ACTOR_COUNT> actorRemoveList;

PoolHandle<Actor> playerHandle;

static void InitializeActor(Actor* pActor, const ActorPrototype* pPrototype) {
	pActor->flags.facingDir = ACTOR_FACING_RIGHT;
	pActor->flags.inAir = true;
	pActor->flags.active = true;
	pActor->flags.pendingRemoval = false;

	pActor->initialPosition = pActor->position;
	pActor->initialVelocity = pActor->velocity;

	pActor->drawState = ActorDrawState{};

	const PersistedActorData* pPersistData = Game::GetPersistedActorData(pActor->persistId);

	Game::actorInitTable[pPrototype->type][pPrototype->subtype](pActor, pPrototype, pPersistData);
}

Actor* Game::SpawnActor(const RoomActor* pTemplate, u32 roomId) {
	if (actors.Count() >= MAX_DYNAMIC_ACTOR_COUNT) {
		return nullptr;
	}

	if (pTemplate == nullptr) {
		return nullptr;
	}

	Actor actor{
		.persistId = pTemplate->id | u64(roomId) << 32,
		.position = pTemplate->position,
		.prototypeHandle = pTemplate->prototypeHandle,
	};

	const ActorPrototype* pPrototype = AssetManager::GetAsset(actor.prototypeHandle);
	if (!pPrototype) {
		return nullptr;
	}

	InitializeActor(&actor, pPrototype);
	const ActorHandle handle = actors.Add(actor);

	if (pPrototype->type == ACTOR_TYPE_PLAYER) {
		playerHandle = handle;
	}

	return actors.Get(handle);
}
Actor* Game::SpawnActor(const ActorPrototypeHandle& prototypeHandle, const glm::vec2& position, const glm::vec2& velocity) {
	if (actors.Count() >= MAX_DYNAMIC_ACTOR_COUNT || prototypeHandle == ActorPrototypeHandle::Null()) {
		return nullptr;
	}

	Actor actor {
		.persistId = UUID_NULL,
		.position = position,
		.velocity = velocity,
		.prototypeHandle = prototypeHandle,
	};

	const ActorPrototype* pPrototype = AssetManager::GetAsset(actor.prototypeHandle);
	if (!pPrototype) {
		return nullptr;
	}

	InitializeActor(&actor, pPrototype);
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

const ActorPrototype* Game::GetActorPrototype(const Actor* pActor) {
	return AssetManager::GetAsset(pActor->prototypeHandle);
}

const Animation* Game::GetActorCurrentAnim(const Actor* pActor, const ActorPrototype* pPrototype) {
	const AnimationHandle& currentAnimId = pPrototype->animations[pActor->drawState.animIndex];
	return AssetManager::GetAsset(currentAnimId);
}

bool Game::ActorValid(const Actor* pActor) {
	return pActor != nullptr && pActor->flags.active && !pActor->flags.pendingRemoval;
}

bool Game::ActorsColliding(const Actor* pActor, const Actor* pOther) {
	const ActorPrototype* pPrototype = GetActorPrototype(pActor);
	const ActorPrototype* pPrototypeOther = GetActorPrototype(pOther);

	if (!pPrototype || !pPrototypeOther) {
		return false;
	}

	const AABB& hitbox = pPrototype->hitbox;
	const AABB& hitboxOther = pPrototypeOther->hitbox;

	return Collision::BoxesOverlap(hitbox, pActor->position, hitboxOther, pOther->position);
}

void Game::ForEachActorCollision(Actor* pActor, TActorType type, ActorCollisionCallbackFn callback) {
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

		const ActorPrototype* pPrototype = GetActorPrototype(pOther);
		if (!pPrototype || pPrototype->type != type) {
			continue;
		}

		if (ActorsColliding(pActor, pOther)) {
			callback(pActor, pOther);
		}
	}
}
void Game::ForEachActorCollision(Actor* pActor, ActorFilterFn filter, ActorCollisionCallbackFn callback) {
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
Actor* Game::GetFirstActorCollision(const Actor* pActor, TActorType type) {
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

		const ActorPrototype* pPrototype = GetActorPrototype(pOther);
		if (!pPrototype || pPrototype->type != type) {
			continue;
		}

		if (ActorsColliding(pActor, pOther)) {
			return pOther;
		}
	}

	return nullptr;
}
Actor* Game::GetFirstActorCollision(const Actor* pActor, ActorFilterFn filter) {
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
void Game::ForEachActor(TActorType type, ActorCallbackFn callback) {
	for (u32 i = 0; i < actors.Count(); i++)
	{
		ActorHandle handle = actors.GetHandle(i);
		Actor* pActor = actors.Get(handle);

		if (!ActorValid(pActor)) {
			continue;
		}

		const ActorPrototype* pPrototype = GetActorPrototype(pActor);
		if (!pPrototype || pPrototype->type != type) {
			continue;
		}

		callback(pActor);
	}
}
void Game::ForEachActor(ActorFilterFn filter, ActorCallbackFn callback) {
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
Actor* Game::GetFirstActor(TActorType type) {
	for (u32 i = 0; i < actors.Count(); i++)
	{
		ActorHandle handle = actors.GetHandle(i);
		Actor* pActor = actors.Get(handle);

		if (!ActorValid(pActor)) {
			continue;
		}

		const ActorPrototype* pPrototype = GetActorPrototype(pActor);
		if (!pPrototype || pPrototype->type != type) {
			continue;
		}

		return pActor;
	}

	return nullptr;
}
Actor* Game::GetFirstActor(ActorFilterFn filter) {
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

void Game::GetAnimFrameFromDirection(Actor* pActor, const ActorPrototype* pPrototype) {
	const glm::vec2 dir = glm::normalize(pActor->velocity);
	const r32 angle = glm::atan(dir.y, dir.x);

	const Animation* pCurrentAnim = GetActorCurrentAnim(pActor, pPrototype);
	if (!pCurrentAnim) {
		return;
	}

	pActor->drawState.frameIndex = (s32)glm::roundEven(((angle + glm::pi<r32>()) / (glm::pi<r32>() * 2)) * pCurrentAnim->frameCount) % pCurrentAnim->frameCount;
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

void Game::AdvanceCurrentAnimation(Actor* pActor, const ActorPrototype* pPrototype) {
	const Animation* pCurrentAnim = GetActorCurrentAnim(pActor, pPrototype);
	if (!pCurrentAnim) {
		return;
	}

	AdvanceAnimation(pActor->drawState.animCounter, pActor->drawState.frameIndex, pCurrentAnim->frameCount, pCurrentAnim->frameLength, pCurrentAnim->loopPoint);
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

bool Game::ActorMoveHorizontal(Actor* pActor, const ActorPrototype* pPrototype, HitResult& outHit) {
	const AABB& hitbox = pPrototype->hitbox;

	const r32 dx = pActor->velocity.x;

	Collision::SweepBoxHorizontal(Game::GetCurrentTilemap(), hitbox, pActor->position, dx, outHit);
	pActor->position.x = outHit.location.x;
	return outHit.blockingHit;
}

bool Game::ActorMoveVertical(Actor* pActor, const ActorPrototype* pPrototype, HitResult& outHit) {
	const AABB& hitbox = pPrototype->hitbox;

	const r32 dy = pActor->velocity.y;

	Collision::SweepBoxVertical(Game::GetCurrentTilemap(), hitbox, pActor->position, dy, outHit);
	pActor->position.y = outHit.location.y;
	return outHit.blockingHit;
}

void Game::ApplyGravity(Actor* pActor, r32 gravity) {
	pActor->velocity.y += gravity;
}
#pragma endregion

#pragma region Damage
Damage Game::CalculateDamage(Actor* pActor, u16 baseDamage) {
	const r32 damageMultiplier = Random::GenerateReal(0.95f, 1.05f); // 5% variation

	// TODO: Take actor stats into account
	Damage result{};

	result.value = glm::roundEven(baseDamage * damageMultiplier);

	constexpr u16 critRate = 8;
	constexpr u16 critMultiplier = 2;
	result.flags.crit = Random::GenerateInt(0, 127) < critRate;
	if (result.flags.crit) {
		result.value *= critMultiplier;
	}

	return result;
}

static void SpawnDamageNumber(Actor* pActor, const Damage& damage) {
	glm::vec2 spawnPos = pActor->position;

	const ActorPrototype* pPrototype = Game::GetActorPrototype(pActor);
	if (pPrototype) {
		const AABB& hitbox = pPrototype->hitbox;
		const glm::vec2 randomPointInsideHitbox = {
			Random::GenerateReal(hitbox.x1, hitbox.x2),
			Random::GenerateReal(hitbox.y1, hitbox.y2)
		};
		spawnPos += randomPointInsideHitbox;
	}

	constexpr glm::vec2 velocity = { 0, -0.03125f };
	Actor* pDmg = Game::SpawnActor(dmgNumberPrototypeId, spawnPos, velocity);
	if (pDmg != nullptr) {
		pDmg->state.dmgNumberState.damage = damage;
	}
}

// Returns new health after taking damage
u16 Game::ActorTakeDamage(Actor* pActor, const Damage& damage, u16 currentHealth, u16& damageCounter) {
	constexpr s32 damageDelay = 30;

	u16 newHealth = currentHealth;
	if (damage.value > newHealth) {
		newHealth = 0;
	}
	else newHealth -= damage.value;
	damageCounter = damageDelay;

	SpawnDamageNumber(pActor, damage);

	return newHealth;
}

// Returns new health after healing
u16 Game::ActorHeal(Actor* pActor, u16 value, u16 currentHealth, u16 maxHealth) {
	u16 newHealth = currentHealth + value;
	if (newHealth > maxHealth) {
		newHealth = maxHealth; // Cap health at maximum
	}

	Damage healing{};
	healing.value = value;
	healing.flags.healing = true;

	SpawnDamageNumber(pActor, healing);

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

		const ActorPrototype* pPrototype = Game::GetActorPrototype(pActor);
		if (!pPrototype) {
			continue;
		}

		actorUpdateTable[pPrototype->type][pPrototype->subtype](pActor, pPrototype);
	}

	for (u32 i = 0; i < actorRemoveList.Count(); i++) {
		auto handle = *actorRemoveList.Get(actorRemoveList.GetHandle(i));
		actors.Remove(handle);
	}

	actorRemoveList.Clear();
}

bool Game::DrawActorDefault(const Actor* pActor, const ActorPrototype* pPrototype) {
	const ActorDrawState& drawState = pActor->drawState;

	// Culling
	if (!Game::Rendering::PositionInViewportBounds(pActor->position) || !drawState.visible) {
		return false;
	}

	const u16 animIndex = drawState.animIndex % pPrototype->animCount;

	const Animation* pCurrentAnim = GetActorCurrentAnim(pActor, pPrototype);
	if (!pCurrentAnim) {
		return false;
	}

	glm::i16vec2 drawPos = Game::Rendering::WorldPosToScreenPixels(pActor->position) + drawState.pixelOffset;
	const s32 customPalette = drawState.useCustomPalette ? drawState.palette : -1;

	const AnimationFrame& frame = pCurrentAnim->GetFrames()[drawState.frameIndex];

	return Game::Rendering::DrawMetasprite(drawState.layer, frame.metaspriteId, drawPos, drawState.hFlip, drawState.vFlip, customPalette);
}

void Game::DrawActors() {
	for (u32 i = 0; i < actors.Count(); i++)
	{
		Actor* pActor = actors.Get(actors.GetHandle(i));
		if (!ActorValid(pActor)) {
			continue;
		}

		const ActorPrototype* pPrototype = Game::GetActorPrototype(pActor);
		if (!pPrototype) {
			continue;
		}

		actorDrawTable[pPrototype->type][pPrototype->subtype](pActor, pPrototype);
	}
}

