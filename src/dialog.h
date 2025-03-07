#pragma once
#include "coroutines.h"

struct DialogState {
    bool active = false;
    u32 currentLine = 0;
    const char* const* pDialogueLines = nullptr;
    u32 lineCount;
    PoolHandle<Coroutine> currentLineCoroutine = PoolHandle<Coroutine>::Null();
};

namespace Game {
	void OpenDialog(const char* const* pDialogueLines, u32 lineCount);
	void UpdateDialog();
	bool IsDialogActive();
	DialogState* GetDialogState();
}