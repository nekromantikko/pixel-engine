#include "metasprite.h"
#include "system.h"

namespace Metasprite {

	Metasprite metasprites[maxMetaspriteCount];
	char nameMemory[maxMetaspriteCount * metaspriteMaxNameLength];
	Rendering::Sprite spriteMemory[maxMetaspriteCount * metaspriteMaxSpriteCount];
	Collision::Collider colliderMemory[maxMetaspriteCount * metaspriteMaxColliderCount];

	Metasprite* GetMetaspritesPtr() {
		return metasprites;
	}

	void InitializeMetasprites() {
		for (u32 i = 0; i < maxMetaspriteCount; i++) {
			metasprites[i].name = &nameMemory[i * metaspriteMaxNameLength];
			nameMemory[i * metaspriteMaxNameLength] = 0;

			metasprites[i].spritesRelativePos = &spriteMemory[i * metaspriteMaxSpriteCount];
			spriteMemory[i * metaspriteMaxSpriteCount] = Rendering::Sprite{};

			metasprites[i].colliders = &colliderMemory[i * metaspriteMaxColliderCount];
			colliderMemory[i * metaspriteMaxColliderCount] = Collision::Collider{};
		}
	}

	void LoadMetasprites(const char* fname) {
		FILE* pFile;
		fopen_s(&pFile, fname, "rb");

		if (pFile == NULL) {
			ERROR("Failed to load metasprite file\n");
		}

		const char signature[4]{};
		fread((void*)signature, sizeof(u8), 4, pFile);
		
		for (u32 i = 0; i < maxMetaspriteCount; i++) {
			fread(&metasprites[i].spriteCount, sizeof(u32), 1, pFile);
			fread(&metasprites[i].colliderCount, sizeof(u32), 1, pFile);
		}

		fread(nameMemory, metaspriteMaxNameLength, maxMetaspriteCount, pFile);
		fread(spriteMemory, metaspriteMaxSpriteCount, maxMetaspriteCount, pFile);
		fread(colliderMemory, metaspriteMaxColliderCount, maxMetaspriteCount, pFile);

		fclose(pFile);

		// Init references
		for (u32 i = 0; i < maxMetaspriteCount; i++) {
			metasprites[i].name = &nameMemory[i * metaspriteMaxNameLength];
			metasprites[i].spritesRelativePos = &spriteMemory[i * metaspriteMaxSpriteCount];
			metasprites[i].colliders = &colliderMemory[i * metaspriteMaxColliderCount];
		}
	}

	void SaveMetasprites(const char* fname) {
		FILE* pFile;
		fopen_s(&pFile, fname, "wb");

		if (pFile == NULL) {
			ERROR("Failed to write metasprite file\n");
		}

		const char signature[4] = "SPR";
		fwrite(signature, sizeof(u8), 4, pFile);
		
		for (u32 i = 0; i < maxMetaspriteCount; i++) {
			fwrite(&metasprites[i].spriteCount, sizeof(u32), 1, pFile);
			fwrite(&metasprites[i].colliderCount, sizeof(u32), 1, pFile);
		}

		fwrite(nameMemory, metaspriteMaxNameLength, maxMetaspriteCount, pFile);
		fwrite(spriteMemory, metaspriteMaxSpriteCount, maxMetaspriteCount, pFile);
		fwrite(colliderMemory, metaspriteMaxColliderCount, maxMetaspriteCount, pFile);

		fclose(pFile);
	}
}