#include "collision.h"
#include "rendering.h"

namespace Collision {

	TileCollision bgTileCollision[256]{};

	TileCollision* GetBgCollisionPtr() {
		return bgTileCollision;
	}

	void LoadBgCollision(const char* fname) {
		FILE* pFile;
		fopen_s(&pFile, fname, "rb");

		if (pFile == NULL) {
			ERROR("Failed to load tile collision file\n");
		}

		const char signature[4]{};
		fread((void*)signature, sizeof(u8), 4, pFile);
		fread((void*)bgTileCollision, sizeof(TileCollision), 256, pFile);

		fclose(pFile);
	}

	void SaveBgCollision(const char* fname) {
		FILE* pFile;
		fopen_s(&pFile, fname, "wb");

		if (pFile == NULL) {
			ERROR("Failed to write tile collision file\n");
		}

		const char signature[4] = "TIL";
		fwrite(signature, sizeof(u8), 4, pFile);
		fwrite(bgTileCollision, sizeof(TileCollision), 256, pFile);

		fclose(pFile);
	}
}