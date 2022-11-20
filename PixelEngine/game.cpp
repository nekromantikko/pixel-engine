#include "game.h"
#include "system.h"
#include "input.h"
#include <math.h>
#include <string.h>

namespace Game {
    r32 secondsElapsed = 0.0f;

    // FPS counter
    r32 time100f = 0;
    r32 fcount = 0;
    r32 fps = 0;
    char fpsString[30];

    // Input
    Input::ControllerState controllerState;
    Input::ControllerState controllerStatePrev;

    // Viewport
    struct Viewport {
        f32 x;
        f32 y;
        f32 w;
        f32 h;
    };
    Viewport viewport;
    f32 xCameraSpeed;
    f32 yCameraSpeed;

    enum EditMode {
        Scroll,
        Brush,
        Select,
    };
    EditMode mode = Brush;

    s32 xCursor, yCursor;
    u8 palette = 0;
    u8 clipboard[8 * 8]{};
    s32 xSelOffset, ySelOffset;
    Rendering::Sprite clipboardSprites[8 * 8]{};

    struct Screen {
        // Size of one nametable
        u8 tiles[NAMETABLE_SIZE]{};
    };

    struct Level {
        Screen* screens;
        u16 screenCount;
    };

    Level level;
#define HUD_TILE_COUNT 128

    // Sprite stufff
    Rendering::Sprite outSprites[48]{};
    Rendering::Sprite bowSprites[8] = {
        { 0, 0, 128, 1 }, { 0, 8, 129, 1 },
        { 8, 0, 144, 1 }, { 8, 8, 145, 1 },
        { 16, 0, 160, 1 }, { 16, 8, 161, 1 },
        { 24, 0, 176, 1 }, { 24, 8, 177, 1 },
    };
    Rendering::CompoundSprite bow{
        bowSprites,
        8
    };
    s32 xBowOffset = 17, yBowOffset = -4;
    Rendering::Sprite characterSprites[16] = {
        { 0, 0, 200, 1 }, { 0, 8, 201, 1 }, { 0, 16, 202, 1 }, { 0, 24, 203, 1 },
        { 8, 0, 216, 1 }, { 8, 8, 217, 1 }, { 8, 16, 218, 1 }, { 8, 24, 219, 1 },
        { 16, 0, 232, 1 }, { 16, 8, 233, 1 }, { 16, 16, 234, 1 }, { 16, 24, 235, 1 },
        { 24, 0, 248, 1 }, { 24, 8, 249, 1 }, { 24, 16, 250, 1 }, { 24, 24, 251, 1 },
    };
    Rendering::CompoundSprite character {
        characterSprites,
        16
    };
    s32 xCharacter = 256, yCharacter = 144;
    Rendering::Sprite lWing0Sprites[8] = {
        { 0, 0, 136, 1 }, { 0, 8, 137, 1 },
        { 8, 0, 152, 1 }, { 8, 8, 153, 1 },
        { 16, 0, 168, 1 }, { 16, 8, 169, 1 },
        { 24, 0, 184, 1 }, { 24, 8, 185, 1 },
    };
    Rendering::CompoundSprite lWing0{
        lWing0Sprites,
        8
    };
    Rendering::Sprite lWing1Sprites[8] = {
        { 0, 0, 138, 1 }, { 0, 8, 139, 1 },
        { 8, 0, 154, 1 }, { 8, 8, 155, 1 },
        { 16, 0, 170, 1 }, { 16, 8, 171, 1 },
        { 24, 0, 186, 1 }, { 24, 8, 187, 1 },
    };
    Rendering::CompoundSprite lWing1{
        lWing1Sprites,
        8
    };
    Rendering::Sprite lWing2Sprites[8] = {
        { 0, 0, 140, 1 }, { 0, 8, 141, 1 },
        { 8, 0, 156, 1 }, { 8, 8, 157, 1 },
        { 16, 0, 172, 1 }, { 16, 8, 173, 1 },
        { 24, 0, 188, 1 }, { 24, 8, 189, 1 },
    };
    Rendering::CompoundSprite lWing2{
        lWing2Sprites,
        8
    };
    Rendering::Sprite lWing3Sprites[8] = {
        { 0, 0, 142, 1 }, { 0, 8, 143, 1 },
        { 8, 0, 158, 1 }, { 8, 8, 159, 1 },
        { 16, 0, 174, 1 }, { 16, 8, 175, 1 },
        { 24, 0, 190, 1 }, { 24, 8, 191, 1 },
    };
    Rendering::CompoundSprite lWing3{
        lWing3Sprites,
        8
    };
    Rendering::CompoundSprite lWingFrames[4] = {
        lWing0,
        lWing1,
        lWing2,
        lWing3
    };
    Rendering::AnimatedSprite lWing{
        lWingFrames,
        4,
        180
    };
    s32 xLWingOffset = -3, yLWingOffset = -3;
    Rendering::Sprite rWing0Sprites[8] = {
        { 0, 0, 137, 0x42 }, { 0, 8, 136, 0x42 },
        { 8, 0, 153, 0x42 }, { 8, 8, 152, 0x42 },
        { 16, 0, 169, 0x42 }, { 16, 8, 168, 0x42 },
        { 24, 0, 185, 0x42 }, { 24, 8, 184, 0x42 },
    };
    Rendering::CompoundSprite rWing0{
        rWing0Sprites,
        8
    };
    Rendering::Sprite rWing1Sprites[8] = {
        { 0, 0, 139, 0x42 }, { 0, 8, 138, 0x42 },
        { 8, 0, 155, 0x42 }, { 8, 8, 154, 0x42 },
        { 16, 0, 171, 0x42 }, { 16, 8, 170, 0x42 },
        { 24, 0, 187, 0x42 }, { 24, 8, 186, 0x42 },
    };
    Rendering::CompoundSprite rWing1{
        rWing1Sprites,
        8
    };
    Rendering::Sprite rWing2Sprites[8] = {
        { 0, 0, 141, 0x42 }, { 0, 8, 140, 0x42 },
        { 8, 0, 157, 0x42 }, { 8, 8, 156, 0x42 },
        { 16, 0, 173, 0x42 }, { 16, 8, 172, 0x42 },
        { 24, 0, 189, 0x42 }, { 24, 8, 188, 0x42 },
    };
    Rendering::CompoundSprite rWing2{
        rWing2Sprites,
        8
    };
    Rendering::Sprite rWing3Sprites[8] = {
        { 0, 0, 143, 0x42 }, { 0, 8, 142, 0x42 },
        { 8, 0, 159, 0x42 }, { 8, 8, 158, 0x42 },
        { 16, 0, 175, 0x42 }, { 16, 8, 174, 0x42 },
        { 24, 0, 191, 0x42 }, { 24, 8, 190, 0x42 },
    };
    Rendering::CompoundSprite rWing3{
        rWing3Sprites,
        8
    };
    Rendering::CompoundSprite rWingFrames[4] = {
        rWing0,
        rWing1,
        rWing2,
        rWing3
    };
    Rendering::AnimatedSprite rWing{
        rWingFrames,
        4,
        180
    };
    s32 xRWingOffset = 16, yRWingOffset = -3;

