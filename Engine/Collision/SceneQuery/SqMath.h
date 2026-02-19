#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqMath.h
//
// TERMINOLOGY:
//   Vec3 - 3-component float vector, POD layout {x,y,z}
//
// POLICY:
//   - SCALAR-ONLY. No SIMD intrinsics, no platform headers.
//   - No mutable statics. No globals.
//   - All epsilons live here as inline constexpr namespace constants (SSOT).
//   - NormalizeSafe is NaN-safe: NaN inputs produce fallback, never leak.
//
// CONTRACT:
//   - This header is standalone: only <cmath>.
//   - Vec3 operators and helpers are pure functions.
//   - SIMD acceleration is reserved for a future SqMathSimd.h layer
//     that includes this header and is gated by build flags.
//     SqMathSimd.h will provide overloads (e.g., Dot4, Cross4) that
//     operate on SIMD-loaded Vec3 batches. This header's scalar
//     signatures remain the canonical fallback.
//
// PROOF POINTS:
//   - [PR3.1] static_assert(sizeof(Vec3)==12)
//   - [PR3.1] NormalizeSafe({0,0,0}, fb) == fb
//   - [PR3.1] NormalizeSafe({NaN,...}, fb) == fb
//
// REFERENCES:
//   - ex.cpp lines 28-46 (golden SSOT)
// =========================================================================

#include <cmath>

namespace Engine { namespace Collision { namespace sq {

// ---- Constants (SSOT for all SceneQuery epsilons) -----------------------
inline constexpr float kEpsSq       = 1e-20f;  // squared-length threshold for degenerate vectors
inline constexpr float kEpsParallel = 1e-12f;  // near-zero velocity/direction component
inline constexpr float kEpsPointInTri = 1e-6f; // barycentric winding tolerance

// ---- Vec3 POD -----------------------------------------------------------
struct Vec3 { float x{}, y{}, z{}; };

constexpr Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
constexpr Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
constexpr Vec3 operator*(const Vec3& a, float s) { return {a.x*s, a.y*s, a.z*s}; }

constexpr float Dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
constexpr Vec3  Cross(const Vec3& a, const Vec3& b) {
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}
constexpr float LenSq(const Vec3& v) { return Dot(v,v); }
inline float Len(const Vec3& v) { return std::sqrt(LenSq(v)); }
constexpr float Abs(float x) { return x < 0 ? -x : x; }

// NaN-safe normalize: if l2 is NaN, too small, or negative, returns fallback.
// The negated comparison !(l2 > kEpsSq) catches NaN because NaN > x is false.
inline Vec3 NormalizeSafe(const Vec3& v, const Vec3& fallback) {
    float l2 = LenSq(v);
    if (!(l2 > kEpsSq)) return fallback;
    float inv = 1.0f / std::sqrt(l2);
    return v * inv;
}

static_assert(sizeof(Vec3) == 12, "Vec3 must be 12 bytes POD");

}}} // namespace Engine::Collision::sq
