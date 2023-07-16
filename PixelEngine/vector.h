#pragma once
#include "typedef.h"
#include <cmath>

struct Vec2 {
	r32 x, y;

    inline Vec2 operator+(Vec2 a) const {
        return Vec2{ x + a.x, y + a.y };
    }
    inline Vec2 operator-(Vec2 a) const {
        return Vec2{ x - a.x, y - a.y };
    }
    inline Vec2 operator*(Vec2 a) const {
        return Vec2{ x * a.x, y * a.y };
    }
    inline Vec2 operator/(Vec2 a) const {
        return Vec2{ x / a.x, y / a.y };
    }

    inline Vec2 operator*(r32 s) const {
        return Vec2{ x * s, y * s };
    }
    inline Vec2 operator/(r32 s) const {
        return Vec2{ x / s, y / s };
    }

    inline r32 Length() const {
        return sqrt((x*x) + (y*y));
    }
    inline Vec2 Normalize() const {
        return *this / Length();
    }
};

inline r32 DotProduct(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

struct IVec2 {
    s32 x, y;

    inline IVec2 operator+(IVec2 a) const {
        return IVec2{ x + a.x, y + a.y };
    }
    inline IVec2 operator-(IVec2 a) const {
        return IVec2{ x - a.x, y - a.y };
    }
    inline IVec2 operator*(IVec2 a) const {
        return IVec2{ x * a.x, y * a.y };
    }
    inline IVec2 operator/(IVec2 a) const {
        return IVec2{ x / a.x, y / a.y };
    }

    inline IVec2 operator*(s32 s) const {
        return IVec2{ x * s, y * s };
    }
    inline IVec2 operator/(s32 s) const {
        return IVec2{ x / s, y / s };
    }

    inline r32 Length() const {
        return sqrt((x * x) + (y * y));
    }
};