#pragma once
#include "typedef.h"

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