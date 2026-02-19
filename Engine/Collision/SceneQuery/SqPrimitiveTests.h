#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqPrimitiveTests.h
//
// TERMINOLOGY:
//   BPT     - Basic Primitive Tests (closest-point, overlap, sweep tests)
//   Plane   - n dot x = d (n assumed unit for signed distances)
//   SAT     - Separating Axis Theorem (13 axes for tri-vs-AABB)
//
// POLICY:
//   - All functions are pure: inputs only, no side effects.
//   - Epsilon constants sourced from SqMath.h (kEpsParallel, kEpsSq).
//   - Slab-method ray/segment tests follow Ericson convention.
//
// CONTRACT:
//   - Standalone: includes only SqTypes.h.
//   - Adapted from bpt:: namespace (ex2.cpp) into sq:: namespace.
//   - No behavioral changes from reference.
//
// PROOF POINTS:
//   - [PR3.3] TestSphereAABB: sphere centered on AABB face returns true
//   - [PR3.3] TestTriangleAABB: separating axis correctly rejects non-overlapping pairs
//
// REFERENCES:
//   - ex2.cpp lines 25-328 (golden SSOT, bpt_helpers.h)
//   - Ericson, RTCD Chapters 4-5
// =========================================================================

#include "SqTypes.h"
#include <limits>

