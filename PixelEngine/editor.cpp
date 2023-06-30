#include "editor.h"
#include "system.h"
#include <stdio.h>

namespace Editor {
    u8 GetSelectionTileIndex(u32 x, u32 y, u32 w, u32 h) {
        u8 offset = 0x10;
        u8 xIndex = 0;
        u8 yIndex = 0;

        if (w != 1) {
            xIndex = 0b01;
            xIndex <<= (x % w != 0) ? 1 : 0;
            xIndex += (x % w == w - 1) ? 1 : 0;
        }
        if (h != 1) {
            yIndex = 0b01;
            yIndex <<= (y % h != 0) ? 1 : 0;
            yIndex += (y % h == h - 1) ? 1 : 0;
        }

        u8 index = xIndex + (yIndex << 2);
        return index + offset;
    }

    void SelectTiles(EditorState *pState) {
        u16 bottom = pState->ySelOffset < 0 ? pState->yCursor + pState->ySelOffset : pState->yCursor;
        u16 left = pState->xSelOffset < 0 ? pState->xCursor + pState->xSelOffset : pState->xCursor;
        u16 selectionWidth = abs(pState->xSelOffset);
        u16 selectionHeight = abs(pState->ySelOffset);
        for (int y = 0; y < selectionHeight; y++) {
            for (int x = 0; x < selectionWidth; x++) {
                u8 screenIndex = (left + x) / NAMETABLE_WIDTH_TILES;
                u16 screenRelativeX = (left + x) % NAMETABLE_WIDTH_TILES;
                u16 screenMemOffset = NAMETABLE_WIDTH_TILES * (bottom + y) + screenRelativeX;
                pState->clipboard[x + y * selectionWidth] = pState->pLevel->screens[screenIndex].tiles[screenMemOffset];
            }
        }
    }

    void PaintTiles(EditorState* pState) {
        u16 bottom = pState->ySelOffset < 0 ? pState->yCursor + pState->ySelOffset : pState->yCursor;
        u16 left = pState->xSelOffset < 0 ? pState->xCursor + pState->xSelOffset : pState->xCursor;
        u16 selectionWidth = abs(pState->xSelOffset);
        u16 selectionHeight = abs(pState->ySelOffset);
        for (int y = 0; y < selectionHeight; y++) {
            for (int x = 0; x < selectionWidth; x++) {
                u8 screenIndex = (left + x) / NAMETABLE_WIDTH_TILES;
                u16 screenRelativeX = (left + x) % NAMETABLE_WIDTH_TILES;
                u16 screenMemOffset = NAMETABLE_WIDTH_TILES * (bottom + y) + screenRelativeX;
                pState->pLevel->screens[screenIndex].tiles[screenMemOffset] = pState->clipboard[x + y * selectionWidth];
                Rendering::WriteNametable(pState->pRenderContext, screenIndex % NAMETABLE_COUNT, 1, screenMemOffset, &pState->pLevel->screens[screenIndex].tiles[screenMemOffset]);
            }
        }
    }

    void ChangePalette(EditorState* pState) {
        u16 bottom = pState->ySelOffset < 0 ? pState->yCursor + pState->ySelOffset : pState->yCursor;
        u16 left = pState->xSelOffset < 0 ? pState->xCursor + pState->xSelOffset : pState->xCursor;
        u16 selectionWidth = abs(pState->xSelOffset);
        u16 selectionHeight = abs(pState->ySelOffset);
        for (int y = 0; y < selectionHeight; y++) {
            for (int x = 0; x < selectionWidth; x++) {
                u8 screenIndex = (left + x) / NAMETABLE_WIDTH_TILES;
                u16 screenRelativeX = (left + x) % NAMETABLE_WIDTH_TILES;
                u16 xBlock = screenRelativeX / 4;
                u16 yBlock = (bottom + y) / 4;
                u8 smallBlockOffset = (screenRelativeX % 4 / 2) + ((bottom + y) % 4 / 2) * 2;
                u16 blockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
                u16 nametableOffset = NAMETABLE_ATTRIBUTE_OFFSET + blockIndex;
                pState->pLevel->screens[screenIndex].tiles[nametableOffset] &= ~(0b11 << (smallBlockOffset * 2));
                pState->pLevel->screens[screenIndex].tiles[nametableOffset] |= (pState->palette << (smallBlockOffset * 2));
                Rendering::WriteNametable(pState->pRenderContext, screenIndex % NAMETABLE_COUNT, 1, nametableOffset, &pState->pLevel->screens[screenIndex].tiles[nametableOffset]);
                DEBUG_LOG("Writing tile %#x to location %#x\n", pState->pLevel->screens[screenIndex].tiles[nametableOffset], nametableOffset);
            }
        }
    }

