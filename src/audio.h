#pragma once
#include <cstdlib>
#include <filesystem>
#include "typedef.h"
#include "asset_types.h"

enum SoundChannelId {
    CHAN_ID_PULSE0 = 0,
    CHAN_ID_PULSE1,
    CHAN_ID_TRIANGLE,
    CHAN_ID_NOISE,
    //CHAN_ID_DPCM,

    CHAN_COUNT
};

struct SoundOperation;

enum SoundType {
    SOUND_TYPE_SFX = 0,
    SOUND_TYPE_MUSIC,

    SOUND_TYPE_COUNT
};

struct Sound {
    u32 length;
    u32 loopPoint;
    u16 type;
    u16 sfxChannel;
    u32 dataOffset;
};

namespace Audio {
	void CreateContext();
    void Init();
    void Free();
    void DestroyContext();

    void WriteChannel(u32 channel, u8 address, u8 data);

    void PlayMusic(SoundHandle musicHandle, bool loop);
    void StopMusic();
    void PlaySFX(SoundHandle soundHandle);

#ifdef EDITOR
    void ReadChannel(u32 channel, void* outData);
    void ReadDebugBuffer(u8* outSamples, u32 count);
#endif
}

namespace Assets {
    void InitSound(void* data);
    u32 GetSoundSize(const Sound* pSound = nullptr);
    SoundOperation* GetSoundData(const Sound* pSound);
    bool LoadSoundFromFile(const std::filesystem::path& path, u32& dataSize, void* data = nullptr);
}