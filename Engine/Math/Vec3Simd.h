#pragma once
// =========================================================================
// SSOT: docs/contracts/math/vec3-contract.md
// REF: PhysX-style split: public Vec3 storage, internal Vec3V SIMD route.
//
// POLICY:
//   - Internal math backend only. Do not store Vec3V in public engine state.
//   - Vec3 remains the canonical scalar storage and fallback contract.
//   - No runtime CPU dispatch in v1; compile-time scalar/SSE route only.
// =========================================================================

#include "Vec3.h"

#if EL_MATH_ENABLE_SIMD
#include <immintrin.h>
#endif

namespace Engine { namespace Math { namespace simd {

#if EL_MATH_ENABLE_SIMD

struct FloatV { __m128 v; };
struct BoolV { __m128 v; };
struct Vec3V { __m128 v; };

EL_FORCE_INLINE FloatV Splat(float value) {
    FloatV out;
    out.v = _mm_set1_ps(value);
    return out;
}

EL_FORCE_INLINE float StoreF(const FloatV& value) {
    return _mm_cvtss_f32(value.v);
}

EL_FORCE_INLINE Vec3V Set(float x, float y, float z) {
    Vec3V out;
    out.v = _mm_set_ps(0.0f, z, y, x);
    return out;
}

EL_FORCE_INLINE Vec3V Splat3(float value) {
    return Set(value, value, value);
}

EL_FORCE_INLINE Vec3V LoadU(const Vec3& value) {
    return Set(value.x, value.y, value.z);
}

EL_FORCE_INLINE Vec3 StoreU(const Vec3V& value) {
    float lanes[4];
    _mm_storeu_ps(lanes, value.v);
    return {lanes[0], lanes[1], lanes[2]};
}

EL_FORCE_INLINE void StoreU(const Vec3V& value, Vec3& out) {
    out = StoreU(value);
}

EL_FORCE_INLINE Vec3V Add(const Vec3V& a, const Vec3V& b) {
    Vec3V out;
    out.v = _mm_add_ps(a.v, b.v);
    return out;
}

EL_FORCE_INLINE Vec3V Sub(const Vec3V& a, const Vec3V& b) {
    Vec3V out;
    out.v = _mm_sub_ps(a.v, b.v);
    return out;
}

EL_FORCE_INLINE Vec3V Scale(const Vec3V& value, const FloatV& scale) {
    Vec3V out;
    out.v = _mm_mul_ps(value.v, scale.v);
    return out;
}

EL_FORCE_INLINE Vec3V Scale(const Vec3V& value, float scale) {
    return Scale(value, Splat(scale));
}

EL_FORCE_INLINE Vec3V Mul(const Vec3V& a, const Vec3V& b) {
    Vec3V out;
    out.v = _mm_mul_ps(a.v, b.v);
    return out;
}

EL_FORCE_INLINE Vec3V Min(const Vec3V& a, const Vec3V& b) {
    Vec3V out;
    out.v = _mm_min_ps(a.v, b.v);
    return out;
}

EL_FORCE_INLINE Vec3V Max(const Vec3V& a, const Vec3V& b) {
    Vec3V out;
    out.v = _mm_max_ps(a.v, b.v);
    return out;
}

EL_FORCE_INLINE Vec3V Abs(const Vec3V& value) {
    Vec3V out;
    const __m128 signMask = _mm_set1_ps(-0.0f);
    out.v = _mm_andnot_ps(signMask, value.v);
    return out;
}

EL_FORCE_INLINE FloatV Dot3(const Vec3V& a, const Vec3V& b) {
    const __m128 product = _mm_mul_ps(a.v, b.v);
    float lanes[4];
    _mm_storeu_ps(lanes, product);
    return Splat(lanes[0] + lanes[1] + lanes[2]);
}

EL_FORCE_INLINE Vec3V Cross3(const Vec3V& a, const Vec3V& b) {
    const __m128 aYzx = _mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(3, 0, 2, 1));
    const __m128 bZxy = _mm_shuffle_ps(b.v, b.v, _MM_SHUFFLE(3, 1, 0, 2));
    const __m128 aZxy = _mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(3, 1, 0, 2));
    const __m128 bYzx = _mm_shuffle_ps(b.v, b.v, _MM_SHUFFLE(3, 0, 2, 1));

    Vec3V out;
    out.v = _mm_sub_ps(_mm_mul_ps(aYzx, bZxy), _mm_mul_ps(aZxy, bYzx));
    return out;
}

#else

struct FloatV { float v; };
struct BoolV { bool x, y, z, w; };
struct Vec3V { Vec3 v; };

EL_FORCE_INLINE FloatV Splat(float value) {
    return {value};
}

EL_FORCE_INLINE float StoreF(const FloatV& value) {
    return value.v;
}

EL_FORCE_INLINE Vec3V Set(float x, float y, float z) {
    return {{x, y, z}};
}

EL_FORCE_INLINE Vec3V Splat3(float value) {
    return Set(value, value, value);
}

EL_FORCE_INLINE Vec3V LoadU(const Vec3& value) {
    return {value};
}

EL_FORCE_INLINE Vec3 StoreU(const Vec3V& value) {
    return value.v;
}

EL_FORCE_INLINE void StoreU(const Vec3V& value, Vec3& out) {
    out = StoreU(value);
}

EL_FORCE_INLINE Vec3V Add(const Vec3V& a, const Vec3V& b) {
    return {a.v + b.v};
}

EL_FORCE_INLINE Vec3V Sub(const Vec3V& a, const Vec3V& b) {
    return {a.v - b.v};
}

EL_FORCE_INLINE Vec3V Scale(const Vec3V& value, const FloatV& scale) {
    return {value.v * scale.v};
}

EL_FORCE_INLINE Vec3V Scale(const Vec3V& value, float scale) {
    return {value.v * scale};
}

EL_FORCE_INLINE Vec3V Mul(const Vec3V& a, const Vec3V& b) {
    return {Engine::Math::Mul(a.v, b.v)};
}

EL_FORCE_INLINE Vec3V Min(const Vec3V& a, const Vec3V& b) {
    return {Engine::Math::Min(a.v, b.v)};
}

EL_FORCE_INLINE Vec3V Max(const Vec3V& a, const Vec3V& b) {
    return {Engine::Math::Max(a.v, b.v)};
}

EL_FORCE_INLINE Vec3V Abs(const Vec3V& value) {
    return {Engine::Math::Abs(value.v)};
}

EL_FORCE_INLINE FloatV Dot3(const Vec3V& a, const Vec3V& b) {
    return {Engine::Math::Dot(a.v, b.v)};
}

EL_FORCE_INLINE Vec3V Cross3(const Vec3V& a, const Vec3V& b) {
    return {Engine::Math::Cross(a.v, b.v)};
}

#endif

}}} // namespace Engine::Math::simd