    void DrawSelection(EditorState* pState, Viewport *viewport, u32 spriteOffset) {
        Rendering::ClearSprites(pState->pRenderContext, spriteOffset, 64);
        u16 bottom = pState->ySelOffset < 0 ? pState->yCursor + pState->ySelOffset : pState->yCursor;
        u16 left = pState->xSelOffset < 0 ? pState->xCursor + pState->xSelOffset : pState->xCursor;
        u16 selectionWidth = abs(pState->xSelOffset);
        u16 selectionHeight = abs(pState->ySelOffset);
        s32 xStart = (left * 8) - viewport->x;
        s32 yStart = (bottom * 8) - viewport->y;
        for (int y = 0; y < selectionHeight; y++) {
            for (int x = 0; x < selectionWidth; x++) {
                pState->clipboardSprites[x + y * selectionWidth] = {
                    yStart + 8 * y,
                    xStart + 8 * x,
                    pState->mode == Brush ? pState->clipboard[x + y * selectionWidth] : GetSelectionTileIndex(x,y,selectionWidth,selectionHeight),
                    pState->palette
                };
            }
        }
        Rendering::WriteSprites(pState->pRenderContext, selectionWidth * selectionHeight, spriteOffset, pState->clipboardSprites);
    }

    void HandleScrollInput(EditorState* pState, Input::ControllerState input, Input::ControllerState prevInput, Viewport *viewport, float dt) {
        float dx = 0, dy = 0;

        if ((input & Input::ControllerState::Right)) {
            dx = 128.0f;
        }
        if ((input & Input::ControllerState::Left)) {
            dx = -128.0f;
        }
        if ((input & Input::ControllerState::Up)) {
            dy = -128.0f;
        }
        if ((input & Input::ControllerState::Down)) {
            dy = 128.0f;
        }

        dx *= dt;
        dy *= dt;

        MoveViewport(viewport, pState->pRenderContext, pState->pLevel, dx, dy);
    }

    void HandleBrushInput(EditorState* pState, Input::ControllerState input, Input::ControllerState prevInput) {
        if ((input & Input::ControllerState::Right) && !(prevInput & Input::ControllerState::Right)) {
            if (input & Input::ControllerState::Select) {
                for (int i = 0; i < abs(pState->xSelOffset * pState->ySelOffset); i++) {
                    if (pState->clipboard[i] % 16 == 15) {
                        pState->clipboard[i] -= 15;
                    }
                    else pState->clipboard[i] += 1;
                }
            }
            else pState->xCursor++;
        }
        if ((input & Input::ControllerState::Left) && !(prevInput & Input::ControllerState::Left)) {
            if (input & Input::ControllerState::Select) {
                for (int i = 0; i < abs(pState->xSelOffset * pState->ySelOffset); i++) {
                    if (pState->clipboard[i] % 16 == 0) {
                        pState->clipboard[i] += 15;
                    }
                    else pState->clipboard[i] -= 1;
                }
            }
            else if (--pState->xCursor < 0) {
                pState->xCursor = 0;
            };
        }
        if ((input & Input::ControllerState::Up) && !(prevInput & Input::ControllerState::Up)) {
            if (input & Input::ControllerState::Select) {
                pState->mode = Brush;
                for (int i = 0; i < abs(pState->xSelOffset * pState->ySelOffset); i++) {
                    pState->clipboard[i] -= 16;
                }
            }
            else if (--pState->yCursor == 0) {
                pState->yCursor = 0;
            };
        }
        if ((input & Input::ControllerState::Down) && !(prevInput & Input::ControllerState::Down)) {
            if (input & Input::ControllerState::Select) {
                pState->mode = Brush;
                for (int i = 0; i < abs(pState->xSelOffset * pState->ySelOffset); i++) {
                    pState->clipboard[i] += 16;
                }
            }
            else pState->yCursor++;
        }
        if ((input & Input::ControllerState::A) && !(prevInput & Input::ControllerState::A)) {
            if ((input & Input::ControllerState::Select)) {
                pState->palette = --pState->palette % 4;
            }
            else PaintTiles(pState);
        }
        if ((input & Input::ControllerState::B) && !(prevInput & Input::ControllerState::B)) {
            if ((input & Input::ControllerState::Select)) {
                pState->palette = ++pState->palette % 4;
            }
            else ChangePalette(pState);
        }
    }

