#include "coroutines.h"

static constexpr u32 MAX_COROUTINE_COUNT = 256;

static Pool<Coroutine, MAX_COROUTINE_COUNT> coroutines;
static Pool<PoolHandle<Coroutine>, MAX_COROUTINE_COUNT> coroutineRemoveList;

static inline bool StepCoroutine(Coroutine* pCoroutine) {
    return pCoroutine->func(pCoroutine->state);
}

void Game::StepCoroutines() {
    coroutineRemoveList.Clear();

    for (u32 i = 0; i < coroutines.Count(); i++) {
        PoolHandle<Coroutine> handle = coroutines.GetHandle(i);
        Coroutine* pCoroutine = coroutines.Get(handle);

        if (!StepCoroutine(pCoroutine)) {
            coroutineRemoveList.Add(handle);

            if (pCoroutine->callback) {
                pCoroutine->callback();
            }
        }
    }

    for (u32 i = 0; i < coroutineRemoveList.Count(); i++) {
        auto handle = *coroutineRemoveList.Get(coroutineRemoveList.GetHandle(i));
        coroutines.Remove(handle);
    }
}

PoolHandle<Coroutine> Game::StartCoroutine(CoroutineFunc func, void* state, u32 stateSize, CoroutineCallback callback) {
	auto handle = coroutines.Add();
	Coroutine* pCoroutine = coroutines.Get(handle);

	if (pCoroutine == nullptr) {
		return PoolHandle<Coroutine>::Null();
	}

	pCoroutine->func = func;
	memcpy(pCoroutine->state, state, stateSize);
	pCoroutine->callback = callback;
	return handle;
}

void Game::StopCoroutine(const CoroutineHandle& handle) {
    coroutines.Remove(handle);
}