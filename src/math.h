#pragma once
#include "typedef.h"
#include <cmath>

constexpr r64 pi = 3.14159265358979323846;

bool IsNearlyEqual(r32 a, r32 b, r32 tolerance = 0.00001f);

bool IsNearlyZero(r32 a, r32 tolerance = 0.00001f);

r32 Sign(r32 a);
r32 Max(r32 a, r32 b);
r32 Min(r32 a, r32 b);