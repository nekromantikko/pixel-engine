#include "audio.h"
#include <SDL.h>
#include <vector>
#include "system.h"
#include "memory_pool.h"

static constexpr u32 debugBufferSize = 1024;
static constexpr r64 nesCpuFreq = 1789773.f;
static constexpr r64 clockFreq = nesCpuFreq / 2.0f;

static const u32 maxSoundsPlaying = 8;

namespace Audio {
    constexpr u8 pulseSequences[4] = {
        0b00000001,
        0b00000011,
        0b00001111,
        0b11111100
    };

    constexpr u8 triangleSequence[0x20] = {
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };

    constexpr u8 lengthTable[0x20] = {
        10, 254, 20, 2, 40, 4, 80, 6, 10, 8, 0, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
    };

    constexpr u8 noisePeriodTable[0x10] = {
        4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
    };

    struct AudioContext {
        SDL_AudioDeviceID audioDevice;

        u8 debugBuffer[debugBufferSize];
        u32 debugWriteOffset;
        u32 debugReadOffset;

        PulseChannel pulse[2];
        TriangleChannel triangle;
        NoiseChannel noise;

        r64 accumulator;
        s64 clockCounter;

        // Reserve channels for music while sfx is playing
        PulseChannel pulseReserve[2];
        TriangleChannel triangleReserve;
        NoiseChannel noiseReserve;

        const Sound* pMusic = nullptr;
        u32 musicPos = 0;
        bool loopMusic;

        // Max one sfx per channel can play
        const Sound* sfx[4]{};
        u32 sfxPos[4]{};
    };

    static void WritePulse(AudioContext* pContext, PulseChannel* pulse, u8 address, u8 data) {
        if (address > 3) {
            return;
        }

        // Write data straight into register
        u8* ptr = (u8*)&pulse->reg + address;
        *ptr = data;

        // Handle side effects
        if (address == 0) {
            // From nesdev:
            // "The duty cycle is changed (see table below), but the sequencer's current position isn't affected." 
            // So I guess nothing happens? Double check if sounds weird
        }
        else if (address == 1) {
            pulse->sweepCounter = pulse->reg.sweepPeriod + 1;
        }
        else if (address == 2) {
            const u16 period = pulse->reg.periodLow + (pulse->reg.periodHigh << 8);
            pulse->muted = (period < 0x08);
        }
        else if (address == 3) {
            // From nesdev:
            // "The sequencer is immediately restarted at the first value of the current sequence. The envelope is also restarted. The period divider is not reset."
            const u16 period = pulse->reg.periodLow + (pulse->reg.periodHigh << 8);
            pulse->counter = period + 1;
            pulse->muted = (period < 0x08);

            const u8 envelopePeriod = pulse->reg.volume;
            pulse->envelopeCounter = envelopePeriod + 1;
            pulse->envelopeVolume = 0x0f;

            pulse->lengthCounter = lengthTable[pulse->reg.lengthCounterLoad];
        }
    }

    void WritePulse(AudioContext* pContext, bool idx, u8 address, u8 data) {
        PulseChannel& pulse = pContext->pulse[idx];
        WritePulse(pContext, &pulse, address, data);
    }

    static void WriteTriangle(AudioContext* pContext, TriangleChannel* triangle, u8 address, u8 data) {
        if (address > 3) {
            return;
        }

        u8* ptr = (u8*)&triangle->reg + address;
        *ptr = data;

        // Handle side effects
        if (address == 0) {

        }
        else if (address == 1) {

        }
        else if (address == 2) {

        }
        else if (address == 3) {
            // From nesdev:
            // "Sets the linear counter reload flag"
            // Also known as halt

            triangle->halt = true;

            const u16 period = triangle->reg.periodLow + (triangle->reg.periodHigh << 8);
            triangle->counter = period + 1;

            triangle->lengthCounter = lengthTable[triangle->reg.lengthCounterLoad];
        }
    }

    void WriteTriangle(AudioContext* pContext, u8 address, u8 data) {
        TriangleChannel& triangle = pContext->triangle;
        WriteTriangle(pContext, &triangle, address, data);
    }

