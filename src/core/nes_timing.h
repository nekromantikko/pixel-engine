#pragma once
#include "typedef.h"

// NTSC, might add an option for PAL later
static constexpr r64 NES_CPU_FREQ = 1789773.0;
static constexpr r64 CLOCK_FREQ = NES_CPU_FREQ / 2.0;
static constexpr r64 CLOCK_PERIOD = 1.0 / CLOCK_FREQ;

static constexpr s32 QUARTER_FRAME_CLOCK = 3729;
static constexpr s32 HALF_FRAME_CLOCK = 7457;
static constexpr s32 THREEQUARTERS_FRAME_CLOCK = 11186;
static constexpr s32 FRAME_CLOCK = 14915;