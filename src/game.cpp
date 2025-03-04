#include "game.h"
#include "system.h"
#include "game_input.h"
#include "game_state.h"
#include <cstring>
#include <cstdio>
#include "level.h"
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

namespace Game {
    r64 secondsElapsed = 0.0f;
    u32 clockCounter = 0;

    // Rendering data
    RenderSettings* pRenderSettings;
    ChrSheet* pChr;
    Nametable* pNametables;
    Scanline* pScanlines;
    Palette* pPalettes;

    bool paused = false;

    Sound jumpSfx;
    Sound gunSfx;
    Sound ricochetSfx;
    Sound damageSfx;
    Sound expSfx;
    Sound enemyDieSfx;

    //Sound bgm;
    //bool musicPlaying = false;

    // TODO: Try to eliminate as much of this as possible
    
    constexpr s32 haloSmallPrototypeIndex = 0x0a;
    constexpr s32 haloLargePrototypeIndex = 0x0b;
    
#pragma region Damage
    // TODO: Where to get this info properly?
    constexpr u16 largeExpValue = 500;
    constexpr u16 smallExpValue = 10;

    struct SpawnExpState {
        glm::vec2 position;
        u16 remainingValue;
    };

    static bool SpawnExpCoroutine(void* s) {
        SpawnExpState& state = *(SpawnExpState*)s;

        if (state.remainingValue > 0) {
            u16 spawnedValue = state.remainingValue >= largeExpValue ? largeExpValue : smallExpValue;
            s32 prototypeIndex = spawnedValue >= largeExpValue ? haloLargePrototypeIndex : haloSmallPrototypeIndex;

            const r32 speed = Random::GenerateReal(0.1f, 0.3f);
            const glm::vec2 velocity = Random::GenerateDirection() * speed;

            Actor* pSpawned = SpawnActor(prototypeIndex, state.position, velocity);

            pSpawned->state.pickupState.lingerCounter = 30;
            pSpawned->flags.facingDir = (s8)Random::GenerateInt(-1, 1);
            pSpawned->state.pickupState.value = pSpawned->pPrototype->data.pickupData.value;

            if (state.remainingValue < spawnedValue) {
                state.remainingValue = 0;
            }
            else state.remainingValue -= spawnedValue;

            return true;
        }
        return false;
    }

#pragma endregion

    static void Step() {
        if (!paused) {
            StepFrame();
        }

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
        // Rendering data
        pRenderSettings = ::Rendering::GetSettingsPtr();
        pChr = ::Rendering::GetChrPtr(0);
        pPalettes = ::Rendering::GetPalettePtr(0);
        pNametables = ::Rendering::GetNametablePtr(0);
        pScanlines = ::Rendering::GetScanlinePtr(0);

        Rendering::Init();

        Tiles::LoadTileset("assets/forest.til");
        Metasprites::Load("assets/meta.spr");
        Levels::LoadLevels("assets/levels.lev");
        Assets::LoadActorPrototypes("assets/actors.prt");

        // TEMP SOUND STUFF
        jumpSfx = Audio::LoadSound("assets/jump.nsf");
        gunSfx = Audio::LoadSound("assets/gun1.nsf");
        ricochetSfx = Audio::LoadSound("assets/ricochet.nsf");
        damageSfx = Audio::LoadSound("assets/damage.nsf");
        expSfx = Audio::LoadSound("assets/exp.nsf");
        enemyDieSfx = Audio::LoadSound("assets/enemydie.nsf");
        //bgm = Audio::LoadSound("assets/music.nsf");

		InitGameData();
        InitGameState(GAME_STATE_PLAYING);

        // TODO: Level should load palettes and tileset?
        LoadLevel(0);
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

    bool IsPaused() {
        return paused;
    }
    void SetPaused(bool p) {
        Rendering::ClearSpriteLayers();
        paused = p;
    }
#pragma endregion
}