#include "actors.h"
#include "system.h"
#include <cassert>

static ActorPreset actorPresets[MAX_ACTOR_PRESET_COUNT];
static char nameMemory[MAX_ACTOR_PRESET_COUNT * ACTOR_MAX_NAME_LENGTH];
static ActorAnimFrame frameMemory[MAX_ACTOR_PRESET_COUNT * ACTOR_MAX_FRAME_COUNT];

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
		actorPresets[i].frameCount = 1;
		actorPresets[i].pFrames = &frameMemory[i * ACTOR_MAX_FRAME_COUNT];
		frameMemory[i * ACTOR_MAX_FRAME_COUNT] = { 0,0 };
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
		fread(&actorPresets[i].animMode, sizeof(u32), 1, pFile);

		fread(&actorPresets[i].hitbox, sizeof(Hitbox), 1, pFile);

		fread(&actorPresets[i].frameCount, sizeof(u32), 1, pFile);
		ActorAnimFrame* const firstFrame = frameMemory + i * ACTOR_MAX_FRAME_COUNT;
		actorPresets[i].pFrames = firstFrame;
	}

	fread(nameMemory, ACTOR_MAX_NAME_LENGTH, MAX_ACTOR_PRESET_COUNT, pFile);
	fread(frameMemory, sizeof(ActorAnimFrame), MAX_ACTOR_PRESET_COUNT * ACTOR_MAX_FRAME_COUNT, pFile);

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
		fwrite(&actorPresets[i].animMode, sizeof(u32), 1, pFile);

		fwrite(&actorPresets[i].hitbox, sizeof(Hitbox), 1, pFile);

		fwrite(&actorPresets[i].frameCount, sizeof(u32), 1, pFile);
	}

	fwrite(nameMemory, ACTOR_MAX_NAME_LENGTH, MAX_ACTOR_PRESET_COUNT, pFile);
	fwrite(frameMemory, sizeof(ActorAnimFrame), MAX_ACTOR_PRESET_COUNT * ACTOR_MAX_FRAME_COUNT, pFile);

	fclose(pFile);
}
#pragma endregion