	void Initialize(Rendering::RenderContext* pContext) {
        viewport.x = 0.0f;
        viewport.y = 96.0f;
        viewport.w = VIEWPORT_WIDTH_TILES * TILE_SIZE;
        viewport.h = VIEWPORT_HEIGHT_TILES * TILE_SIZE;

        xCursor = 0;
        yCursor = 15;

        xSelOffset = 1;
        ySelOffset = -1;

        clipboard[0] = 192;

        level.screenCount = 16;
        level.screens = (Screen*)malloc(sizeof(Screen) * level.screenCount);
        memset(level.screens, 0, sizeof(Screen) * level.screenCount);

        // Render all of first and second nametable
        for (int i = 0; i < NAMETABLE_COUNT; i++) {
            Rendering::UpdateNametable(pContext, i, NAMETABLE_SIZE - HUD_TILE_COUNT, HUD_TILE_COUNT, level.screens[i].tiles + HUD_TILE_COUNT);
        }
	}

    void SelectTiles() {
        u16 bottom = ySelOffset < 0 ? yCursor + ySelOffset : yCursor;
        u16 left = xSelOffset < 0 ? xCursor + xSelOffset : xCursor;
        u16 selectionWidth = abs(xSelOffset);
        u16 selectionHeight = abs(ySelOffset);
        for (int y = 0; y < selectionHeight; y++) {
            for (int x = 0; x < selectionWidth; x++) {
                u8 screenIndex = (left + x) / NAMETABLE_WIDTH_TILES;
                u16 screenRelativeX = (left + x) % NAMETABLE_WIDTH_TILES;
                u16 screenMemOffset = NAMETABLE_WIDTH_TILES * (bottom + y) + screenRelativeX;
                clipboard[x + y * selectionWidth] = level.screens[screenIndex].tiles[screenMemOffset];
            }
        }
    }

