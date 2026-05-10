#include "Vec3SimdSelfTest.h"

#include "Vec3Simd.h"

#include <cassert>

namespace Engine { namespace Math {

namespace {

constexpr float kParityEps = 1e-5f;

void ExpectNear(float actual, float expected) {
    assert(Abs(actual - expected) <= kParityEps);
}

void ExpectVecNear(const Vec3& actual, const Vec3& expected) {
    assert(NearEqual(actual, expected, kParityEps));
}

Vec3 Store(const simd::Vec3V& value) {
    return simd::StoreU(value);
}

} // namespace

void RunVec3SimdSelfTest() {
#ifndef _DEBUG
    return;
#else
    static bool s_ran = false;
    if (s_ran) {
        return;
    }
    s_ran = true;

    const Vec3 a{1.0f, -2.0f, 3.5f};
    const Vec3 b{-4.0f, 0.25f, 8.0f};
    const Vec3 c{0.0f, -0.0f, -7.25f};

    const simd::Vec3V av = simd::LoadU(a);
    const simd::Vec3V bv = simd::LoadU(b);
    const simd::Vec3V cv = simd::LoadU(c);

    ExpectVecNear(Store(av), a);
    ExpectVecNear(Store(simd::Add(av, bv)), a + b);
    ExpectVecNear(Store(simd::Sub(av, bv)), a - b);
    ExpectVecNear(Store(simd::Scale(av, -2.0f)), a * -2.0f);
    ExpectVecNear(Store(simd::Scale(av, simd::Splat(0.5f))), a * 0.5f);
    ExpectVecNear(Store(simd::Mul(av, bv)), Mul(a, b));
    ExpectVecNear(Store(simd::Min(av, bv)), Min(a, b));
    ExpectVecNear(Store(simd::Max(av, bv)), Max(a, b));
    ExpectVecNear(Store(simd::Abs(cv)), Abs(c));

    ExpectNear(simd::StoreF(simd::Dot3(av, bv)), Dot(a, b));
    ExpectVecNear(Store(simd::Cross3(av, bv)), Cross(a, b));

    const Vec3 zero = Zero3();
    ExpectVecNear(Store(simd::Add(simd::LoadU(zero), simd::Splat3(1.0f))), One3());
#endif
}

}} // namespace Engine::Math
