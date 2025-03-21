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
#include <imgui.h>
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

namespace Game {
    r64 secondsElapsed = 0.0f;
    u32 clockCounter = 0;

    Sound jumpSfx;
    Sound gunSfx;
    Sound ricochetSfx;
    Sound damageSfx;
    Sound expSfx;
    Sound enemyDieSfx;

    //Sound bgm;
    //bool musicPlaying = false;

    static void Step() {
        StepFrame();

        /*if (ButtonPressed(BUTTON_START)) {
            if (!musicPlaying) {
                Audio::PlayMusic(&bgm, true);
            }
            else {
                Audio::StopMusic();
            }
            musicPlaying = !musicPlaying;
        }*/

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

        Tiles::LoadTileset("assets/tileset.ass");
        Metasprites::Load("assets/metasprites.ass");
        Assets::LoadRoomTemplates("assets/rooms.ass");
        Assets::LoadActorPrototypes("assets/actors.ass");
        Assets::LoadDungeons("assets/dungeons.ass");
        Assets::LoadOverworld("assets/overworld.ass");

        // TEMP SOUND STUFF
        jumpSfx = Audio::LoadSound("assets/jump.nsf");
        gunSfx = Audio::LoadSound("assets/gun1.nsf");
        ricochetSfx = Audio::LoadSound("assets/ricochet.nsf");
        damageSfx = Audio::LoadSound("assets/damage.nsf");
        expSfx = Audio::LoadSound("assets/exp.nsf");
        enemyDieSfx = Audio::LoadSound("assets/enemydie.nsf");
        //bgm = Audio::LoadSound("assets/music.nsf");

		InitGameData();
        InitGameState(GAME_STATE_DUNGEON);

        // TODO: Level should load palettes and tileset?
        LoadRoom(1, { 14, 14 });
    }

    void Free() {
        Audio::StopMusic();

        Audio::FreeSound(&jumpSfx);
        Audio::FreeSound(&gunSfx);
        Audio::FreeSound(&ricochetSfx);
        Audio::FreeSound(&damageSfx);
        Audio::FreeSound(&expSfx);
        Audio::FreeSound(&enemyDieSfx);
        //Audio::FreeSound(&bgm);
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