    void HandleSelectInput(EditorState* pState, Input::ControllerState input, Input::ControllerState prevInput) {
        if ((input & Input::ControllerState::Right) && !(prevInput & Input::ControllerState::Right)) {
            if (++pState->xSelOffset > 8) {
                pState->xSelOffset = 8;
            }
            if (pState->xSelOffset == 0) {
                pState->xSelOffset = 1;
            }
        }
        if ((input & Input::ControllerState::Left) && !(prevInput & Input::ControllerState::Left)) {
            if (--pState->xSelOffset < -8) {
                pState->xSelOffset = -8;
            }
            if (pState->xSelOffset == 0) {
                pState->xSelOffset = -1;
            }
        }
        if ((input & Input::ControllerState::Up) && !(prevInput & Input::ControllerState::Up)) {
            if (--pState->ySelOffset < -8) {
                pState->ySelOffset = -8;
            }
            if (pState->ySelOffset == 0) {
                pState->ySelOffset = -1;
            }
        }
        if ((input & Input::ControllerState::Down) && !(prevInput & Input::ControllerState::Down)) {
            if (++pState->ySelOffset > 8) {
                pState->ySelOffset = 8;
            }
            if (pState->ySelOffset == 0) {
                pState->ySelOffset = 1;
            }
        }
        if ((input & Input::ControllerState::A)) {
            SelectTiles(pState);
            pState->mode = Brush;
        }
    }

    void HandleSaveInput(EditorState* pState, Input::ControllerState input, Input::ControllerState prevInput) {
        if ((input & Input::ControllerState::Select) && !(prevInput & Input::ControllerState::Select)) {
            SaveLevel(pState->pLevel, pState->pLevel->name);
        }
    }

    void HandleInput(EditorState* pState, Input::ControllerState input, Input::ControllerState prevInput, Viewport *viewport, float dt) {
        switch (pState->mode) {
        case Scroll:
            HandleScrollInput(pState, input, prevInput, viewport, dt);
            break;
        case Brush:
            HandleBrushInput(pState, input, prevInput);
            break;
        case Select:
            HandleSelectInput(pState, input, prevInput);
            break;
        case Save:
            HandleSaveInput(pState, input, prevInput);
        default:
            break;
        }

        if ((input & Input::ControllerState::Start) && !(prevInput & Input::ControllerState::Start)) {
            pState->mode = (EditorMode)((pState->mode + 1) % 4);
            if (pState->xCursor < viewport->x / 8) {
                pState->xCursor = viewport->x / 8;
            }
            if (pState->xCursor >= viewport->x / 8 + VIEWPORT_WIDTH_TILES) {
                pState->xCursor = viewport->x / 8 + (VIEWPORT_WIDTH_TILES - 1);
            }
        }
    }

    // UTILS

    const char* GetEditorModeName(EditorMode mode) {
        switch (mode) {
        case Scroll:
            return "SCROLL";
        case Brush:
            return "BRUSH";
        case Select:
            return "SELECT";
        case Save:
            return "SAVE";
        default:
            return "UNKNOWN";
        }
    }
}