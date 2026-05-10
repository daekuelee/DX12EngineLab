// Compile-only proof that Engine/Math/Transform.h is self-contained.

#include "Transform.h"

#include <cassert>
#include <type_traits>

namespace {
using Engine::Math::Quat;
using Engine::Math::RigidTransform;
using Engine::Math::Vec3;

static_assert(sizeof(Quat) == 16, "Quat must remain 16 bytes");
static_assert(alignof(Quat) == alignof(float), "Quat must keep float alignment");
static_assert(std::is_standard_layout<Quat>::value, "Quat must be standard layout");
static_assert(std::is_trivially_copyable<Quat>::value, "Quat must be trivially copyable");

static_assert(sizeof(RigidTransform) == 28, "RigidTransform must remain Vec3 + Quat");
static_assert(alignof(RigidTransform) == alignof(float), "RigidTransform must keep float alignment");
static_assert(std::is_standard_layout<RigidTransform>::value, "RigidTransform must be standard layout");
static_assert(std::is_trivially_copyable<RigidTransform>::value, "RigidTransform must be trivially copyable");

constexpr Quat kIdentityQuat = Engine::Math::IdentityQuat();
static_assert(kIdentityQuat == Quat{0.0f, 0.0f, 0.0f, 1.0f}, "IdentityQuat changed");

constexpr Quat kMulIdentity = kIdentityQuat * kIdentityQuat;
static_assert(kMulIdentity == kIdentityQuat, "Quat identity multiplication changed");

bool Near(float a, float b, float tolerance = 1e-4f) {
    return Engine::Math::Abs(a - b) <= tolerance;
}

bool NearVec(const Vec3& a, const Vec3& b, float tolerance = 1e-4f) {
    return Near(a.x, b.x, tolerance) &&
           Near(a.y, b.y, tolerance) &&
           Near(a.z, b.z, tolerance);
}

bool RunTransformContractChecks() {
    const RigidTransform identity = Engine::Math::IdentityRigidTransform();
    assert(Engine::Math::IsValid(identity));
    assert(NearVec(Engine::Math::TransformPoint(identity, {1.0f, 2.0f, 3.0f}),
                   {1.0f, 2.0f, 3.0f}));
    assert(NearVec(Engine::Math::TransformVector(identity, {1.0f, 2.0f, 3.0f}),
                   {1.0f, 2.0f, 3.0f}));

    const Quat yaw0 = Engine::Math::QuatFromYawY(0.0f);
    assert(Engine::Math::IsUnit(yaw0));
    assert(yaw0 == kIdentityQuat);

    const float pi = 3.14159265358979323846f;
    const Quat yaw90 = Engine::Math::QuatFromYawY(pi * 0.5f);
    assert(Engine::Math::IsUnit(yaw90));
    assert(NearVec(Engine::Math::Rotate(yaw90, {0.0f, 0.0f, 1.0f}),
                   {1.0f, 0.0f, 0.0f}));

    const RigidTransform parent{{10.0f, 0.0f, 0.0f}, yaw90};
    const RigidTransform child{{0.0f, 0.0f, 2.0f}, Engine::Math::IdentityQuat()};
    const RigidTransform composed = Engine::Math::Compose(parent, child);
    assert(NearVec(composed.position, {12.0f, 0.0f, 0.0f}));

    const RigidTransform inv = Engine::Math::Inverse(parent);
    const Vec3 roundTrip = Engine::Math::TransformPoint(inv,
        Engine::Math::TransformPoint(parent, {2.0f, 3.0f, 4.0f}));
    assert(NearVec(roundTrip, {2.0f, 3.0f, 4.0f}));

    const Quat fallback = Engine::Math::NormalizeQuatSafe({0.0f, 0.0f, 0.0f, 0.0f});
    assert(fallback == kIdentityQuat);

    return true;
}
}

int Transform_compilecheck_dummy() {
    return RunTransformContractChecks() ? 0 : 1;
}