    static void WriteNoise(AudioContext* pContext, NoiseChannel* noise, u8 address, u8 data) {
        if (address > 3) {
            return;
        }

        u8* ptr = (u8*)&noise->reg + address;
        *ptr = data;

        // Handle side effects
        if (address == 0) {

        }
        else if (address == 1) {

        }
        else if (address == 2) {

        }
        else if (address == 3) {
            noise->counter = noisePeriodTable[noise->reg.period];

            const u8 envelopePeriod = noise->reg.volume;
            noise->envelopeCounter = envelopePeriod + 1;
            noise->envelopeVolume = 0x0f;

            noise->shiftRegister = 1;
            noise->lengthCounter = lengthTable[noise->reg.lengthCounterLoad];
        }
    }

    void WriteNoise(AudioContext* pContext, u8 address, u8 data) {
        NoiseChannel& noise = pContext->noise;
        WriteNoise(pContext, &noise, address, data);
    }

    // Returns true if keep reading
    static bool HandleSoundOp(AudioContext* pAudioContext, const SoundOperation* operation, PulseChannel* p0, PulseChannel* p1, TriangleChannel* tri, NoiseChannel* noise) {
        bool keepReading = true;
        
        switch (operation->opCode) {
        case OP_WRITE_PULSE0: {
            if (p0 != nullptr) {
                WritePulse(pAudioContext, p0, operation->address, operation->data);
            }
            break;
        }
        case OP_WRITE_PULSE1: {
            if (p1 != nullptr) {
                WritePulse(pAudioContext, p1, operation->address, operation->data);
            }
            break;
        }
        case OP_WRITE_TRIANGLE: {
            if (tri != nullptr) {
                WriteTriangle(pAudioContext, tri, operation->address, operation->data);
            }
            break;
        }
        case OP_WRITE_NOISE: {
            if (noise != nullptr) {
                WriteNoise(pAudioContext, noise, operation->address, operation->data);
            }
            break;
        }
        case OP_ENDFRAME: {
            keepReading = false;
            break;
        }
        default:
            break;
        }

        return keepReading;
    }

    // Returns true if sound keeps playing still
    static bool TickSFX(AudioContext* pContext, u32 channel) {
        if (channel >= CHAN_COUNT) {
            return false;
        }

        const Sound* pSound = pContext->sfx[channel];
        u32& pos = pContext->sfxPos[channel];

        if (pSound == nullptr) {
            return false;
        }

        PulseChannel* p0 = (channel == CHAN_ID_PULSE0) ? &pContext->pulse[0] : nullptr;
        PulseChannel* p1 = (channel == CHAN_ID_PULSE1) ? &pContext->pulse[1] : nullptr;
        TriangleChannel* tri = (channel == CHAN_ID_TRIANGLE) ? &pContext->triangle : nullptr;
        NoiseChannel* noise = (channel == CHAN_ID_NOISE) ? &pContext->noise : nullptr;

        bool keepReading = true;
        while (keepReading) {
            const SoundOperation* operation = pSound->data + pos;

            keepReading = HandleSoundOp(pContext, operation, p0, p1, tri, noise);

            if (++pos == pSound->length) {
                return false;
            }
        }

        return true;
    }

    static bool TickMusic(AudioContext* pContext) {
        if (pContext->pMusic == nullptr) {
            return false;
        }

        PulseChannel* p0 = (pContext->sfx[0] == nullptr) ? &pContext->pulse[0] : &pContext->pulseReserve[0];
        PulseChannel* p1 = (pContext->sfx[1] == nullptr) ? &pContext->pulse[1] : &pContext->pulseReserve[1];
        TriangleChannel* tri = (pContext->sfx[2] == nullptr) ? &pContext->triangle : &pContext->triangleReserve;
        NoiseChannel* noise = (pContext->sfx[3] == nullptr) ? &pContext->noise : &pContext->noiseReserve;

        bool keepReading = true;
        while (keepReading) {
            const SoundOperation* operation = pContext->pMusic->data + pContext->musicPos;

            keepReading = HandleSoundOp(pContext, operation, p0, p1, tri, noise);

            if (++pContext->musicPos == pContext->pMusic->length) {
                if (pContext->loopMusic) {
                    pContext->musicPos = pContext->pMusic->loopPoint;
                }
                else {
                    return false;
                }
            }
        }

        return true;
    }

