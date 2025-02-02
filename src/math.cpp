#include "math.h"

bool IsNearlyEqual(r32 a, r32 b, r32 tolerance) {
	return fabs(a - b) <= tolerance;
}

bool IsNearlyZero(r32 a, r32 tolerance) {
	return IsNearlyEqual(a, 0.0f, tolerance);
}

r32 Sign(r32 a) {
	if (a < 0.0f) {
		return -1.0f;
	}
	else if (a > 0.0f) {
		return 1.0f;
	}
	else return 0.0f;
}