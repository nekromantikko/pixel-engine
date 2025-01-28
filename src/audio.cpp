#include "audio.h"
#include <SDL.h>
#include <vector>

constexpr u32 debugBufferSize = 1024;
constexpr r64 nesCpuFreq = 1789773.f;
constexpr r64 clockFreq = nesCpuFreq / 2.0f;

namespace Audio {
    constexpr u8 pulseSequences[4] = {
        0b00000001,
        0b00000011,
        0b00001111,
        0b11111100
    };

    constexpr u8 lengthTable[0x20] = {
        10, 254, 20, 2, 40, 4, 80, 6, 10, 8, 0, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
    };

    struct AudioContext {
        SDL_AudioDeviceID audioDevice;

        u8 debugBuffer[debugBufferSize];
        u32 debugWriteOffset;
        u32 debugReadOffset;

        PulseChannel pulse1;
        PulseChannel pulse2;

        r64 accumulator;
        s64 clockCounter;
    };

    static u8 ClockPulse(PulseChannel& pulse, bool quarterFrame, bool halfFrame, int sweepDiff) {
        
        if (quarterFrame) {
            if (--pulse.envelopeCounter == 0) {
                pulse.envelopeCounter = pulse.reg.volume + 1;
                if (pulse.envelopeVolume > 0) {
                    pulse.envelopeVolume--;
                }
                else if (pulse.reg.loop) {
                    pulse.envelopeVolume = (pulse.envelopeVolume - 1) & 0x0F;
                }
            }
        }

        u16 period = pulse.reg.periodLow + (pulse.reg.periodHigh << 8);
        pulse.muted |= (period < 0x08);

        if (halfFrame) {
            if (--pulse.sweepCounter == 0) {
                pulse.sweepCounter = pulse.reg.sweepPeriod + 1;

                s16 periodChange = period >> pulse.reg.sweepShift;
                s16 targetPeriod = period + periodChange;
                if (pulse.reg.sweepNegate) {
                    targetPeriod = period - periodChange - sweepDiff;
                }

                pulse.muted |= (targetPeriod > 0x7ff);

                if (pulse.reg.sweepEnabled && !pulse.muted && (pulse.reg.sweepShift > 0)) {
                    period = targetPeriod;
                    pulse.reg.periodLow = period & 0b11111111;
                    pulse.reg.periodHigh = period >> 8;
                }
            }

            if (!pulse.reg.loop && pulse.lengthCounter != 0) {
                pulse.lengthCounter--;
            }
        }

        if (pulse.muted || pulse.lengthCounter == 0) return 0;

        pulse.counter--;
        if (pulse.counter == 0xFFFF) {
            pulse.sequenceIdx--;
            pulse.counter = period + 1;
        }

        u8 sequence = pulseSequences[pulse.reg.dutyCycle];
        u8 value = (sequence >> pulse.sequenceIdx) & 0b00000001;
        u8 volume = pulse.reg.constantVolume ? pulse.reg.volume : pulse.envelopeVolume;
        return value * volume;
    }

    static bool Clock(AudioContext* pContext, u8& outSample) {
        const r64 sampleTime = 1.0f / 44100.f;
        const r64 clockTime = 1.0f / clockFreq;
        bool result = false;

        pContext->clockCounter++;
        bool quarterFrame = false;
        bool halfFrame = false;

        if (pContext->clockCounter == 3729) {
            quarterFrame = true;
        }
        else if (pContext->clockCounter == 7457) {
            quarterFrame = true;
            halfFrame = true;
        }
        else if (pContext->clockCounter == 11186) {
            quarterFrame = true;
        }
        else if (pContext->clockCounter == 14915) {
            quarterFrame = true;
            halfFrame = true;
            pContext->clockCounter = 0;
        }

        r32 pulseOut = 0.0f;

        u8 pulseSum = ClockPulse(pContext->pulse1, quarterFrame, halfFrame, 1);
        pulseSum += ClockPulse(pContext->pulse2, quarterFrame, halfFrame, 0);

        if (pulseSum > 0) {
            pulseOut = 95.66f / (8128.0f / pulseSum + 100);
        }

        r32 tndOut = 0.0f;

        u8 sample = (u8)((pulseOut + tndOut) * 255);

        pContext->accumulator += clockTime;
        if (pContext->accumulator >= sampleTime) {
            pContext->accumulator -= sampleTime;
            outSample = sample;
            result = true;
        }

        return result;
    }