    static void TickSoundPlayer(AudioContext* pContext) {
        for (int channel = 0; channel < CHAN_COUNT; channel++) {
            if (pContext->sfx[channel] == nullptr) {
                continue;
            }

            if (!TickSFX(pContext, channel)) {
                pContext->sfx[channel] = nullptr;

                // I think this should happen one frame later
                switch (channel) {
                case CHAN_ID_PULSE0:
                    pContext->pulse[0] = pContext->pulseReserve[0];
                    break;
                case CHAN_ID_PULSE1:
                    pContext->pulse[1] = pContext->pulseReserve[1];
                    break;
                case CHAN_ID_TRIANGLE:
                    pContext->triangle = pContext->triangleReserve;
                    break;
                case CHAN_ID_NOISE:
                    pContext->noise = pContext->noiseReserve;
                    break;
                default:
                    break;
                }
            }
        }

        if (!TickMusic(pContext)) {
            pContext->pMusic = nullptr;
        }
    }

    static u8 ClockNoise(NoiseChannel& noise, bool quarterFrame, bool halfFrame) {
        // Clock envelope
        if (quarterFrame) {
            if (--noise.envelopeCounter == 0) {
                const u8 envelopePeriod = noise.reg.volume;
                noise.envelopeCounter = envelopePeriod + 1;
                if (noise.envelopeVolume > 0) {
                    noise.envelopeVolume--;
                }
                else if (noise.reg.loop) {
                    noise.envelopeVolume = (noise.envelopeVolume - 1) & 0x0F;
                }
            }
        }

        // Clock length counter
        if (halfFrame) {
            if (!noise.reg.loop && noise.lengthCounter != 0) {
                noise.lengthCounter--;
            }
        }

        noise.counter--;
        if (noise.counter == 0xFFFF) {
            
            u8 modeBitIndex = noise.reg.mode ? 6 : 1;
            u8 modeBit = noise.shiftRegister >> modeBitIndex;
            u8 feedbackBit = (noise.shiftRegister ^ modeBit) & 1;

            noise.shiftRegister >>= 1;
            noise.shiftRegister |= (feedbackBit << 14);

            noise.counter = noisePeriodTable[noise.reg.period];
        }

        bool muted = (noise.lengthCounter == 0) || (noise.shiftRegister & 1);
        if (muted) {
            return 0;
        }

        u8 volume = noise.reg.constantVolume ? noise.reg.volume : noise.envelopeVolume;
        return volume;
    }

    static u8 ClockTriangle(TriangleChannel& triangle, bool quarterFrame, bool halfFrame) {
        const u16 period = triangle.reg.periodLow + (triangle.reg.periodHigh << 8);
        
        if (quarterFrame) {
            if (triangle.halt) {
                triangle.linearCounter = triangle.reg.linearLoad;
            }
            else if (triangle.linearCounter != 0) {
                triangle.linearCounter--;
            }

            if (!triangle.reg.loop) {
                triangle.halt = false;
            }
        }

        if (halfFrame) {
            if (!triangle.reg.loop && triangle.lengthCounter != 0) {
                triangle.lengthCounter--;
            }
        }

        for (int i = 0; i < 2; i++) {
            if (triangle.lengthCounter != 0 && triangle.linearCounter != 0) {
                triangle.counter--;
                if (triangle.counter == 0xFFFF) {
                    triangle.sequenceIdx--;
                    triangle.counter = period + 1;
                }
            }
        }

        if (triangle.lengthCounter == 0) return 0;

        u8 value = triangleSequence[triangle.sequenceIdx];
        return value;
    }

