#pragma once
#include <assert.h>
#include "typedef.h"
#include "memory_pool.h"

static constexpr u32 MAX_COROUTINE_STATE_SIZE = 32;

typedef bool (*CoroutineFunc)(void*);
typedef void (*CoroutineCallback)();

struct Coroutine {
    CoroutineFunc func;
    alignas(void*) u8 state[MAX_COROUTINE_STATE_SIZE];

    void (*callback)() = nullptr;
};

typedef PoolHandle<Coroutine> CoroutineHandle;

namespace Game {
    CoroutineHandle StartCoroutine(CoroutineFunc func, void* state, u32 stateSize, CoroutineCallback callback = nullptr);

    template <typename S>
    CoroutineHandle StartCoroutine(CoroutineFunc func, const S& state, CoroutineCallback callback = nullptr) {
        static_assert(sizeof(S) <= MAX_COROUTINE_STATE_SIZE);
		return StartCoroutine(func, (void*)&state, sizeof(S), callback);
    }

    void StepCoroutines();
	void StopCoroutine(const CoroutineHandle& handle);
}