#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

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
		pActor->state.playerState.entryDelayCounter = 0;
		pActor->state.playerState.deathCounter = 0;
		pActor->state.playerState.damageCounter = 0;
		pActor->state.playerState.sitCounter = 0;
		pActor->state.playerState.flags.aimMode = PLAYER_AIM_FWD;
		pActor->state.playerState.flags.doubleJumped = false;
		pActor->state.playerState.flags.sitState = PLAYER_STANDING;
		pActor->state.playerState.flags.slowFall = false;
		pActor->drawState.layer = SPRITE_LAYER_FG;
		//pActor->pDrawFn = DrawPlayer;
		break;
	}
	case ACTOR_TYPE_NPC: {
		pActor->state.npcState.health = pPrototype->data.npcData.health;
		pActor->state.npcState.damageCounter = 0;
		pActor->drawState.layer = SPRITE_LAYER_FG;
		break;
	}
	case ACTOR_TYPE_BULLET: {
		pActor->state.bulletState.lifetime = pPrototype->data.bulletData.lifetime;
		pActor->state.bulletState.lifetimeCounter = pPrototype->data.bulletData.lifetime;
		pActor->drawState.layer = SPRITE_LAYER_FG;
		break;
	}
	case ACTOR_TYPE_PICKUP: {
		pActor->drawState.layer = SPRITE_LAYER_FG;
		break;
	}
	case ACTOR_TYPE_EFFECT: {
		pActor->state.effectState.lifetime = pPrototype->data.effectData.lifetime;
		pActor->state.effectState.lifetimeCounter = pPrototype->data.effectData.lifetime;
		pActor->drawState.layer = SPRITE_LAYER_FX;

		if (pPrototype->subtype == EFFECT_SUBTYPE_NUMBERS) {
			//pActor->pDrawFn = DrawNumbers;
		}

		break;
	}
	case ACTOR_TYPE_CHECKPOINT: {
		if (persistData.activated) {
			pActor->state.checkpointState.activated = true;
		}
		pActor->drawState.layer = SPRITE_LAYER_BG;
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

DynamicActorPool* Game::GetActors() {
	return &actors;
}

void Game::UpdateActors(void (*tempCallback)(Actor* pActor)) {
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

		tempCallback(pActor);
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

