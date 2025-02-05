#include "actors.h"
#include "system.h"
#include <cassert>

static ActorPreset actorPresets[MAX_ACTOR_PRESET_COUNT];
static char nameMemory[MAX_ACTOR_PRESET_COUNT * ACTOR_MAX_NAME_LENGTH];

#pragma region Presets
ActorPreset* Actors::GetPreset(s32 index) {
	return &actorPresets[index];
}

char* Actors::GetPresetName(s32 index) {
	return &nameMemory[index * ACTOR_MAX_NAME_LENGTH];
}

void Actors::ClearPresets() {
	for (u32 i = 0; i < MAX_ACTOR_PRESET_COUNT; i++) {
		nameMemory[i * ACTOR_MAX_NAME_LENGTH] = 0;
		actorPresets[i].pMetasprite = Metasprites::GetMetasprite(0);
	}
}

void Actors::LoadPresets(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "rb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to load actor preset file\n");
	}

	const char signature[4]{};
	fread((void*)signature, sizeof(u8), 4, pFile);

	for (u32 i = 0; i < MAX_ACTOR_PRESET_COUNT; i++) {
		fread(&actorPresets[i].type, sizeof(u32), 1, pFile);
		fread(&actorPresets[i].behaviour, sizeof(u32), 1, pFile);

		fread(&actorPresets[i].hitbox, sizeof(Hitbox), 1, pFile);

		s32 metaspriteIndex;
		fread(&metaspriteIndex, sizeof(s32), 1, pFile);
		actorPresets[i].pMetasprite = Metasprites::GetMetasprite(metaspriteIndex);
	}

	fread(nameMemory, ACTOR_MAX_NAME_LENGTH, MAX_ACTOR_PRESET_COUNT, pFile);

	fclose(pFile);
}

void Actors::SavePresets(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "wb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to write actor preset file\n");
	}

	const char signature[4] = "SPR";
	fwrite(signature, sizeof(u8), 4, pFile);

	for (u32 i = 0; i < MAX_ACTOR_PRESET_COUNT; i++) {
		fwrite(&actorPresets[i].type, sizeof(u32), 1, pFile);
		fwrite(&actorPresets[i].behaviour, sizeof(u32), 1, pFile);

		fwrite(&actorPresets[i].hitbox, sizeof(Hitbox), 1, pFile);

		s32 metaspriteIndex = Metasprites::GetIndex(actorPresets[i].pMetasprite);
		fwrite(&metaspriteIndex, sizeof(s32), 1, pFile);
	}

	fwrite(nameMemory, ACTOR_MAX_NAME_LENGTH, MAX_ACTOR_PRESET_COUNT, pFile);

	fclose(pFile);
}
#pragma endregion

