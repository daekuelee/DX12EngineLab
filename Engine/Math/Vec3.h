#pragma once
// =========================================================================
// SSOT: docs/contracts/math/vec3-contract.md
// REF: PhysX keeps public PxVec3 as 3 floats and lifts to internal Vec3V for SIMD.
//
// POLICY:
//   - Engine-wide scalar Vec3 contract.
//   - 12-byte POD storage: {x,y,z}.
//   - No DirectXMath, SIMD intrinsics, platform headers, globals, or mutable state.
//   - SIMD acceleration belongs in a separate backend layer that loads/stores Vec3.
// =========================================================================

#include "MathCommon.h"

#include <cmath>
#include <type_traits>

namespace Engine { namespace Math {

inline constexpr float kVecEpsSq = 1e-20f;
inline constexpr float kVecNormalizeTolerance = 1e-4f;

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

static_assert(sizeof(Vec3) == 12, "Vec3 must remain 12 bytes");
static_assert(alignof(Vec3) == alignof(float), "Vec3 must keep float alignment");
static_assert(std::is_standard_layout<Vec3>::value, "Vec3 must be standard layout");
static_assert(std::is_trivially_copyable<Vec3>::value, "Vec3 must be trivially copyable");

inline constexpr Vec3 Zero3() { return {0.0f, 0.0f, 0.0f}; }
inline constexpr Vec3 One3() { return {1.0f, 1.0f, 1.0f}; }
inline constexpr Vec3 Up3() { return {0.0f, 1.0f, 0.0f}; }

inline constexpr Vec3 operator-(const Vec3& v) {
    return {-v.x, -v.y, -v.z};
}

inline constexpr Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline constexpr Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline constexpr Vec3 operator*(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

inline constexpr Vec3 operator*(float s, const Vec3& v) {
    return v * s;
}

inline Vec3 operator/(const Vec3& v, float s) {
    const float inv = 1.0f / s;
    return v * inv;
}

inline Vec3& operator+=(Vec3& a, const Vec3& b) {
    a = a + b;
    return a;
}

inline Vec3& operator-=(Vec3& a, const Vec3& b) {
    a = a - b;
    return a;
}

inline Vec3& operator*=(Vec3& v, float s) {
    v = v * s;
    return v;
}

inline Vec3& operator/=(Vec3& v, float s) {
    v = v / s;
    return v;
}

inline constexpr bool operator==(const Vec3& a, const Vec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline constexpr bool operator!=(const Vec3& a, const Vec3& b) {
    return !(a == b);
}

inline constexpr float Abs(float v) {
    return v < 0.0f ? -v : v;
}

inline constexpr float Min(float a, float b) {
    return a < b ? a : b;
}

inline constexpr float Max(float a, float b) {
    return a > b ? a : b;
}

inline constexpr float Clamp(float v, float lo, float hi) {
    return Max(lo, Min(v, hi));
}

inline constexpr Vec3 Abs(const Vec3& v) {
    return {Abs(v.x), Abs(v.y), Abs(v.z)};
}

inline constexpr Vec3 Min(const Vec3& a, const Vec3& b) {
    return {Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z)};
}

inline constexpr Vec3 Max(const Vec3& a, const Vec3& b) {
    return {Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z)};
}

inline constexpr Vec3 Clamp(const Vec3& v, const Vec3& lo, const Vec3& hi) {
    return Max(lo, Min(v, hi));
}

inline constexpr Vec3 Mul(const Vec3& a, const Vec3& b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

inline constexpr float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline constexpr Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline constexpr float LengthSq(const Vec3& v) {
    return Dot(v, v);
}

inline float Length(const Vec3& v) {
    return std::sqrt(LengthSq(v));
}

inline bool IsFinite(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline constexpr bool IsZero(const Vec3& v) {
    return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
}

inline bool IsNormalized(const Vec3& v, float tolerance = kVecNormalizeTolerance) {
    return IsFinite(v) && Abs(Length(v) - 1.0f) <= tolerance;
}

inline bool NearEqual(const Vec3& a, const Vec3& b, float tolerance) {
    return Abs(a.x - b.x) <= tolerance &&
           Abs(a.y - b.y) <= tolerance &&
           Abs(a.z - b.z) <= tolerance;
}

inline Vec3 Normalize(const Vec3& v) {
    const float len = Length(v);
    return len > 0.0f ? v / len : Zero3();
}

inline Vec3 NormalizeSafe(const Vec3& v, const Vec3& fallback) {
    const float lenSq = LengthSq(v);
    if (!(lenSq > kVecEpsSq)) {
        return fallback;
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    const Vec3 out = v * invLen;
    return IsFinite(out) ? out : fallback;
}

inline Vec3 ScaleAdd(const Vec3& a, float scale, const Vec3& b) {
    return a * scale + b;
}

inline Vec3 ProjectOn(const Vec3& v, const Vec3& unitAxis) {
    return unitAxis * Dot(v, unitAxis);
}

inline Vec3 RejectFrom(const Vec3& v, const Vec3& unitAxis) {
    return v - ProjectOn(v, unitAxis);
}

// Compatibility aliases for older collision code naming.
inline constexpr float LenSq(const Vec3& v) { return LengthSq(v); }
inline float Len(const Vec3& v) { return Length(v); }

}} // namespace Engine::Math
