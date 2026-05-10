#pragma once
// =========================================================================
// SSOT: docs/contracts/math/transform-contract.md
// REF: PhysX PxTransform/PxQuat public boundary: scalar pose = quat + position.
//
// POLICY:
//   - Public/storage types stay scalar and trivially copyable.
//   - RigidTransform is pose-only: no scale, no shear, no projective state.
//   - Quaternion operations assume unit/sane rotation unless a function says it
//     normalizes or validates.
// =========================================================================

#include "Vec3.h"

#include <cassert>
#include <cmath>
#include <type_traits>

namespace Engine { namespace Math {

inline constexpr float kQuatUnitTolerance = 1e-4f;
inline constexpr float kQuatSaneTolerance = 1e-2f;
inline constexpr float kQuatNormalizeEpsSq = 1e-20f;

struct Quat {
    float x{};
    float y{};
    float z{};
    float w{1.0f};
};

struct RigidTransform {
    Vec3 position{};
    Quat rotation{};
};

static_assert(sizeof(Quat) == 16, "Quat must remain 16 bytes");
static_assert(alignof(Quat) == alignof(float), "Quat must keep float alignment");
static_assert(std::is_standard_layout<Quat>::value, "Quat must be standard layout");
static_assert(std::is_trivially_copyable<Quat>::value, "Quat must be trivially copyable");

static_assert(sizeof(RigidTransform) == 28, "RigidTransform must be Vec3 + Quat scalar storage");
static_assert(alignof(RigidTransform) == alignof(float), "RigidTransform must keep float alignment");
static_assert(std::is_standard_layout<RigidTransform>::value, "RigidTransform must be standard layout");
static_assert(std::is_trivially_copyable<RigidTransform>::value, "RigidTransform must be trivially copyable");

inline constexpr Quat IdentityQuat() {
    return {0.0f, 0.0f, 0.0f, 1.0f};
}

inline constexpr RigidTransform IdentityRigidTransform() {
    return {Zero3(), IdentityQuat()};
}

inline constexpr bool operator==(const Quat& a, const Quat& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

inline constexpr bool operator!=(const Quat& a, const Quat& b) {
    return !(a == b);
}

inline constexpr float QuatDot(const Quat& a, const Quat& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline constexpr float QuatMagnitudeSq(const Quat& q) {
    return QuatDot(q, q);
}

inline float QuatMagnitude(const Quat& q) {
    return std::sqrt(QuatMagnitudeSq(q));
}

inline bool IsFinite(const Quat& q) {
    return std::isfinite(q.x) && std::isfinite(q.y) &&
           std::isfinite(q.z) && std::isfinite(q.w);
}

inline bool IsFinite(const RigidTransform& t) {
    return IsFinite(t.position) && IsFinite(t.rotation);
}

inline bool IsUnit(const Quat& q, float tolerance = kQuatUnitTolerance) {
    return IsFinite(q) && Abs(QuatMagnitude(q) - 1.0f) <= tolerance;
}

inline bool IsSane(const Quat& q, float tolerance = kQuatSaneTolerance) {
    return IsFinite(q) && Abs(QuatMagnitude(q) - 1.0f) <= tolerance;
}

inline bool IsValid(const RigidTransform& t) {
    return IsFinite(t.position) && IsUnit(t.rotation);
}

inline bool IsSane(const RigidTransform& t) {
    return IsFinite(t.position) && IsSane(t.rotation);
}

inline Quat NormalizeQuatSafe(const Quat& q, Quat fallback = IdentityQuat()) {
    const float magSq = QuatMagnitudeSq(q);
    if (!IsFinite(q) || !(magSq > kQuatNormalizeEpsSq)) {
        return fallback;
    }

    const float invMag = 1.0f / std::sqrt(magSq);
    const Quat out{q.x * invMag, q.y * invMag, q.z * invMag, q.w * invMag};
    return IsFinite(out) ? out : fallback;
}

inline RigidTransform NormalizeRigidTransformSafe(
    const RigidTransform& t,
    RigidTransform fallback = IdentityRigidTransform())
{
    if (!IsFinite(t.position)) {
        return fallback;
    }

    return {t.position, NormalizeQuatSafe(t.rotation)};
}

inline Quat QuatFromAxisAngleUnit(float angleRad, const Vec3& unitAxis) {
    assert(IsNormalized(unitAxis) && "QuatFromAxisAngleUnit requires a unit axis");
    const float half = angleRad * 0.5f;
    const float s = std::sin(half);
    return NormalizeQuatSafe({
        unitAxis.x * s,
        unitAxis.y * s,
        unitAxis.z * s,
        std::cos(half)
    });
}

inline Quat QuatFromYawY(float yawRad) {
    const float half = yawRad * 0.5f;
    return NormalizeQuatSafe({0.0f, std::sin(half), 0.0f, std::cos(half)});
}

inline constexpr Quat Conjugate(const Quat& q) {
    return {-q.x, -q.y, -q.z, q.w};
}

inline constexpr Quat operator*(const Quat& a, const Quat& b) {
    return {
        a.w * b.x + b.w * a.x + a.y * b.z - b.y * a.z,
        a.w * b.y + b.w * a.y + a.z * b.x - b.z * a.x,
        a.w * b.z + b.w * a.z + a.x * b.y - b.x * a.y,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
}

inline Vec3 Rotate(const Quat& q, const Vec3& v) {
    assert(IsSane(q) && "Rotate requires a sane unit quaternion");
    const Vec3 qv{q.x, q.y, q.z};
    const Vec3 t = Cross(qv, v) * 2.0f;
    return v + t * q.w + Cross(qv, t);
}

inline Vec3 RotateInv(const Quat& q, const Vec3& v) {
    assert(IsSane(q) && "RotateInv requires a sane unit quaternion");
    return Rotate(Conjugate(q), v);
}

inline Vec3 TransformPoint(const RigidTransform& t, const Vec3& point) {
    assert(IsSane(t) && "TransformPoint requires a sane rigid transform");
    return Rotate(t.rotation, point) + t.position;
}

inline Vec3 TransformPointInv(const RigidTransform& t, const Vec3& point) {
    assert(IsSane(t) && "TransformPointInv requires a sane rigid transform");
    return RotateInv(t.rotation, point - t.position);
}

inline Vec3 TransformVector(const RigidTransform& t, const Vec3& vector) {
    assert(IsSane(t) && "TransformVector requires a sane rigid transform");
    return Rotate(t.rotation, vector);
}

inline Vec3 TransformVectorInv(const RigidTransform& t, const Vec3& vector) {
    assert(IsSane(t) && "TransformVectorInv requires a sane rigid transform");
    return RotateInv(t.rotation, vector);
}

inline RigidTransform Inverse(const RigidTransform& t) {
    assert(IsSane(t) && "Inverse requires a sane rigid transform");
    const Quat qInv = Conjugate(t.rotation);
    return {Rotate(qInv, -t.position), qInv};
}

inline RigidTransform Compose(const RigidTransform& parent, const RigidTransform& child) {
    assert(IsSane(parent) && "Compose requires a sane parent transform");
    assert(IsSane(child) && "Compose requires a sane child transform");
    return {
        TransformPoint(parent, child.position),
        NormalizeQuatSafe(parent.rotation * child.rotation)
    };
}

}} // namespace Engine::Math