    static void FillAudioBuffer(void* userdata, u8* stream, int len) {
        AudioContext* pContext = (AudioContext*)userdata;

        for (int i = 0; i < len; i++) {
            u8 pulse;
            while (!Clock(pContext, pulse)) {}

            stream[i] = pulse;
        }

        WriteDebugBuffer(pContext, stream, len);
    }

    AudioContext* CreateAudioContext() {
        AudioContext* pContext = (AudioContext*)calloc(1, sizeof(AudioContext));

        memset(pContext->debugBuffer, 0, debugBufferSize);
        pContext->debugWriteOffset = 0;
        pContext->debugReadOffset = 0;

        SDL_AudioSpec audioSpec{};
        audioSpec.freq = 44100;
        audioSpec.format = AUDIO_U8;
        audioSpec.samples = 512;
        audioSpec.channels = 1;
        audioSpec.callback = FillAudioBuffer;
        audioSpec.userdata = (void*)pContext;

        pContext->audioDevice = SDL_OpenAudioDevice(
            nullptr,
            0,
            &audioSpec,
            nullptr,
            0);

        SDL_PauseAudioDevice(pContext->audioDevice, 0);

        return pContext;
    }
    void FreeAudioContext(AudioContext* pContext) {
        SDL_CloseAudioDevice(pContext->audioDevice);
        free(pContext);
    }

    void PlayPulse(AudioContext* pContext, bool idx, u8 dutyCycle, u8 volume, u16 period, bool sweepEnabled, u8 sweepAmount, u8 sweepPeriod, bool sweepNegate, bool constantVolume, int length) {
        PulseChannel& pulse = idx == 0 ? pContext->pulse1 : pContext->pulse2;
        pulse.reg.dutyCycle = dutyCycle;
        pulse.reg.volume = volume;
        pulse.reg.periodLow = period & 0b11111111;
        pulse.reg.periodHigh = period >> 8;
        pulse.counter = period + 1;

        pulse.reg.sweepEnabled = sweepEnabled;
        pulse.reg.sweepShift = sweepAmount;
        pulse.reg.sweepPeriod = sweepPeriod;
        pulse.reg.sweepNegate = sweepNegate;
        pulse.sweepCounter = sweepPeriod + 1;

        pulse.reg.constantVolume = constantVolume;
        pulse.envelopeCounter = volume + 1;
        pulse.envelopeVolume = 15;

        if (length < 0) {
            pulse.reg.loop = true;
            pulse.lengthCounter = 0x1f;
        }
        else {
            pulse.reg.loop = false;
            pulse.reg.lengthCounterLoad = length;
            pulse.lengthCounter = lengthTable[length];
        }

        pulse.muted = false;
    }

    void ReadDebugBuffer(AudioContext* pContext, u8* outSamples, u32 count) {
        u32 remainingSamples = count;
        while (remainingSamples > 0) {
            u32 capacity = debugBufferSize - pContext->debugReadOffset;
            u32 samplesToRead = capacity >= remainingSamples ? remainingSamples : capacity;
            memcpy(outSamples, pContext->debugBuffer + pContext->debugReadOffset, samplesToRead);
            remainingSamples -= samplesToRead;
            pContext->debugReadOffset += samplesToRead;
            pContext->debugReadOffset %= debugBufferSize;
        }
    }

    void WriteDebugBuffer(AudioContext* pContext, u8* samples, u32 count) {
        u32 remainingSamples = count;
        while (remainingSamples > 0) {
            u32 capacity = debugBufferSize - pContext->debugWriteOffset;
            u32 samplesToWrite = capacity >= remainingSamples ? remainingSamples : capacity;
            memcpy(pContext->debugBuffer + pContext->debugWriteOffset, samples, samplesToWrite);
            remainingSamples -= samplesToWrite;
            pContext->debugWriteOffset += samplesToWrite;
            pContext->debugWriteOffset %= debugBufferSize;
        }
    }
}