#pragma once
#include "editor_core.h"
#include "rendering.h"

namespace Editor {
	namespace Metasprite {
		
		struct Metasprite {
			const char* name;
			u32 spriteCount;
			Rendering::Sprite* spritesRelativePos;
		};

		void DrawPreviewWindow(EditorContext* pContext, Metasprite* pMetasprite);
	}
}