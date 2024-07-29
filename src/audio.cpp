#include "audio.h"
#include <SDL.h>
#include <vector>

constexpr u32 audioBufferSize = 0xffff;

namespace Audio {
    struct AudioContext {
        SDL_AudioDeviceID audioDevice;

        s64 processedSamples;
        u8 buffer[audioBufferSize];
        u32 writeOffset;
        u32 readOffset;
    };

    void FillAudioBuffer(void* userdata, u8* stream, int len) {
        AudioContext* pContext = (AudioContext*)userdata;
        ReadAudioBuffer(pContext, stream, len);
    }

    AudioContext* CreateAudioContext() {
        AudioContext* pContext = (AudioContext*)calloc(1, sizeof(AudioContext));

        pContext->processedSamples = 0;
        memset(pContext->buffer, 0, audioBufferSize);
        pContext->writeOffset = 0;
        pContext->readOffset = 0;

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

    static r32 SquareWave(r32 frequency, r32 amplitude, s64 t) {
        const r32 period = 44100.f / frequency;

        return abs(fmodf(t, period)) < (period / 2) ? amplitude : -amplitude;
    }

    static r32 TriangleWave(r32 frequency, r32 amplitude, s64 t) {
        const r32 period = 44100.f / frequency;

        return (4 * amplitude / period)* abs((fmodf(t - period / 4, period)) - period / 2) - amplitude;
    }

    void Update(r64 dt, AudioContext* pContext) {
        u32 sampleCount = dt * 44100;

        for (int i = 0; i < sampleCount; i++) {
            pContext->processedSamples += 1;

            r32 combined = SquareWave(261.625525f, 0.1f, pContext->processedSamples) + TriangleWave(300.f, 0.1f, pContext->processedSamples);

            u8 denormalized = (combined + 1.0f) * 0x7f;
            WriteAudioBuffer(pContext, &denormalized, 1);
        }
    }

    void PeekAudioBuffer(AudioContext* pContext, u8* outSamples, u32 count) {
        u32 remainingSamples = count;
        u32 offset = pContext->readOffset;
        while (remainingSamples > 0) {
            u32 capacity = audioBufferSize - offset;
            u32 samplesToRead = capacity >= remainingSamples ? remainingSamples : capacity;
            memcpy(outSamples, pContext->buffer + offset, samplesToRead);
            remainingSamples -= samplesToRead;
            offset += samplesToRead;
            offset %= audioBufferSize;
        }
    }

    void ReadAudioBuffer(AudioContext* pContext, u8* outSamples, u32 count) {
        u32 remainingSamples = count;
        while (remainingSamples > 0) {
            u32 capacity = audioBufferSize - pContext->readOffset;
            u32 samplesToRead = capacity >= remainingSamples ? remainingSamples : capacity;
            memcpy(outSamples, pContext->buffer + pContext->readOffset, samplesToRead);
            remainingSamples -= samplesToRead;
            pContext->readOffset += samplesToRead;
            pContext->readOffset %= audioBufferSize;
        }
    }

    void WriteAudioBuffer(AudioContext* pContext, u8* samples, u32 count) {
        u32 remainingSamples = count;
        while (remainingSamples > 0) {
            u32 capacity = audioBufferSize - pContext->writeOffset;
            u32 samplesToWrite = capacity >= remainingSamples ? remainingSamples : capacity;
            memcpy(pContext->buffer + pContext->writeOffset, samples, samplesToWrite);
            remainingSamples -= samplesToWrite;
            pContext->writeOffset += samplesToWrite;
            pContext->writeOffset %= audioBufferSize;
        }
    }
}