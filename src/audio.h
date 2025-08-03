#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "system_types.h"

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