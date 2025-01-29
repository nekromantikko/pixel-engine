#pragma once
#include <cstdlib>
#include "typedef.h"

namespace Audio {
    enum SoundOpCode : uint8_t {
        OP_NONE = 0x00,
        OP_WRITE_PULSE0 = 0x01,
        OP_WRITE_PULSE1 = 0x02,
        OP_WRITE_TRIANGLE = 0x03,
        OP_WRITE_NOISE = 0x04,
        OP_WRITE_DPCM = 0x05,
        OP_ENDFRAME = 0x0f
    };

    struct SoundOperation {
        u8 opCode : 4;
        u8 address : 4;
        u8 data;
    };

    struct NSFHeader {
        char signature[4];
        u32 unused;
        u32 size;
        u32 loopPoint;
    };

    enum ChannelId {
        CHAN_ID_PULSE0 = 0,
        CHAN_ID_PULSE1,
        CHAN_ID_TRIANGLE,
        CHAN_ID_NOISE,
        //CHAN_ID_DPCM,

        CHAN_COUNT
    };

    struct Sound {
        u32 length;
        u32 loopPoint;
        SoundOperation* data;
    };

    struct PulseRegister {
        // Address 0
        u8 volume : 4;
        u8 constantVolume : 1;
        u8 loop : 1;
        u8 dutyCycle : 2;

        // Address 1
        u8 sweepShift : 3;
        u8 sweepNegate : 1;
        u8 sweepPeriod : 3;
        u8 sweepEnabled : 1;

        // Address 2
        u8 periodLow : 8;

        // Address 3
        u8 periodHigh : 3;
        u8 lengthCounterLoad : 5;
    };

    struct PulseChannel {
        PulseRegister reg;

        u8 sequenceIdx : 3;
        u16 counter;

        u8 sweepCounter;

        s8 envelopeCounter;
        u8 envelopeVolume;

        u8 lengthCounter : 5;
        bool muted;
    };

    struct TriangleRegister {
        // Address 0
        u8 linearLoad : 7;
        u8 loop : 1;

        // Address 1
        u8 unused;

        // Address 2
        u8 periodLow : 8;

        // Address 3
        u8 periodHigh : 3;
        u8 lengthCounterLoad : 5;
    };

    struct TriangleChannel {
        TriangleRegister reg;

        u8 sequenceIdx : 5;
        u16 counter;

        u8 linearCounter : 7;
        bool halt;

        u8 lengthCounter : 5;
    };

    struct NoiseRegister {
        // Address 0
        u8 volume : 4;
        u8 constantVolume : 1;
        u8 loop : 1;
        u8 unused0 : 2;

        // Address 1
        u8 unused1;

        // Address 2
        u8 period : 4;
        u8 unused2 : 3;
        u8 mode : 1;

        // Address 3
        u8 unused3 : 3;
        u8 lengthCounterLoad : 5;
    };

    struct NoiseChannel {
        NoiseRegister reg;

        u16 counter;

        s8 envelopeCounter;
        u8 envelopeVolume;

        u16 shiftRegister : 15;

        u8 lengthCounter : 5;
    };

	struct AudioContext;

	AudioContext* CreateAudioContext();
	void FreeAudioContext(AudioContext* pAudioContext);

    void WritePulse(AudioContext* pContext, bool idx, u8 address, u8 data);
    void WriteTriangle(AudioContext* pContext, u8 address, u8 data);
    void WriteNoise(AudioContext* pContext, u8 address, u8 data);

    void DebugReadPulse(AudioContext* pContext, bool idx, void* outData);
    void DebugReadTriangle(AudioContext* pContext, void* outData);
    void DebugReadNoise(AudioContext* pContext, void* outData);

	void ReadDebugBuffer(AudioContext* pContext, u8* outSamples, u32 count);
	void WriteDebugBuffer(AudioContext* pContext, u8* samples, u32 count);

    Sound LoadSound(AudioContext* pContext, const char* fname);
    void PlayMusic(AudioContext* pContext, const Sound* pSound, bool loop);
    void StopMusic(AudioContext* pContext);
    void PlaySFX(AudioContext* pContext, const Sound* pSound, u32 channel);
    void FreeSound(AudioContext* pContext, Sound* pSound);
}