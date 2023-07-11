#include "collision.h"
#include "rendering.h"

namespace Collision {

	TileCollision bgTileCollision[256]{};

	TileCollision* GetBgCollisionPtr() {
		return bgTileCollision;
	}

}