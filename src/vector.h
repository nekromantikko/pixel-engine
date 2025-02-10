#pragma once
#include "typedef.h"
#include <cmath>

struct Vec2 {
	r32 x, y;

    inline Vec2 operator+(const Vec2& a) const {
        return Vec2{ x + a.x, y + a.y };
    }
    inline Vec2 operator-(const Vec2& a) const {
        return Vec2{ x - a.x, y - a.y };
    }
    inline Vec2 operator*(const Vec2& a) const {
        return Vec2{ x * a.x, y * a.y };
    }
    inline Vec2 operator/(const Vec2& a) const {
        return Vec2{ x / a.x, y / a.y };
    }

    inline Vec2 operator*(const r32 s) const {
        return Vec2{ x * s, y * s };
    }
    inline Vec2 operator/(const r32 s) const {
        return Vec2{ x / s, y / s };
    }

    inline r32 Length() const {
        return sqrt((x*x) + (y*y));
    }
    inline Vec2 Normalize() const {
        return *this / Length();
    }
};

inline r32 DotProduct(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}

inline Vec2 operator*(const r32& s, const Vec2& vec) {
    return { vec.x * s, vec.y * s };
}

struct IVec2 {
    s32 x, y;

    inline IVec2 operator+(const IVec2& a) const {
        return IVec2{ x + a.x, y + a.y };
    }
    inline IVec2 operator-(const IVec2& a) const {
        return IVec2{ x - a.x, y - a.y };
    }
    inline IVec2 operator*(const IVec2& a) const {
        return IVec2{ x * a.x, y * a.y };
    }
    inline IVec2 operator/(const IVec2& a) const {
        return IVec2{ x / a.x, y / a.y };
    }

    inline IVec2 operator*(const s32 s) const {
        return IVec2{ x * s, y * s };
    }
    inline IVec2 operator/(const s32 s) const {
        return IVec2{ x / s, y / s };
    }

    inline r32 Length() const {
        return sqrt((x * x) + (y * y));
    }
};