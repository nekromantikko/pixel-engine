#include "audio.h"
#include "core_types.h"
#include <cstdlib>
#include <SDL.h>
#include <vector>
#include "debug.h"
#include <cassert>
#include "nes_timing.h"
#include "asset_manager.h"
#include "random.h"

static constexpr u32 DEBUG_BUFFER_SIZE = 1024;

static constexpr u8 PULSE_SEQ[4] = {
        0b00000001,
        0b00000011,
        0b00001111,
        0b11111100
};

static constexpr u8 TRIANGLE_SEQ[0x20] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static constexpr u8 LENGTH_TABLE[0x20] = {
    10, 254, 20, 2, 40, 4, 80, 6, 10, 8, 0, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static constexpr u16 NOISE_PERIOD_TABLE[0x10] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

static constexpr s8 MAX_SEMITONE_SHIFT = 12; // Max semitone shift for pitch
static constexpr u32 SEMITONE_SHIFT_TABLE_SIZE = MAX_SEMITONE_SHIFT * 2 + 1; // -12 to +12 semitones
static constexpr r32 SEMITONE_SHIFT_TABLE[SEMITONE_SHIFT_TABLE_SIZE] = {
    2.0f, 1.88775f, 1.7818f, 1.68179f, 1.5874f, 1.49831f, 1.41421f, 1.33484f, 1.25992f, 1.18921f, 1.12246f, 1.05946f, 1.0f, 0.943874f, 0.890899f, 0.840896f, 0.793701f, 0.749154f, 0.707107f, 0.66742f, 0.629961f, 0.594604f, 0.561231f, 0.529732f, 0.5f
};

enum SoundOpCode : uint8_t {
    OP_NONE = 0x00,
    OP_WRITE_PULSE0 = 0x01,
    OP_WRITE_PULSE1 = 0x02,
    OP_WRITE_TRIANGLE = 0x03,
    OP_WRITE_NOISE = 0x04,
    OP_WRITE_DPCM = 0x05,
    OP_ENDFRAME = 0x0f
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

    // Effects
	s8 shiftSemitones = 0; // Pitch shift in semitones
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

    // Effects
    s8 shiftSemitones = 0; // Pitch shift in semitones
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

    // Effects
    s8 shiftSemitones = 0; // Pitch shift in semitones
};

struct AudioContext {
    SDL_AudioDeviceID audioDevice;

#ifdef EDITOR
    u8 debugBuffer[DEBUG_BUFFER_SIZE];
    u32 debugWriteOffset;
    u32 debugReadOffset;
#endif

    PulseChannel pulse[2];
    TriangleChannel triangle;
    NoiseChannel noise;

    r64 accumulator;
    s64 clockCounter;

    // Reserve channels for music while sfx is playing
    PulseChannel pulseReserve[2];
    TriangleChannel triangleReserve;
    NoiseChannel noiseReserve;

    SoundHandle music = SoundHandle::Null();
    u32 musicPos = 0;
    bool loopMusic;

    // Max one sfx per channel can play
    SoundHandle sfx[CHAN_COUNT]{};
    u32 sfxPos[CHAN_COUNT]{};
};

static AudioContext g_context;

static void WritePulse(PulseChannel* pulse, u8 address, u8 data) {
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

        pulse->lengthCounter = LENGTH_TABLE[pulse->reg.lengthCounterLoad];
    }
}

static void WritePulse(bool idx, u8 address, u8 data) {
    PulseChannel& pulse = g_context.pulse[idx];
    WritePulse(&pulse, address, data);
}

static void WriteTriangle(TriangleChannel* triangle, u8 address, u8 data) {
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

        triangle->lengthCounter = LENGTH_TABLE[triangle->reg.lengthCounterLoad];
    }
}

static void WriteTriangle(u8 address, u8 data) {
    TriangleChannel& triangle = g_context.triangle;
    WriteTriangle(&triangle, address, data);
}

