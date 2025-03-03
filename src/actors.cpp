#include "actors.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"
#include "system.h"
#include <cassert>

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
		pActor->playerState.entryDelayCounter = 0;
		pActor->playerState.deathCounter = 0;
		pActor->playerState.damageCounter = 0;
		pActor->playerState.sitCounter = 0;
		pActor->playerState.flags.aimMode = PLAYER_AIM_FWD;
		pActor->playerState.flags.doubleJumped = false;
		pActor->playerState.flags.sitState = PLAYER_STANDING;
		pActor->playerState.flags.slowFall = false;
		pActor->drawState.layer = SPRITE_LAYER_FG;
		//pActor->pDrawFn = DrawPlayer;
		break;
	}
	case ACTOR_TYPE_NPC: {
		pActor->npcState.health = pPrototype->npcData.health;
		pActor->npcState.damageCounter = 0;
		pActor->drawState.layer = SPRITE_LAYER_FG;
		break;
	}
	case ACTOR_TYPE_BULLET: {
		pActor->bulletState.lifetime = pPrototype->bulletData.lifetime;
		pActor->bulletState.lifetimeCounter = pPrototype->bulletData.lifetime;
		pActor->drawState.layer = SPRITE_LAYER_FG;
		break;
	}
	case ACTOR_TYPE_PICKUP: {
		pActor->drawState.layer = SPRITE_LAYER_FG;
		break;
	}
	case ACTOR_TYPE_EFFECT: {
		pActor->effectState.lifetime = pPrototype->effectData.lifetime;
		pActor->effectState.lifetimeCounter = pPrototype->effectData.lifetime;
		pActor->drawState.layer = SPRITE_LAYER_FX;

		if (pPrototype->subtype == EFFECT_SUBTYPE_NUMBERS) {
			//pActor->pDrawFn = DrawNumbers;
		}

		break;
	}
	case ACTOR_TYPE_CHECKPOINT: {
		if (persistData.activated) {
			pActor->checkpointState.activated = true;
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

	const ActorPrototype* pPrototype = Actors::GetPrototype(prototypeIndex);
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

static ActorPrototype prototypes[MAX_ACTOR_PROTOTYPE_COUNT];
static char prototypeNames[MAX_ACTOR_PROTOTYPE_COUNT][ACTOR_PROTOTYPE_MAX_NAME_LENGTH];

#pragma region Presets
ActorPrototype* Actors::GetPrototype(s32 index) {
	if (index < 0 || index >= MAX_ACTOR_PROTOTYPE_COUNT) {
		return nullptr;
	}

	return &prototypes[index];
}

s32 Actors::GetPrototypeIndex(const ActorPrototype* pPrototype) {
	s32 index = pPrototype - prototypes;
	if (index < 0 || index >= MAX_ACTOR_PROTOTYPE_COUNT) {
		return -1;
	}

	return index;
}

char* Actors::GetPrototypeName(s32 index) {
	return prototypeNames[index];
}

char* Actors::GetPrototypeName(const ActorPrototype* pPrototype) {
	return GetPrototypeName(GetPrototypeIndex(pPrototype));
}

void Actors::GetPrototypeNames(const char** pOutNames) {
	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
		pOutNames[i] = prototypeNames[i];
	}
}

void Actors::ClearPrototypes() {
	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
		prototypes[i].animCount = 1;
	}
	memset(prototypeNames, 0, MAX_ACTOR_PROTOTYPE_COUNT * ACTOR_PROTOTYPE_MAX_NAME_LENGTH);
}

void Actors::LoadPrototypes(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "rb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to load actor preset file\n");
	}

	const char signature[4]{};
	fread((void*)signature, sizeof(u8), 4, pFile);

	//static old old_prototypes[MAX_ACTOR_PROTOTYPE_COUNT];
	//fread(old_prototypes, sizeof(old), MAX_ACTOR_PROTOTYPE_COUNT, pFile);

	/*for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
		const old& p = old_prototypes[i];
		ActorPrototype& prototype = prototypes[i];

		prototype.type = 0;
		prototype.subtype = 0;
		prototype.hitbox = p.hitbox;
		prototype.animCount = 1;
		prototype.animations[0] = {
			.type = (u8)(p.animMode - 1),
			.frameLength = 6,
			.frameCount = (u16)p.frameCount,
			.loopPoint = 0,
			.metaspriteIndex = (s16)p.frames[0].metaspriteIndex,
		};
		strcpy(prototypeNames[i], p.name);
	}*/

	fread(prototypes, sizeof(ActorPrototype), MAX_ACTOR_PROTOTYPE_COUNT, pFile);
	fread(prototypeNames, MAX_ACTOR_PROTOTYPE_COUNT, ACTOR_PROTOTYPE_MAX_NAME_LENGTH, pFile);

	fclose(pFile);


}

void Actors::SavePrototypes(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "wb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to write actor preset file\n");
	}

	const char signature[4] = "PRT";
	fwrite(signature, sizeof(u8), 4, pFile);

	fwrite(prototypes, sizeof(ActorPrototype), MAX_ACTOR_PROTOTYPE_COUNT, pFile);
	fwrite(prototypeNames, MAX_ACTOR_PROTOTYPE_COUNT, ACTOR_PROTOTYPE_MAX_NAME_LENGTH, pFile);

	fclose(pFile);
}
#pragma endregion

