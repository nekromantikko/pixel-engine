#include "game.h"
#include "game_input.h"
#include "game_state.h"
#include "game_rendering.h"
#include "nes_timing.h"
#include "asset_manager.h"

static GameConfig g_config;

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
        Rendering::Init();

		g_config = {
			.uiBankHandle = AssetManager::GetAssetHandle<ChrBankHandle>("chr_sheets/ui.bmp"),
			.mapBankHandle = AssetManager::GetAssetHandle<ChrBankHandle>("chr_sheets/map_tiles.bmp"),
			.playerPrototypeHandle = AssetManager::GetAssetHandle<ActorPrototypeHandle>("actor_prototypes/freya.actor"),
			.playerOverworldPrototypeHandle = AssetManager::GetAssetHandle<ActorPrototypeHandle>("actor_prototypes/freya_overworld.actor"),
			.xpRemnantPrototypeHandle = AssetManager::GetAssetHandle<ActorPrototypeHandle>("actor_prototypes/exp_remnant.actor"),
			.overworldHandle = AssetManager::GetAssetHandle<OverworldHandle>("overworlds/default.ow")
		};

		InitGameData();
        InitGameState(GAME_STATE_DUNGEON);

        DungeonHandle testDungeon = AssetManager::GetAssetHandle<DungeonHandle>("dungeons/test_cave.dung");
        LoadRoom(testDungeon, { 14, 14 });
    }

    void Free() {}

	const GameConfig& GetConfig() {
		return g_config;
	}

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