namespace Engine { namespace Collision { namespace sq {

// ---- Small local helpers ------------------------------------------------

inline float Clamp(float x, float lo, float hi) { return (x < lo) ? lo : (x > hi ? hi : x); }
inline float Min3(float a, float b, float c) { return (std::min)(a, (std::min)(b, c)); }
inline float Max3(float a, float b, float c) { return (std::max)(a, (std::max)(b, c)); }

inline float AabbMin(const AABB& b, int axis) { return axis==0 ? b.minX : (axis==1 ? b.minY : b.minZ); }
inline float AabbMax(const AABB& b, int axis) { return axis==0 ? b.maxX : (axis==1 ? b.maxY : b.maxZ); }

inline Vec3 AabbExtents(const AABB& b) {
    Vec3 c = AABBCenter(b);
    return { b.maxX - c.x, b.maxY - c.y, b.maxZ - c.z };
}

inline AABB ExpandAabb(const AABB& b, float e) {
    return { b.minX - e, b.minY - e, b.minZ - e, b.maxX + e, b.maxY + e, b.maxZ + e };
}

// ---- Plane --------------------------------------------------------------

struct Plane {
    Vec3  n;   // normal
    float d;   // plane constant: n dot x = d
};

inline float PlaneSignedDistance(const Plane& p, const Vec3& x) {
    return Dot(p.n, x) - p.d;
}

// ---- Closest point to AABB / OBB ----------------------------------------

inline Vec3 ClosestPointPointAABB(const Vec3& p, const AABB& b) {
    return {
        Clamp(p.x, b.minX, b.maxX),
        Clamp(p.y, b.minY, b.maxY),
        Clamp(p.z, b.minZ, b.maxZ)
    };
}

inline float DistPointAABBSq(const Vec3& p, const AABB& b) {
    float sq = 0.0f;
    if (p.x < b.minX) { float d = b.minX - p.x; sq += d*d; }
    else if (p.x > b.maxX) { float d = p.x - b.maxX; sq += d*d; }
    if (p.y < b.minY) { float d = b.minY - p.y; sq += d*d; }
    else if (p.y > b.maxY) { float d = p.y - b.maxY; sq += d*d; }
    if (p.z < b.minZ) { float d = b.minZ - p.z; sq += d*d; }
    else if (p.z > b.maxZ) { float d = p.z - b.maxZ; sq += d*d; }
    return sq;
}

inline Vec3 ClosestPointPointOBB(const Vec3& p, const OBB& b) {
    Vec3 d = p - b.center;
    Vec3 q = b.center;

    float x = Dot(d, b.axisX); x = Clamp(x, -b.half.x, b.half.x); q = q + b.axisX * x;
    float y = Dot(d, b.axisY); y = Clamp(y, -b.half.y, b.half.y); q = q + b.axisY * y;
    float z = Dot(d, b.axisZ); z = Clamp(z, -b.half.z, b.half.z); q = q + b.axisZ * z;

    return q;
}

inline float DistPointOBBSq(const Vec3& p, const OBB& b) {
    Vec3 v = p - b.center;
    float sq = 0.0f;

    float d = Dot(v, b.axisX);
    float ex = (d < -b.half.x) ? (d + b.half.x) : (d > b.half.x ? (d - b.half.x) : 0.0f);
    sq += ex*ex;

    d = Dot(v, b.axisY);
    float ey = (d < -b.half.y) ? (d + b.half.y) : (d > b.half.y ? (d - b.half.y) : 0.0f);
    sq += ey*ey;

    d = Dot(v, b.axisZ);
    float ez = (d < -b.half.z) ? (d + b.half.z) : (d > b.half.z ? (d - b.half.z) : 0.0f);
    sq += ez*ez;

    return sq;
}

// ---- Sphere vs AABB / OBB -----------------------------------------------

inline bool TestSphereAABB(const Vec3& c, float r, const AABB& b) {
    return DistPointAABBSq(c, b) <= r*r;
}

inline bool TestSphereOBB(const Vec3& c, float r, const OBB& b) {
    return DistPointOBBSq(c, b) <= r*r;
}

// ---- Ray / Segment vs AABB (slab method) --------------------------------
// Ray: p(t) = p + d*t, t in [0, +inf)
// Segment: p(t) = p0 + (p1-p0)*t, t in [0, 1]

inline bool IntersectRayAABB(const Vec3& p, const Vec3& d, const AABB& a,
                              float& tmin, float& tmax)
{
    tmin = 0.0f;
    tmax = std::numeric_limits<float>::infinity();

    for (int i = 0; i < 3; ++i) {
        const float pi = (i==0 ? p.x : (i==1 ? p.y : p.z));
        const float di = (i==0 ? d.x : (i==1 ? d.y : d.z));
        const float minI = AabbMin(a, i);
        const float maxI = AabbMax(a, i);

        if (std::fabs(di) < kEpsParallel) {
            // Parallel: origin must be inside slab
            if (pi < minI || pi > maxI) return false;
            continue;
        }

        float ood = 1.0f / di;
        float t0 = (minI - pi) * ood;
        float t1 = (maxI - pi) * ood;
        if (t0 > t1) std::swap(t0, t1);

        tmin = (std::max)(tmin, t0);
        tmax = (std::min)(tmax, t1);
        if (tmin > tmax) return false;
    }
    return true;
}

inline bool IntersectSegmentAABB01(const Vec3& p0, const Vec3& p1, const AABB& a,
                                    float& outTmin, float& outTmax)
{
    Vec3 d = p1 - p0;
    float tmin, tmax;
    if (!IntersectRayAABB(p0, d, a, tmin, tmax)) return false;

    if (tmax < 0.0f || tmin > 1.0f) return false;
    outTmin = (std::max)(tmin, 0.0f);
    outTmax = (std::min)(tmax, 1.0f);
    return outTmin <= outTmax;
}

inline bool TestSegmentAABB(const Vec3& p0, const Vec3& p1, const AABB& a) {
    float t0, t1;
    return IntersectSegmentAABB01(p0, p1, a, t0, t1);
}

// ---- AABB / OBB vs Plane ------------------------------------------------

inline bool TestAABBPlane(const AABB& b, const Plane& p) {
    Vec3 c = AABBCenter(b);
    Vec3 e = AabbExtents(b);

    float r =
        e.x * std::fabs(p.n.x) +
        e.y * std::fabs(p.n.y) +
        e.z * std::fabs(p.n.z);

    float s = PlaneSignedDistance(p, c);
    return std::fabs(s) <= r;
}

inline bool TestOBBPlane(const OBB& b, const Plane& p) {
    float r =
        b.half.x * std::fabs(Dot(p.n, b.axisX)) +
        b.half.y * std::fabs(Dot(p.n, b.axisY)) +
        b.half.z * std::fabs(Dot(p.n, b.axisZ));

    float s = PlaneSignedDistance(p, b.center);
    return std::fabs(s) <= r;
}

// ---- Triangle vs AABB (SAT, 13 axes) ------------------------------------

inline void ProjectTriOntoAxis(const Triangle& t, const Vec3& axis,
                                float& outMin, float& outMax)
{
    float p0 = Dot(t.p0, axis);
    float p1 = Dot(t.p1, axis);
    float p2 = Dot(t.p2, axis);
    outMin = Min3(p0, p1, p2);
    outMax = Max3(p0, p1, p2);
}

inline void ProjectAabbOntoAxis(const AABB& b, const Vec3& axis,
                                 float& outMin, float& outMax)
{
    Vec3 c = AABBCenter(b);
    Vec3 e = AabbExtents(b);

    float cproj = Dot(c, axis);
    float r =
        e.x * std::fabs(axis.x) +
        e.y * std::fabs(axis.y) +
        e.z * std::fabs(axis.z);

    outMin = cproj - r;
    outMax = cproj + r;
}

inline bool OverlapOnAxis(const Triangle& t, const AABB& b, const Vec3& axis) {
    // Skip near-zero axis (degenerate cross products)
    if (LenSq(axis) <= kEpsSq) return true;

    float triMin, triMax;
    float boxMin, boxMax;
    ProjectTriOntoAxis(t, axis, triMin, triMax);
    ProjectAabbOntoAxis(b, axis, boxMin, boxMax);
    return !(triMax < boxMin || boxMax < triMin);
}

inline bool TestTriangleAABB(const Triangle& tri, const AABB& box) {
    // Move into box-centered space
    Vec3 c = AABBCenter(box);
    Triangle t{ tri.p0 - c, tri.p1 - c, tri.p2 - c };

    Vec3 e = AabbExtents(box);
    AABB bLocal{ -e.x, -e.y, -e.z, +e.x, +e.y, +e.z };

    Vec3 f0 = t.p1 - t.p0;
    Vec3 f1 = t.p2 - t.p1;
    Vec3 f2 = t.p0 - t.p2;

    // 1) AABB face normals
    if (!OverlapOnAxis(t, bLocal, {1,0,0})) return false;
    if (!OverlapOnAxis(t, bLocal, {0,1,0})) return false;
    if (!OverlapOnAxis(t, bLocal, {0,0,1})) return false;

    // 2) Triangle normal
    Vec3 n = Cross(f0, t.p2 - t.p0);
    if (!OverlapOnAxis(t, bLocal, n)) return false;

    // 3) 9 cross-product axes (AABB axes x tri edges)
    Vec3 axes[9] = {
        Cross({1,0,0}, f0), Cross({1,0,0}, f1), Cross({1,0,0}, f2),
        Cross({0,1,0}, f0), Cross({0,1,0}, f1), Cross({0,1,0}, f2),
        Cross({0,0,1}, f0), Cross({0,0,1}, f1), Cross({0,0,1}, f2),
    };
    for (int i = 0; i < 9; ++i)
        if (!OverlapOnAxis(t, bLocal, axes[i])) return false;

    return true;
}

// ---- Moving sphere vs AABB (expand + segment test) ----------------------

inline bool SweepSphereAABB_TOI01(const Vec3& c0, const Vec3& c1, float r,
                                   const AABB& box, float& outT)
{
    AABB expanded = ExpandAabb(box, r);

    float tmin, tmax;
    if (!IntersectSegmentAABB01(c0, c1, expanded, tmin, tmax)) return false;

    outT = tmin;
    return true;
}

}}} // namespace Engine::Collision::sq