    void PaintTiles(Rendering::RenderContext* pContext) {
        u16 bottom = ySelOffset < 0 ? yCursor + ySelOffset : yCursor;
        u16 left = xSelOffset < 0 ? xCursor + xSelOffset : xCursor;
        u16 selectionWidth = abs(xSelOffset);
        u16 selectionHeight = abs(ySelOffset);
        for (int y = 0; y < selectionHeight; y++) {
            for (int x = 0; x < selectionWidth; x++) {
                u8 screenIndex = (left + x) / NAMETABLE_WIDTH_TILES;
                u16 screenRelativeX = (left + x) % NAMETABLE_WIDTH_TILES;
                u16 screenMemOffset = NAMETABLE_WIDTH_TILES * (bottom + y) + screenRelativeX;
                level.screens[screenIndex].tiles[screenMemOffset] = clipboard[x + y * selectionWidth];
                Rendering::UpdateNametable(pContext, screenIndex % NAMETABLE_COUNT, 1, screenMemOffset, &level.screens[screenIndex].tiles[screenMemOffset]);
            }
        }
    }

    void ChangePalette(Rendering::RenderContext* pContext) {
        u16 bottom = ySelOffset < 0 ? yCursor + ySelOffset : yCursor;
        u16 left = xSelOffset < 0 ? xCursor + xSelOffset : xCursor;
        u16 selectionWidth = abs(xSelOffset);
        u16 selectionHeight = abs(ySelOffset);
        for (int y = 0; y < selectionHeight; y++) {
            for (int x = 0; x < selectionWidth; x++) {
                u8 screenIndex = (left + x) / NAMETABLE_WIDTH_TILES;
                u16 screenRelativeX = (left + x) % NAMETABLE_WIDTH_TILES;
                u16 xBlock = screenRelativeX / 4;
                u16 yBlock = (bottom + y) / 4;
                u8 smallBlockOffset = (screenRelativeX % 4 / 2) + ((bottom + y) % 4 / 2) * 2;
                u16 blockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
                u16 nametableOffset = NAMETABLE_ATTRIBUTE_OFFSET + blockIndex;
                level.screens[screenIndex].tiles[nametableOffset] &= ~(0b11 << (smallBlockOffset * 2));
                level.screens[screenIndex].tiles[nametableOffset] |= (palette << (smallBlockOffset * 2));
                Rendering::UpdateNametable(pContext, screenIndex % NAMETABLE_COUNT, 1, nametableOffset, &level.screens[screenIndex].tiles[nametableOffset]);
            }
        }
    }

    void UpdateHUD(Rendering::RenderContext* pContext) {
        u8 fpsLabel[] = { 
            fpsString[0],fpsString[1],fpsString[2],fpsString[3],0,'F','P','S'
        };
        Rendering::UpdateNametable(pContext, 0, 8, 1, fpsLabel);
    }

    void RenderHUD(Rendering::RenderContext* pContext) {
        Rendering::RenderState state = {
            0,
            0,
            0,
            1
        };
        Rendering::SetRenderState(pContext, 0, 16, state);
    }

