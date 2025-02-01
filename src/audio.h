#pragma once
#include <cstdlib>
#include "typedef.h"

enum SoundChannelId {
    CHAN_ID_PULSE0 = 0,
    CHAN_ID_PULSE1,
    CHAN_ID_TRIANGLE,
    CHAN_ID_NOISE,
    //CHAN_ID_DPCM,

    CHAN_COUNT
};

struct SoundOperation;

struct Sound {
    u32 length;
    u32 loopPoint;
    SoundOperation* data;
};

namespace Audio {
	void CreateContext();
    void Init();
    void Free();
    void DestroyContext();

    void WriteChannel(u32 channel, u8 address, u8 data);

    Sound LoadSound(const char* fname);
    void PlayMusic(const Sound* pSound, bool loop);
    void StopMusic();
    void PlaySFX(const Sound* pSound, u32 channel);
    void FreeSound(Sound* pSound);

#ifdef EDITOR
    void ReadChannel(u32 channel, void* outData);
    void ReadDebugBuffer(u8* outSamples, u32 count);
#endif
}