#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqBroadphase.h
//
// TERMINOLOGY:
//   Time-window pruning - Ericson 5.5.8 style: compute [tEnter, tExit] for
//                         moving AABB vs static AABB, prune nodes whose
//                         window doesn't overlap the valid query range.
//   CapsuleAabbAtT      - compute the world AABB of a capsule at time t
//                         (translation only, expanded by radius + extra).
//
// POLICY:
//   - All functions are pure. No side effects.
//   - Time parameter t in [0,1].
//
// CONTRACT:
//   - Standalone: includes only SqTypes.h.
//   - AabbAabb_SweepInterval01: returns true if overlap window exists in [0,1].
//   - CapsuleAabbAtT: produces conservative AABB for capsule at time t.
//
// PROOF POINTS:
//   - [PR3.5] AabbAabb_SweepInterval01: stationary overlap returns [0,1]
//   - [PR3.5] CapsuleAabbAtT: bounds contain both sphere endpoints at time t
//
// REFERENCES:
//   - ex.cpp lines 264-306 (golden SSOT)
//   - Ericson, RTCD Section 5.5.8
// =========================================================================

#include "SqTypes.h"

namespace Engine { namespace Collision { namespace sq {

// ---- Time-window: moving AABB vs static AABB ----------------------------
// a(t) = a0 + v*t, t in [0,1].
// Returns true if overlap window [tEnter, tExit] exists and intersects [0,1].
inline bool AabbAabb_SweepInterval01(const AABB& a0, const Vec3& v, const AABB& b,
                                      float& tEnter, float& tExit)
{
    tEnter = 0.0f; tExit = 1.0f;

    auto axis = [&](float aMin, float aMax, float vel, float bMin, float bMax) -> bool {
        if (Abs(vel) < kEpsParallel) return !(aMax < bMin || aMin > bMax);
        float t0 = (bMin - aMax) / vel;
        float t1 = (bMax - aMin) / vel;
        float enter = (std::min)(t0, t1);
        float exit  = (std::max)(t0, t1);
        tEnter = (std::max)(tEnter, enter);
        tExit  = (std::min)(tExit,  exit);
        return tEnter <= tExit;
    };

    if (!axis(a0.minX, a0.maxX, v.x, b.minX, b.maxX)) return false;
    if (!axis(a0.minY, a0.maxY, v.y, b.minY, b.maxY)) return false;
    if (!axis(a0.minZ, a0.maxZ, v.z, b.minZ, b.maxZ)) return false;

    if (tExit < 0.0f || tEnter > 1.0f) return false;
    tEnter = (std::max)(tEnter, 0.0f);
    tExit  = (std::min)(tExit,  1.0f);
    return tEnter <= tExit;
}

// ---- Capsule AABB at time t (translation only) --------------------------
// Computes world AABB of capsule at time t, expanded by (radius + extra).
// extra is typically SweepConfig::skin.
inline AABB CapsuleAabbAtT(const SweepCapsuleInput& in, float t, float extra)
{
    Vec3 A = in.segA0 + in.delta * t;
    Vec3 B = in.segB0 + in.delta * t;

    float r = in.radius + extra;
    return {
        (std::min)(A.x, B.x) - r,
        (std::min)(A.y, B.y) - r,
        (std::min)(A.z, B.z) - r,
        (std::max)(A.x, B.x) + r,
        (std::max)(A.y, B.y) + r,
        (std::max)(A.z, B.z) + r
    };
}

}}} // namespace Engine::Collision::sq
