#pragma once
#include <cstdlib>
#include "typedef.h"

namespace Audio {
    struct PulseRegister {
        u8 volume : 4;
        u8 constantVolume : 1;
        u8 loop : 1;
        u8 dutyCycle : 2;

        u8 sweepShift : 3;
        u8 sweepNegate : 1;
        u8 sweepPeriod : 3;
        u8 sweepEnabled : 1;

        u8 periodLow : 8;

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

	struct AudioContext;

	AudioContext* CreateAudioContext();
	void FreeAudioContext(AudioContext* pAudioContext);

    void PlayPulse(AudioContext* pContext, bool idx, u8 dutyCycle, u8 volume, u16 period, bool sweepEnabled, u8 sweepAmount, u8 sweepPeriod, bool sweepNegate, bool constantVolume, int length);

	void ReadDebugBuffer(AudioContext* pContext, u8* outSamples, u32 count);
	void WriteDebugBuffer(AudioContext* pContext, u8* samples, u32 count);
}