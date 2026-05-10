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
//   - AabbAabb_SweepInterval: in/out interval refinement on caller range.
//   - AabbAabb_SweepInterval01: returns true if overlap window exists in [0,1].
//   - CapsuleAabbAtT: produces conservative AABB for capsule at time t.
//
// PROOF POINTS:
//   - [PR3.5] AabbAabb_SweepInterval01: stationary overlap returns [0,1]
//   - [PR3.5] CapsuleAabbAtT: bounds contain both sphere endpoints at time t
//
// REFERENCES:
//   - docs/agent-context/scenequery-refactor.md
//   - docs/reference/physx/contracts/scenequery-pipeline.md
//   - Ericson, RTCD Section 5.5.8
// =========================================================================

#include "SqTypes.h"

namespace Engine { namespace Collision { namespace sq {

// ---- Time-window: moving AABB vs static AABB ----------------------------
// General in/out form.
// a(t) = a0 + v*t. Caller provides an initial time window [tEnter, tExit].
// This function intersects that window with this AABB pair's overlap window.
inline bool AabbAabb_SweepInterval(const AABB& a0, const Vec3& v, const AABB& b,
                                   float& tEnter, float& tExit)
{
    if (tEnter > tExit) return false;

    auto axis = [&](float aMin, float aMax, float vel, float bMin, float bMax) -> bool {
        if (Abs(vel) < kEpsParallel) {
            // No relative motion on this axis -> must already overlap on this axis.
            return !(aMax < bMin || aMin > bMax);
        }

        // What this axis solves:
        //   A(t) = [aMin + vel*t, aMax + vel*t], B = [bMin, bMax]
        //   overlap <=> (aMin + vel*t <= bMax) and (aMax + vel*t >= bMin)
        //
        // Rearranged bounds use:
        //   t0 = (bMin - aMax) / vel
        //   t1 = (bMax - aMin) / vel
        //
        // If vel < 0, inequality direction flips automatically through min/max,
        // so negative velocity needs no special-case branch.
        const float t0 = (bMin - aMax) / vel;
        const float t1 = (bMax - aMin) / vel;
        const float axisEnter = (std::min)(t0, t1);
        const float axisExit = (std::max)(t0, t1);

        tEnter = (std::max)(tEnter, axisEnter);
        tExit = (std::min)(tExit, axisExit);
        return tEnter <= tExit;
    };

    if (!axis(a0.minX, a0.maxX, v.x, b.minX, b.maxX)) return false;
    if (!axis(a0.minY, a0.maxY, v.y, b.minY, b.maxY)) return false;
    if (!axis(a0.minZ, a0.maxZ, v.z, b.minZ, b.maxZ)) return false;

    return tEnter <= tExit;
}

// Normalized [0,1] wrapper.
inline bool AabbAabb_SweepInterval01(const AABB& a0, const Vec3& v, const AABB& b,
                                     float& tEnter, float& tExit)
{
    tEnter = 0.0f;
    tExit = 1.0f;
    return AabbAabb_SweepInterval(a0, v, b, tEnter, tExit);
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

// ---- Static capsule AABB (no sweep, just position) ----------------------
inline AABB CapsuleAabbStatic(const Vec3& segA, const Vec3& segB, float radius)
{
    return {
        (std::min)(segA.x, segB.x) - radius,
        (std::min)(segA.y, segB.y) - radius,
        (std::min)(segA.z, segB.z) - radius,
        (std::max)(segA.x, segB.x) + radius,
        (std::max)(segA.y, segB.y) + radius,
        (std::max)(segA.z, segB.z) + radius
    };
}

}}} // namespace Engine::Collision::sq
