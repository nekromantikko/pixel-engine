#include "actors.h"
#include "system.h"
#include <cassert>

static ActorPrototype prototypes[MAX_ACTOR_PROTOTYPE_COUNT];
static char prototypeNames[MAX_ACTOR_PROTOTYPE_COUNT][ACTOR_PROTOTYPE_MAX_NAME_LENGTH];

#pragma region Presets
ActorPrototype* Actors::GetPrototype(s32 index) {
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

