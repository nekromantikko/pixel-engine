#include "actor_prototypes.h"
#include "debug.h"
#include "asset_manager.h"

static ActorPrototype prototypes[MAX_ACTOR_PROTOTYPE_COUNT];
static char prototypeNames[MAX_ACTOR_PROTOTYPE_COUNT][ACTOR_PROTOTYPE_MAX_NAME_LENGTH];

ActorPrototype* Assets::GetActorPrototype(s32 index) {
	if (index < 0 || index >= MAX_ACTOR_PROTOTYPE_COUNT) {
		return nullptr;
	}

	return &prototypes[index];
}

s32 Assets::GetActorPrototypeIndex(const ActorPrototype* pPrototype) {
	s32 index = pPrototype - prototypes;
	if (index < 0 || index >= MAX_ACTOR_PROTOTYPE_COUNT) {
		return -1;
	}

	return index;
}

char* Assets::GetActorPrototypeName(s32 index) {
	return prototypeNames[index];
}

char* Assets::GetActorPrototypeName(const ActorPrototype* pPrototype) {
	return GetActorPrototypeName(GetActorPrototypeIndex(pPrototype));
}

void Assets::GetActorPrototypeNames(const char** pOutNames) {
	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
		pOutNames[i] = prototypeNames[i];
	}
}

void Assets::ClearActorPrototypes() {
	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
		prototypes[i].animCount = 1;
	}
	memset(prototypeNames, 0, MAX_ACTOR_PROTOTYPE_COUNT * ACTOR_PROTOTYPE_MAX_NAME_LENGTH);
}

void Assets::LoadActorPrototypes(const char* fname) {
	/*FILE* pFile;
	fopen_s(&pFile, fname, "rb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to load actor preset file\n");
	}

	const char signature[4]{};
	fread((void*)signature, sizeof(u8), 4, pFile);

	fread(prototypes, sizeof(ActorPrototype), MAX_ACTOR_PROTOTYPE_COUNT, pFile);
	fread(prototypeNames, MAX_ACTOR_PROTOTYPE_COUNT, ACTOR_PROTOTYPE_MAX_NAME_LENGTH, pFile);

	fclose(pFile);*/

	/*for (u32 i = 0; i < 18; i++) {
		const char* name = prototypeNames[i];
		ActorPrototypeHandle handle = AssetManager::CreateAsset<ASSET_TYPE_ACTOR_PROTOTYPE>(sizeof(ActorPrototypeNew), name);
		ActorPrototypeNew* pPrototype = (ActorPrototypeNew*)AssetManager::GetAsset(handle);

		ActorPrototype& old = prototypes[i];

		pPrototype->type = old.type;
		pPrototype->subtype = old.subtype;
		pPrototype->hitbox = old.hitbox;
		pPrototype->animCount = old.animCount;
		memcpy(pPrototype->data.raw, old.data.raw, ACTOR_PROTOTYPE_DATA_SIZE);
	}*/
}

void Assets::SaveActorPrototypes(const char* fname) {
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