#include "actors.h"
#include "system.h"
#include <cassert>

static ActorPrototype prototypes[MAX_ACTOR_PROTOTYPE_COUNT];

#pragma region Presets
ActorPrototype* Actors::GetPrototype(s32 index) {
	return &prototypes[index];
}

void Actors::ClearPrototypes() {
	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
		memset(prototypes[i].name, 0, ACTOR_PROTOTYPE_MAX_NAME_LENGTH);
		prototypes[i].frameCount = 1;
	}
}

void Actors::LoadPrototypes(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "rb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to load actor preset file\n");
	}

	const char signature[4]{};
	fread((void*)signature, sizeof(u8), 4, pFile);

	fread(prototypes, sizeof(ActorPrototype), MAX_ACTOR_PROTOTYPE_COUNT, pFile);

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

	fclose(pFile);
}
#pragma endregion

