#pragma once
#include "core_types.h"

struct AudioSettings {
    r32 masterVolume;
    r32 musicVolume;
    r32 sfxVolume;
};

static constexpr AudioSettings DEFAULT_AUDIO_SETTINGS = {
    1.0f, // masterVolume
    1.0f, // musicVolume  
    1.0f  // sfxVolume
};

namespace Audio {
    void Init();
    void Free();

    void WriteChannel(u32 channel, u8 address, u8 data);

    void PlayMusic(SoundHandle musicHandle, bool loop);
    void StopMusic();
    void PlaySFX(SoundHandle soundHandle);

    // Settings
    AudioSettings* GetSettingsPtr();

#ifdef EDITOR
    void ReadChannel(u32 channel, void* outData);
    void ReadDebugBuffer(u8* outSamples, u32 count);
#endif
}