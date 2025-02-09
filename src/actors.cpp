#include "actors.h"
#include "system.h"
#include <cassert>

static ActorPrototype prototypes[MAX_ACTOR_PROTOTYPE_COUNT];
static char nameMemory[MAX_ACTOR_PROTOTYPE_COUNT * ACTOR_MAX_NAME_LENGTH];
static ActorAnimFrame frameMemory[MAX_ACTOR_PROTOTYPE_COUNT * ACTOR_MAX_FRAME_COUNT];

#pragma region Presets
ActorPrototype* Actors::GetPrototype(s32 index) {
	return &prototypes[index];
}

char* Actors::GetPrototypeName(s32 index) {
	return &nameMemory[index * ACTOR_MAX_NAME_LENGTH];
}

void Actors::ClearPrototypes() {
	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
		nameMemory[i * ACTOR_MAX_NAME_LENGTH] = 0;
		prototypes[i].frameCount = 1;
		prototypes[i].pFrames = &frameMemory[i * ACTOR_MAX_FRAME_COUNT];
		frameMemory[i * ACTOR_MAX_FRAME_COUNT] = { 0,0 };
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

	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
		fread(&prototypes[i].type, sizeof(u32), 1, pFile);
		fread(&prototypes[i].behaviour, sizeof(u32), 1, pFile);
		fread(&prototypes[i].animMode, sizeof(u32), 1, pFile);

		fread(&prototypes[i].hitbox, sizeof(AABB), 1, pFile);

		fread(&prototypes[i].frameCount, sizeof(u32), 1, pFile);
		ActorAnimFrame* const firstFrame = frameMemory + i * ACTOR_MAX_FRAME_COUNT;
		prototypes[i].pFrames = firstFrame;
	}

	fread(nameMemory, ACTOR_MAX_NAME_LENGTH, MAX_ACTOR_PROTOTYPE_COUNT, pFile);
	fread(frameMemory, sizeof(ActorAnimFrame), MAX_ACTOR_PROTOTYPE_COUNT * ACTOR_MAX_FRAME_COUNT, pFile);

	fclose(pFile);
}

void Actors::SavePrototypes(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "wb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to write actor preset file\n");
	}

	const char signature[4] = "SPR";
	fwrite(signature, sizeof(u8), 4, pFile);

	for (u32 i = 0; i < MAX_ACTOR_PROTOTYPE_COUNT; i++) {
		fwrite(&prototypes[i].type, sizeof(u32), 1, pFile);
		fwrite(&prototypes[i].behaviour, sizeof(u32), 1, pFile);
		fwrite(&prototypes[i].animMode, sizeof(u32), 1, pFile);

		fwrite(&prototypes[i].hitbox, sizeof(AABB), 1, pFile);

		fwrite(&prototypes[i].frameCount, sizeof(u32), 1, pFile);
	}

	fwrite(nameMemory, ACTOR_MAX_NAME_LENGTH, MAX_ACTOR_PROTOTYPE_COUNT, pFile);
	fwrite(frameMemory, sizeof(ActorAnimFrame), MAX_ACTOR_PROTOTYPE_COUNT * ACTOR_MAX_FRAME_COUNT, pFile);

	fclose(pFile);
}
#pragma endregion

