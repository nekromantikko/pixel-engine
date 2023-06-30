#pragma once
#include "rendering.h"
#include "typedef.h"

namespace Rendering
{
	namespace Util
	{
		struct Tile {
			u8 pixels[64];
		};

		void CreateChrSheet(const char* fname, u8* outBytes);
	}
}