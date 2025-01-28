#pragma once
#include <cstdlib>
#include "typedef.h"

namespace Audio {
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

	struct AudioContext;

	AudioContext* CreateAudioContext();
	void FreeAudioContext(AudioContext* pAudioContext);

    void WritePulse(AudioContext* pContext, bool idx, u8 address, u8 data);
    void WriteTriangle(AudioContext* pContext, u8 address, u8 data);

	void ReadDebugBuffer(AudioContext* pContext, u8* outSamples, u32 count);
	void WriteDebugBuffer(AudioContext* pContext, u8* samples, u32 count);
}