    u8 GetSelectionTileIndex(u32 x, u32 y, u32 w, u32 h) {
        u8 offset = 16;
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

    u32 DrawDebugCharacter(Rendering::RenderContext* pContext, u32 spriteOffset, s32 x, s32 y, float time) {
        u32 spriteCount = 0;
        u32 msElapsed = time * 1000;
        s32 vOffset = (msElapsed / 360) % 2 ? -1 : 0;
        u32 lWingFrame = (msElapsed / lWing.frameLength) % lWing.frameCount;
        for (int i = 0; i < lWing.frames[lWingFrame].spriteCount; i++) {
            Rendering::Sprite offsetSprite = {
                lWing.frames[lWingFrame].sprites[i].y + y + vOffset + yLWingOffset - viewport.y,
                lWing.frames[lWingFrame].sprites[i].x + x + xLWingOffset - viewport.x,
                lWing.frames[lWingFrame].sprites[i].tileId,
                lWing.frames[lWingFrame].sprites[i].attributes
            };
            outSprites[i + spriteCount] = offsetSprite;
        }
        spriteCount += lWing0.spriteCount;

        u32 rWingFrame = (msElapsed / rWing.frameLength) % rWing.frameCount;
        for (int i = 0; i < rWing.frames[rWingFrame].spriteCount; i++) {
            Rendering::Sprite offsetSprite = {
                rWing.frames[rWingFrame].sprites[i].y + y + vOffset + yRWingOffset - viewport.y,
                rWing.frames[rWingFrame].sprites[i].x + x + xRWingOffset - viewport.x,
                rWing.frames[rWingFrame].sprites[i].tileId,
                rWing.frames[rWingFrame].sprites[i].attributes
            };
            outSprites[i + spriteCount] = offsetSprite;
        }
        spriteCount += rWing0.spriteCount;

        for (int i = 0; i < character.spriteCount; i++) {
            Rendering::Sprite offsetSprite = {
                character.sprites[i].y + y + vOffset - viewport.y,
                character.sprites[i].x + x - viewport.x,
                character.sprites[i].tileId,
                character.sprites[i].attributes
            };
            outSprites[i + spriteCount] = offsetSprite;
        }
        spriteCount += character.spriteCount;

        for (int i = 0; i < bow.spriteCount; i++) {
            Rendering::Sprite offsetSprite = {
                bow.sprites[i].y + y + vOffset + yBowOffset - viewport.y,
                bow.sprites[i].x + x + xBowOffset - viewport.x,
                bow.sprites[i].tileId,
                bow.sprites[i].attributes
            };
            outSprites[i + spriteCount] = offsetSprite;
        }
        spriteCount += bow.spriteCount;

        Rendering::SetSprites(pContext, spriteCount, spriteOffset, outSprites);
        return spriteCount;
    }

    void Render(Rendering::RenderContext* pContext) {
        Rendering::SetCurrentTime(pContext, secondsElapsed);

        Rendering::BeginDraw(pContext);

        // Rendering::ClearSprites(pContext, 256);

        u32 spriteOffset = 0;
        /*for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                spriteOffset += DrawDebugCharacter(pContext, spriteOffset, xCharacter + x * 32, yCharacter + y * 32, secondsElapsed + x*0.1 + y*0.8);
            }
        }*/
        spriteOffset += DrawDebugCharacter(pContext, spriteOffset, xCharacter, yCharacter, secondsElapsed);

        // Level editor sprites:
        if (mode != Scroll) {
            Rendering::ClearSprites(pContext, spriteOffset, 64);
            u16 bottom = ySelOffset < 0 ? yCursor + ySelOffset : yCursor;
            u16 left = xSelOffset < 0 ? xCursor + xSelOffset : xCursor;
            u16 selectionWidth = abs(xSelOffset);
            u16 selectionHeight = abs(ySelOffset);
            s32 xStart = (left * 8) - viewport.x;
            s32 yStart = (bottom * 8) - viewport.y;
            for (int y = 0; y < selectionHeight; y++) {
                for (int x = 0; x < selectionWidth; x++) {
                    clipboardSprites[x + y * selectionWidth] = {
                        yStart + 8 * y,
                        xStart + 8 * x,
                        mode == Brush ? clipboard[x + y * selectionWidth] : GetSelectionTileIndex(x,y,selectionWidth,selectionHeight),
                        palette
                    };
                }
            }
            Rendering::SetSprites(pContext, selectionWidth * selectionHeight, spriteOffset, clipboardSprites);
        }

        UpdateHUD(pContext);
        RenderHUD(pContext);

        /*for (int i = 0; i < 272; i++) {
            float sine = sin(secondsElapsed + (i / 8.0f));
            Rendering::RenderState state = {
                viewport.x + (sine * 8),
                viewport.y,
                0,
                mode == Scroll ? 1 : 0
            };
            Rendering::SetRenderState(pContext, 16 + i, 1, state);
        }*/

        Rendering::RenderState state = {
                viewport.x,
                viewport.y,
                0,
                mode == Scroll ? 1 : 0
        };
        Rendering::SetRenderState(pContext, 16, 272, state);
        Rendering::ExecuteHardcodedCommands(pContext);

        Rendering::EndDraw(pContext);
    }

    void HandleScrollInput() {
        if ((controllerState & Input::ControllerState::Right)) {
            xCameraSpeed = 128.0f;
        }
        if ((controllerState & Input::ControllerState::Left)) {
            xCameraSpeed = -128.0f;
        }
        if ((controllerState & Input::ControllerState::Up)) {
            yCameraSpeed = -128.0f;
        }
        if ((controllerState & Input::ControllerState::Down)) {
            yCameraSpeed = 128.0f;
        }
    }

    void HandleBrushInput(Rendering::RenderContext* pContext) {
        if ((controllerState & Input::ControllerState::Right) && !(controllerStatePrev & Input::ControllerState::Right)) {
            if (controllerState & Input::ControllerState::Select) {
                for (int i = 0; i < abs(xSelOffset * ySelOffset); i++) {
                    if (clipboard[i] % 16 == 15) {
                        clipboard[i] -= 15;
                    }
                    else clipboard[i] += 1;
                }
            }
            else xCursor++;
        }
        if ((controllerState & Input::ControllerState::Left) && !(controllerStatePrev & Input::ControllerState::Left)) {
            if (controllerState & Input::ControllerState::Select) {
                for (int i = 0; i < abs(xSelOffset * ySelOffset); i++) {
                    if (clipboard[i] % 16 == 0) {
                        clipboard[i] += 15;
                    }
                    else clipboard[i] -= 1;
                }
            }
            else if (--xCursor < 0) {
                xCursor = 0;
            };
        }
        if ((controllerState & Input::ControllerState::Up) && !(controllerStatePrev & Input::ControllerState::Up)) {
            if (controllerState & Input::ControllerState::Select) {
                mode = Brush;
                for (int i = 0; i < abs(xSelOffset * ySelOffset); i++) {
                    clipboard[i] -= 16;
                }
            }
            else if (--yCursor == 0) {
                yCursor = 0;
            };
        }
        if ((controllerState & Input::ControllerState::Down) && !(controllerStatePrev & Input::ControllerState::Down)) {
            if (controllerState & Input::ControllerState::Select) {
                mode = Brush;
                for (int i = 0; i < abs(xSelOffset * ySelOffset); i++) {
                    clipboard[i] += 16;
                }
            }
            else yCursor++;
        }
        if ((controllerState & Input::ControllerState::A) && !(controllerStatePrev & Input::ControllerState::A)) {
            if ((controllerState & Input::ControllerState::Select)) {
                palette = --palette % 4;
            }
            else PaintTiles(pContext);
        }
        if ((controllerState & Input::ControllerState::B) && !(controllerStatePrev & Input::ControllerState::B)) {
            if ((controllerState & Input::ControllerState::Select)) {
                palette = ++palette % 4;
            }
            else ChangePalette(pContext);
        }
    }

    void HandleSelectInput() {
        if ((controllerState & Input::ControllerState::Right) && !(controllerStatePrev & Input::ControllerState::Right)) {
            if (++xSelOffset > 8) {
                xSelOffset = 8;
            }
            if (xSelOffset == 0) {
                xSelOffset = -1;
            }
        }
        if ((controllerState & Input::ControllerState::Left) && !(controllerStatePrev & Input::ControllerState::Left)) {
            if (--xSelOffset < -8) {
                xSelOffset = -8;
            }
            if (xSelOffset == 0) {
                xSelOffset = 1;
            }
        }
        if ((controllerState & Input::ControllerState::Up) && !(controllerStatePrev & Input::ControllerState::Up)) {
            if (--ySelOffset < -8) {
                ySelOffset = -8;
            }
            if (ySelOffset == 0) {
                ySelOffset = -1;
            }
        }
        if ((controllerState & Input::ControllerState::Down) && !(controllerStatePrev & Input::ControllerState::Down)) {
            if (++ySelOffset > 8) {
                ySelOffset = 8;
            }
            if (ySelOffset == 0) {
                ySelOffset = 1;
            }
        }
        if ((controllerState & Input::ControllerState::A)) {
            SelectTiles();
            mode = Brush;
        }
    }

    void Step(float dt, Rendering::RenderContext* pContext) {
        secondsElapsed += dt;
        xCameraSpeed = 0.0f;
        yCameraSpeed = 0.0f;

        // Poll input
        controllerState = Input::PollInput(controllerState);
        switch (mode) {
        case Scroll:
            HandleScrollInput();
            break;
        case Brush:
            HandleBrushInput(pContext);
            break;
        case Select:
            HandleSelectInput();
            break;
        default:
            break;
        }
        
        if ((controllerState & Input::ControllerState::Start) && !(controllerStatePrev & Input::ControllerState::Start)) {
            mode = (EditMode)((mode + 1) % 3);
            if (xCursor < viewport.x / 8) {
                xCursor = viewport.x / 8;
            }
            if (xCursor >= viewport.x / 8 + VIEWPORT_WIDTH_TILES) {
                xCursor = viewport.x / 8 + (VIEWPORT_WIDTH_TILES - 1);
            }
        }

        controllerStatePrev = controllerState;

        f32 xPrevious = viewport.x;
        viewport.x += dt * xCameraSpeed;
        if (viewport.x < 0.0f) {
            viewport.x = 0.0f;
            xCameraSpeed = 0.0f;
        } else if (viewport.x + viewport.w >= level.screenCount * NAMETABLE_WIDTH_TILES * TILE_SIZE) {
            viewport.x = (level.screenCount * NAMETABLE_WIDTH_TILES * TILE_SIZE) - viewport.w;
            xCameraSpeed = 0.0f;
        }

        viewport.y += dt * yCameraSpeed;
        if (viewport.y < 0.0f) {
            viewport.y = 0.0f;
            yCameraSpeed = 0.0f;
        }
        else if (viewport.y + viewport.h >= NAMETABLE_HEIGHT_TILES * TILE_SIZE) {
            viewport.y = (NAMETABLE_HEIGHT_TILES * TILE_SIZE) - viewport.h;
            yCameraSpeed = 0.0f;
        }

        // TODO: Update nametables when scrolling
        bool crossedBlockBoundary = ((s32)xPrevious % (TILE_SIZE * 2)) != ((s32)viewport.x % (TILE_SIZE * 2));
        float bufferWidth = 128.0f;
        if (xCameraSpeed != 0 && crossedBlockBoundary) {
            u32 leftBlockIndex = (u32)((viewport.x - bufferWidth) / (TILE_SIZE * 2));
            u32 leftScreenIndex = leftBlockIndex / (NAMETABLE_WIDTH_TILES / 2);
            u32 leftScreenBlockOffset = leftBlockIndex % (NAMETABLE_WIDTH_TILES / 2);
            u32 leftScreenTileOffset = leftScreenBlockOffset * 2;
            u32 rightBlockIndex = (u32)((viewport.x + bufferWidth + viewport.w) / (TILE_SIZE * 2));
            u32 rightScreenIndex = rightBlockIndex / (NAMETABLE_WIDTH_TILES / 2);
            u32 rightScreenBlockOffset = rightBlockIndex % (NAMETABLE_WIDTH_TILES / 2);
            u32 rightScreenTileOffset = rightScreenBlockOffset * 2;

            for (int i = 0; i < NAMETABLE_HEIGHT_TILES; i++) {
                if (leftScreenIndex < level.screenCount) {
                    u32 leftOffset = NAMETABLE_WIDTH_TILES * i + leftScreenTileOffset;
                    u32 xBlock = leftScreenTileOffset / 4;
                    u32 yBlock = i / 4;
                    u32 attributeBlockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
                    Rendering::UpdateNametable(pContext, leftScreenIndex % NAMETABLE_COUNT, 2, leftOffset, &level.screens[leftScreenIndex].tiles[leftOffset]);
                    Rendering::UpdateNametable(pContext, leftScreenIndex % NAMETABLE_COUNT, 1, NAMETABLE_ATTRIBUTE_OFFSET + attributeBlockIndex, &level.screens[leftScreenIndex].tiles[NAMETABLE_ATTRIBUTE_OFFSET + attributeBlockIndex]);
                }
                if (rightScreenIndex < level.screenCount) {
                    u32 rightOffset = NAMETABLE_WIDTH_TILES * i + rightScreenTileOffset;
                    u32 xBlock = rightScreenTileOffset / 4;
                    u32 yBlock = i / 4;
                    u32 attributeBlockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
                    Rendering::UpdateNametable(pContext, rightScreenIndex % NAMETABLE_COUNT, 2, rightOffset, &level.screens[rightScreenIndex].tiles[rightOffset]);
                    Rendering::UpdateNametable(pContext, rightScreenIndex % NAMETABLE_COUNT, 1, NAMETABLE_ATTRIBUTE_OFFSET + attributeBlockIndex, &level.screens[rightScreenIndex].tiles[NAMETABLE_ATTRIBUTE_OFFSET + attributeBlockIndex]);
                }
            }
        }

        // FPS averaged over 30 frames
        if (fcount == 100) {
            fps = 100.0 / time100f;
            time100f = 0;
            fcount = 0;
        }
        else {
            time100f += dt;
            fcount++;
        }

        fpsString[0] = ' ';
        fpsString[1] = ' ';
        fpsString[2] = ' ';
        fpsString[3] = ' ';
        _itoa_s((int)fps, fpsString, 30, 10);

        Render(pContext);
    }
}