    static u8 ClockPulse(PulseChannel& pulse, bool quarterFrame, bool halfFrame, int sweepDiff) {
        
        if (quarterFrame) {
            if (--pulse.envelopeCounter == 0) {
                const u8 envelopePeriod = pulse.reg.volume;
                pulse.envelopeCounter = envelopePeriod + 1;
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
            TickSoundPlayer(pContext);
        }

        r32 pulseOut = 0.0f;

        u8 pulseSum = ClockPulse(pContext->pulse[0], quarterFrame, halfFrame, 1);
        pulseSum += ClockPulse(pContext->pulse[1], quarterFrame, halfFrame, 0);

        if (pulseSum > 0) {
            pulseOut = 95.66f / (8128.0f / pulseSum + 100);
        }

        r32 tndOut = 0.0f;
        u8 triangle = ClockTriangle(pContext->triangle, quarterFrame, halfFrame);
        u8 noise = ClockNoise(pContext->noise, quarterFrame, halfFrame);

        if (triangle != 0 || noise != 0) {
            tndOut = 159.79f / (1 / ((r32)triangle / 8227 + (r32)noise / 12241) + 100);
        }

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

    static void ClearRegisters(AudioContext* pContext) {
        WritePulse(pContext, false, 0, 0);
        WritePulse(pContext, false, 1, 0);
        WritePulse(pContext, false, 2, 0);
        WritePulse(pContext, false, 3, 0);

        WritePulse(pContext, true, 0, 0);
        WritePulse(pContext, true, 1, 0);
        WritePulse(pContext, true, 2, 0);
        WritePulse(pContext, true, 3, 0);

        WriteTriangle(pContext, 0, 0);
        WriteTriangle(pContext, 1, 0);
        WriteTriangle(pContext, 2, 0);
        WriteTriangle(pContext, 3, 0);

        WriteNoise(pContext, 0, 0);
        WriteNoise(pContext, 1, 0);
        WriteNoise(pContext, 2, 0);
        WriteNoise(pContext, 3, 0);
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

        ClearRegisters(pContext);

        return pContext;
    }
    void FreeAudioContext(AudioContext* pContext) {
        SDL_CloseAudioDevice(pContext->audioDevice);
        free(pContext);
    }

    // For debug only!
    void DebugReadPulse(AudioContext* pContext, bool idx, void* outData) {
        PulseChannel& pulse = pContext->pulse[idx];
        memcpy(outData, &pulse.reg, 4);
    }
    void DebugReadTriangle(AudioContext* pContext, void* outData) {
        TriangleChannel& triangle = pContext->triangle;
        memcpy(outData, &triangle.reg, 4);
    }
    void DebugReadNoise(AudioContext* pContext, void* outData) {
        NoiseChannel& noise = pContext->noise;
        memcpy(outData, &noise.reg, 4);
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

    Sound LoadSound(AudioContext* pContext, const char* fname) {
        FILE* pFile;
        fopen_s(&pFile, fname, "rb");

        if (pFile == NULL) {
            DEBUG_ERROR("Failed to load music file\n");
        }

        NSFHeader header{};
        fread(&header, sizeof(NSFHeader), 1, pFile);

        Sound sound{};
        sound.length = header.size;
        sound.loopPoint = header.loopPoint;

        sound.data = (SoundOperation*)calloc(sound.length, sizeof(SoundOperation));
        fread(sound.data, sizeof(SoundOperation), sound.length, pFile);

        fclose(pFile);

        return sound;
    }

    void PlayMusic(AudioContext* pContext, const Sound* pSound, bool loop) {
        pContext->pMusic = pSound;
        pContext->musicPos = 0;
        pContext->loopMusic = loop;
    }

    void StopMusic(AudioContext* pContext) {
        pContext->pMusic = nullptr;
        ClearRegisters(pContext);
    }

    void PlaySFX(AudioContext* pContext, const Sound* pSound, u32 channel) {
        if (channel >= CHAN_COUNT) {
            return;
        }

        // Save register state to later continue music
        if (pContext->sfx[channel] == nullptr) {
            switch (channel) {
            case CHAN_ID_PULSE0:
                pContext->pulseReserve[0] = pContext->pulse[0];
                break;
            case CHAN_ID_PULSE1:
                pContext->pulseReserve[1] = pContext->pulse[1];
                break;
            case CHAN_ID_TRIANGLE:
                pContext->triangleReserve = pContext->triangle;
                break;
            case CHAN_ID_NOISE:
                pContext->noiseReserve = pContext->noise;
                break;
            default:
                break;
            }
        }

        pContext->sfx[channel] = pSound;
        pContext->sfxPos[channel] = 0;
    }

    void FreeSound(AudioContext* pContext, Sound* pSound) {
        if (pSound != nullptr) {
            free(pSound->data);
        }
    }
}