#pragma once
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

struct SoundOperation {
    u8 opCode : 4;
    u8 address : 4;
    u8 data;
};

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

	inline SoundOperation* GetData() const {
		return (SoundOperation*)((u8*)this + dataOffset);
	}
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