static void WriteNoise(NoiseChannel* noise, u8 address, u8 data) {
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
        noise->counter = NOISE_PERIOD_TABLE[noise->reg.period];

        const u8 envelopePeriod = noise->reg.volume;
        noise->envelopeCounter = envelopePeriod + 1;
        noise->envelopeVolume = 0x0f;

        noise->shiftRegister = 1;
        noise->lengthCounter = LENGTH_TABLE[noise->reg.lengthCounterLoad];
    }
}

static void WriteNoise(u8 address, u8 data) {
    NoiseChannel& noise = g_context.noise;
    WriteNoise(&noise, address, data);
}

// Returns true if keep reading
static bool ProcessOp(const SoundOperation* operation, PulseChannel* p0, PulseChannel* p1, TriangleChannel* tri, NoiseChannel* noise) {
    bool keepReading = true;

    switch (operation->opCode) {
    case OP_WRITE_PULSE0: {
        if (p0 != nullptr) {
            WritePulse(p0, operation->address, operation->data);
        }
        break;
    }
    case OP_WRITE_PULSE1: {
        if (p1 != nullptr) {
            WritePulse(p1, operation->address, operation->data);
        }
        break;
    }
    case OP_WRITE_TRIANGLE: {
        if (tri != nullptr) {
            WriteTriangle(tri, operation->address, operation->data);
        }
        break;
    }
    case OP_WRITE_NOISE: {
        if (noise != nullptr) {
            WriteNoise(noise, operation->address, operation->data);
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

static void ClearRegisters() {
    for (u32 channel = 0; channel < CHAN_COUNT; channel++) {
        for (u32 i = 0; i < 4; i++) {
            Audio::WriteChannel(channel, i, 0);
        }
    }
}

// Returns true if sound keeps playing still
static bool TickSFX(u32 channel) {
    if (channel >= CHAN_COUNT) {
        return false;
    }

    const SoundHandle soundHandle = g_context.sfx[channel];
    u32& pos = g_context.sfxPos[channel];

    const Sound* pSound = AssetManager::GetAsset(soundHandle);
    if (pSound == nullptr) {
        g_context.sfx[channel] = SoundHandle::Null();
        return false;
    }

    PulseChannel* p0 = (channel == CHAN_ID_PULSE0) ? &g_context.pulse[0] : nullptr;
    PulseChannel* p1 = (channel == CHAN_ID_PULSE1) ? &g_context.pulse[1] : nullptr;
    TriangleChannel* tri = (channel == CHAN_ID_TRIANGLE) ? &g_context.triangle : nullptr;
    NoiseChannel* noise = (channel == CHAN_ID_NOISE) ? &g_context.noise : nullptr;

    bool keepReading = true;
    while (keepReading) {
        const SoundOperation* operation = pSound->GetData() + pos;

        keepReading = ProcessOp(operation, p0, p1, tri, noise);

        if (++pos == pSound->length) {
            return false;
        }
    }

    return true;
}

static bool TickMusic() {
    if (g_context.music == SoundHandle::Null()) {
        return false;
    }

    const Sound* pSound = AssetManager::GetAsset(g_context.music);
    if (!pSound) {
        g_context.music = SoundHandle::Null();
        return false;
    }

    PulseChannel* p0 = (g_context.sfx[CHAN_ID_PULSE0] == SoundHandle::Null()) ? &g_context.pulse[CHAN_ID_PULSE0] : &g_context.pulseReserve[0];
    PulseChannel* p1 = (g_context.sfx[CHAN_ID_PULSE1] == SoundHandle::Null()) ? &g_context.pulse[CHAN_ID_PULSE1] : &g_context.pulseReserve[1];
    TriangleChannel* tri = (g_context.sfx[CHAN_ID_TRIANGLE] == SoundHandle::Null()) ? &g_context.triangle : &g_context.triangleReserve;
    NoiseChannel* noise = (g_context.sfx[CHAN_ID_NOISE] == SoundHandle::Null()) ? &g_context.noise : &g_context.noiseReserve;

    bool keepReading = true;
    while (keepReading) {
        const SoundOperation* operation = pSound->GetData() + g_context.musicPos;

        keepReading = ProcessOp(operation, p0, p1, tri, noise);

        if (++g_context.musicPos == pSound->length) {
            if (g_context.loopMusic) {
                g_context.musicPos = pSound->loopPoint;
            }
            else {
                return false;
            }
        }
    }

    return true;
}

static void TickSoundPlayer() {
    for (int channel = 0; channel < CHAN_COUNT; channel++) {
        if (g_context.sfx[channel] == SoundHandle::Null()) {
            continue;
        }

        if (!TickSFX(channel)) {
            g_context.sfx[channel] = SoundHandle::Null();

            // I think this should happen one frame later
            switch (channel) {
            case CHAN_ID_PULSE0:
                g_context.pulse[0] = g_context.pulseReserve[0];
                break;
            case CHAN_ID_PULSE1:
                g_context.pulse[1] = g_context.pulseReserve[1];
                break;
            case CHAN_ID_TRIANGLE:
                g_context.triangle = g_context.triangleReserve;
                break;
            case CHAN_ID_NOISE:
                g_context.noise = g_context.noiseReserve;
                break;
            default:
                break;
            }
        }
    }

    if (!TickMusic()) {
        g_context.music = SoundHandle::Null();
    }
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
    
	// Apply pitch shift if needed
	if (pulse.shiftSemitones != 0) {
        const r32 multiplier = SEMITONE_SHIFT_TABLE[pulse.shiftSemitones + MAX_SEMITONE_SHIFT];
        period = u16(r32(period) * multiplier);
        period &= 0x7ffu; // Mask to valid period range
	}

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

    u8 sequence = PULSE_SEQ[pulse.reg.dutyCycle];
    u8 value = (sequence >> pulse.sequenceIdx) & 0b00000001;
    u8 volume = pulse.reg.constantVolume ? pulse.reg.volume : pulse.envelopeVolume;
    return value * volume;
}

static u8 ClockTriangle(TriangleChannel& triangle, bool quarterFrame, bool halfFrame) {
    u16 period = triangle.reg.periodLow + (triangle.reg.periodHigh << 8);

    // Apply pitch shift if needed
    if (triangle.shiftSemitones != 0) {
        const r32 multiplier = SEMITONE_SHIFT_TABLE[triangle.shiftSemitones + MAX_SEMITONE_SHIFT];
        period = u16(r32(period) * multiplier);
        period &= 0x7ffu; // Mask to valid period range
    }

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

    u8 value = TRIANGLE_SEQ[triangle.sequenceIdx];
    return value;
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

        noise.counter = NOISE_PERIOD_TABLE[noise.reg.period];

        // Apply pitch shift if needed
        if (noise.shiftSemitones != 0) {
			const r32 multiplier = SEMITONE_SHIFT_TABLE[noise.shiftSemitones + MAX_SEMITONE_SHIFT];
            noise.counter = u16(r32(noise.counter) * multiplier);
        }
    }

    bool muted = (noise.lengthCounter == 0) || (noise.shiftRegister & 1);
    if (muted) {
        return 0;
    }

    u8 volume = noise.reg.constantVolume ? noise.reg.volume : noise.envelopeVolume;
    return volume;
}

static bool Clock(u8& outSample) {
    const r64 sampleTime = 1.0f / 44100.f;
    bool result = false;

    g_context.clockCounter++;
    bool quarterFrame = false;
    bool halfFrame = false;

    if (g_context.clockCounter == QUARTER_FRAME_CLOCK) {
        quarterFrame = true;
    }
    else if (g_context.clockCounter == HALF_FRAME_CLOCK) {
        quarterFrame = true;
        halfFrame = true;
    }
    else if (g_context.clockCounter == THREEQUARTERS_FRAME_CLOCK) {
        quarterFrame = true;
    }
    else if (g_context.clockCounter == FRAME_CLOCK) {
        quarterFrame = true;
        halfFrame = true;
        g_context.clockCounter = 0;
        TickSoundPlayer();
    }

    r32 pulseOut = 0.0f;

    u8 pulseSum = ClockPulse(g_context.pulse[0], quarterFrame, halfFrame, 1);
    pulseSum += ClockPulse(g_context.pulse[1], quarterFrame, halfFrame, 0);

    if (pulseSum > 0) {
        pulseOut = 95.66f / (8128.0f / pulseSum + 100);
    }

    r32 tndOut = 0.0f;
    u8 triangle = ClockTriangle(g_context.triangle, quarterFrame, halfFrame);
    u8 noise = ClockNoise(g_context.noise, quarterFrame, halfFrame);

    if (triangle != 0 || noise != 0) {
        tndOut = 159.79f / (1 / ((r32)triangle / 8227 + (r32)noise / 12241) + 100);
    }

    r32 mix = pulseOut + tndOut;
    u8 sample = u8(mix * 127.0f + 128.0f);

    g_context.accumulator += CLOCK_PERIOD;
    if (g_context.accumulator >= sampleTime) {
        g_context.accumulator -= sampleTime;
        outSample = sample;
        result = true;
    }

    return result;
}

#ifdef EDITOR
static void WriteDebugBuffer(u8* samples, u32 count) {
    u32 remainingSamples = count;
    while (remainingSamples > 0) {
        u32 capacity = DEBUG_BUFFER_SIZE - g_context.debugWriteOffset;
        u32 samplesToWrite = capacity >= remainingSamples ? remainingSamples : capacity;
        memcpy(g_context.debugBuffer + g_context.debugWriteOffset, samples, samplesToWrite);
        remainingSamples -= samplesToWrite;
        g_context.debugWriteOffset += samplesToWrite;
        g_context.debugWriteOffset %= DEBUG_BUFFER_SIZE;
    }
}
#endif

// Callback
static void FillAudioBuffer(void* userdata, u8* stream, int len) {
    for (int i = 0; i < len; i++) {
        u8 sample;
        while (!Clock(sample)) {}

        stream[i] = sample;
    }

#ifdef EDITOR
    WriteDebugBuffer(stream, len);
#endif
}

namespace Audio {
    void Init() {
#ifdef EDITOR
        memset(g_context.debugBuffer, 0, DEBUG_BUFFER_SIZE);
        g_context.debugWriteOffset = 0;
        g_context.debugReadOffset = 0;
#endif

        ClearRegisters();
        // Make sure there's silence at startup
        g_context.noise.reg.constantVolume = true;

        SDL_AudioSpec audioSpec{};
        audioSpec.freq = 44100;
        audioSpec.format = AUDIO_U8;
        audioSpec.samples = 512;
        audioSpec.channels = 1;
        audioSpec.callback = FillAudioBuffer;
        audioSpec.userdata = nullptr;

        g_context.audioDevice = SDL_OpenAudioDevice(
            nullptr,
            0,
            &audioSpec,
            nullptr,
            0);

        SDL_PauseAudioDevice(g_context.audioDevice, 0);
    }

    void Free() {
        SDL_CloseAudioDevice(g_context.audioDevice);
    }

    void WriteChannel(u32 channel, u8 address, u8 data) {
        switch (channel) {
        case CHAN_ID_PULSE0:
            WritePulse(bool(0), address, data);
            break;
        case CHAN_ID_PULSE1:
            WritePulse(bool(1), address, data);
            break;
        case CHAN_ID_TRIANGLE:
            WriteTriangle(address, data);
            break;
        case CHAN_ID_NOISE:
            WriteNoise(address, data);
            break;
        default:
            break;
        }
    }

    void PlayMusic(SoundHandle musicHandle, bool loop) {
        const Sound* pSound = AssetManager::GetAsset(musicHandle);
        if (!pSound || pSound->type != SOUND_TYPE_MUSIC || pSound->length == 0) {
            return;
        }

        g_context.music = musicHandle;
        g_context.musicPos = 0;
        g_context.loopMusic = loop;
    }

    void StopMusic() {
        g_context.music = SoundHandle::Null();
        ClearRegisters();
    }

    void PlaySFX(SoundHandle soundHandle, s8 maxPitchShift) {
        const Sound* pSound = AssetManager::GetAsset(soundHandle);
        if (!pSound || pSound->type != SOUND_TYPE_SFX || pSound->length == 0) {
            return;
        }

        if (pSound->sfxChannel >= CHAN_COUNT) {
            return;
        }

        // Save register state to later continue music
        if (g_context.sfx[pSound->sfxChannel] == SoundHandle::Null()) {
            switch (pSound->sfxChannel) {
            case CHAN_ID_PULSE0:
                g_context.pulseReserve[0] = g_context.pulse[0];
                break;
            case CHAN_ID_PULSE1:
                g_context.pulseReserve[1] = g_context.pulse[1];
                break;
            case CHAN_ID_TRIANGLE:
                g_context.triangleReserve = g_context.triangle;
                break;
            case CHAN_ID_NOISE:
                g_context.noiseReserve = g_context.noise;
                break;
            default:
                break;
            }
        }

        g_context.sfx[pSound->sfxChannel] = soundHandle;
        g_context.sfxPos[pSound->sfxChannel] = 0;

        if (pSound->sfxChannel <= CHAN_ID_NOISE) {
            if (maxPitchShift > MAX_SEMITONE_SHIFT) {
				maxPitchShift = MAX_SEMITONE_SHIFT;
            }
            else if (maxPitchShift < -MAX_SEMITONE_SHIFT) {
				maxPitchShift = -MAX_SEMITONE_SHIFT;
            }
			const s8 pitchShift = maxPitchShift != 0 ? (s8)Random::GenerateInt(-maxPitchShift, maxPitchShift) : 0;

			if (pSound->sfxChannel == CHAN_ID_PULSE0) {
                g_context.pulse[0].shiftSemitones = pitchShift;
			}
            else if (pSound->sfxChannel == CHAN_ID_PULSE1) {
				g_context.pulse[1].shiftSemitones = pitchShift;
			}
            else if (pSound->sfxChannel == CHAN_ID_TRIANGLE) {
				g_context.triangle.shiftSemitones = pitchShift;
			}
            else if (pSound->sfxChannel == CHAN_ID_NOISE) {
				g_context.noise.shiftSemitones = pitchShift;
            }
        }
    }

#ifdef EDITOR
    void ReadChannel(u32 channel, void* outData) {
        switch (channel) {
        case CHAN_ID_PULSE0: {
            PulseChannel& pulse = g_context.pulse[0];
            memcpy(outData, &pulse.reg, 4);
            break;
        }
        case CHAN_ID_PULSE1: {
            PulseChannel& pulse = g_context.pulse[1];
            memcpy(outData, &pulse.reg, 4);
            break;
        }
        case CHAN_ID_TRIANGLE: {
            TriangleChannel& triangle = g_context.triangle;
            memcpy(outData, &triangle.reg, 4);
            break;
        }
        case CHAN_ID_NOISE: {
            NoiseChannel& noise = g_context.noise;
            memcpy(outData, &noise.reg, 4);
            break;
        }
        default:
            break;
        }
    }

    void ReadDebugBuffer(u8* outSamples, u32 count) {
        u32 remainingSamples = count;
        while (remainingSamples > 0) {
            u32 capacity = DEBUG_BUFFER_SIZE - g_context.debugReadOffset;
            u32 samplesToRead = capacity >= remainingSamples ? remainingSamples : capacity;
            memcpy(outSamples, g_context.debugBuffer + g_context.debugReadOffset, samplesToRead);
            remainingSamples -= samplesToRead;
            g_context.debugReadOffset += samplesToRead;
            g_context.debugReadOffset %= DEBUG_BUFFER_SIZE;
        }
    }
#endif
}

