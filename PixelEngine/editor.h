#pragma once
#include "rendering.h"
#include "viewport.h"
#include "input.h"
#include "level.h"

namespace Editor {
    enum EditorMode {
        Scroll,
        Brush,
        Select,
        Save
    };
    
	struct EditorState {
        Rendering::RenderContext* pRenderContext;
        Level* pLevel;

        EditorMode mode = Brush;

        s32 xCursor = 0, yCursor = 15;
        u8 palette = 0;
        u8 clipboard[8 * 8]{ 0x80 };
        s32 xSelOffset = 1, ySelOffset = -1;
        Rendering::Sprite clipboardSprites[8 * 8]{};
	};

    void DrawSelection(EditorState* state, Viewport *viewport, u32 spriteOffset);
    void HandleInput(EditorState* pState, Input::ControllerState input, Input::ControllerState prevInput, Viewport *viewport, float dt);
    const char *GetEditorModeName(EditorMode mode);
}