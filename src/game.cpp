#include "game.h"
#include "system.h"
#include "game_input.h"
#include "game_state.h"
#include <cstring>
#include <cstdio>
#include "game_rendering.h"
#include "collision.h"
#include "metasprite.h"
#include "tiles.h"
#include <vector>
#include "audio.h"
#include "nes_timing.h"
#include <gtc/constants.hpp>
#include "random.h"
#include "fixed_hash_map.h"
#include "coroutines.h"
#include "dialog.h"
#include "game_ui.h"
#include "actor_prototypes.h"
#include "dungeon.h"
#include "room.h"
#include "overworld.h"
#include "asset_manager.h"

// TEMP
constexpr DungeonHandle testDungeon(4648186456448694858);

namespace Game {
    r64 secondsElapsed = 0.0f;
    u32 clockCounter = 0;

    static void Step() {
        StepFrame();

        // Animate color palette hue
        /*s32 hueShift = (s32)glm::roundEven(gameplayFramesElapsed / 12.f);
        for (u32 i = 0; i < PALETTE_MEMORY_SIZE; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            s32 hue = baseColor & 0b1111;

            s32 newHue = hue + hueShift;
            newHue &= 0b1111;

            u8 newColor = (baseColor & 0b1110000) | newHue;
            ((u8*)pPalettes)[i] = newColor;
        }*/
    }

#pragma region Public API
    void Initialize() {
        AssetManager::LoadArchive("assets.npak");

        Rendering::Init();

		InitGameData();
        InitGameState(GAME_STATE_DUNGEON);

        LoadRoom(testDungeon, { 14, 14 });
    }

    void Free() {}

    void Update(r64 dt) {
        secondsElapsed += dt;
        while (secondsElapsed >= CLOCK_PERIOD) {
            clockCounter++;
            secondsElapsed -= CLOCK_PERIOD;

            // One frame
            if (clockCounter == FRAME_CLOCK) {
                Step();
                clockCounter = 0;
            }
        }
    }
#pragma endregion
}