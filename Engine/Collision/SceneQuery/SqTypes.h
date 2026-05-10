#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqTypes.h
//
// TERMINOLOGY:
//   TOI       - Time Of Impact, parameter t in [0,1] along displacement delta
//   Feature   - sub-element of primitive that was hit (face/edge/vertex)
//   PrimRef   - reference to a primitive in the BVH (type + index + bounds)
//   Hit       - query result: whether a hit occurred, at what t, with what normal
//
// POLICY:
//   - No mutable statics. No globals.
//   - All geometry types are POD. All helpers are pure functions.
//   - AABB layout {minX,minY,minZ,maxX,maxY,maxZ} matches Engine::AABB.
//
// CONTRACT:
//   - Standalone: includes only SqMath.h + <cstdint> + <algorithm> + <vector>.
//   - No DirectXMath, no Engine:: types.
//
// PROOF POINTS:
//   - [PR3.1] static_assert(sizeof(AABB)==24)
//   - [PR3.1] AABB layout matches Engine::AABB (verified at integration seam)
//
// REFERENCES:
//   - ex.cpp lines 51-136 (golden SSOT)
// =========================================================================

#include "SqMath.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace Engine { namespace Collision { namespace sq {

// ---- Geometry primitives ------------------------------------------------

struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

constexpr AABB UnionAABB(const AABB& a, const AABB& b) {
    return {
        (std::min)(a.minX, b.minX), (std::min)(a.minY, b.minY), (std::min)(a.minZ, b.minZ),
        (std::max)(a.maxX, b.maxX), (std::max)(a.maxY, b.maxY), (std::max)(a.maxZ, b.maxZ)
    };
}

constexpr Vec3 AABBCenter(const AABB& b) {
    return {(b.minX+b.maxX)*0.5f, (b.minY+b.maxY)*0.5f, (b.minZ+b.maxZ)*0.5f};
}

struct OBB {
    Vec3 center;
    Vec3 axisX;   // unit
    Vec3 axisY;   // unit
    Vec3 axisZ;   // unit
    Vec3 half;    // extents along axes
};

inline AABB OBBWorldAABB(const OBB& o) {
    float hx = Abs(o.axisX.x)*o.half.x + Abs(o.axisY.x)*o.half.y + Abs(o.axisZ.x)*o.half.z;
    float hy = Abs(o.axisX.y)*o.half.x + Abs(o.axisY.y)*o.half.y + Abs(o.axisZ.y)*o.half.z;
    float hz = Abs(o.axisX.z)*o.half.x + Abs(o.axisY.z)*o.half.y + Abs(o.axisZ.z)*o.half.z;
    return {
        o.center.x-hx, o.center.y-hy, o.center.z-hz,
        o.center.x+hx, o.center.y+hy, o.center.z+hz
    };
}

struct Triangle { Vec3 p0, p1, p2; };

inline AABB TriAABB(const Triangle& t) {
    return {
        (std::min)(t.p0.x, (std::min)(t.p1.x, t.p2.x)),
        (std::min)(t.p0.y, (std::min)(t.p1.y, t.p2.y)),
        (std::min)(t.p0.z, (std::min)(t.p1.z, t.p2.z)),
        (std::max)(t.p0.x, (std::max)(t.p1.x, t.p2.x)),
        (std::max)(t.p0.y, (std::max)(t.p1.y, t.p2.y)),
        (std::max)(t.p0.z, (std::max)(t.p1.z, t.p2.z))
    };
}

inline Vec3 TriNormalUnit(const Triangle& t) {
    Vec3 e0 = t.p1 - t.p0;
    Vec3 e1 = t.p2 - t.p0;
    return NormalizeSafe(Cross(e0, e1), {0, 1, 0});
}

inline bool PointInTri(const Vec3& p, const Triangle& t, const Vec3& n) {
    Vec3 e0 = t.p1 - t.p0; Vec3 c0 = Cross(e0, p - t.p0);
    Vec3 e1 = t.p2 - t.p1; Vec3 c1 = Cross(e1, p - t.p1);
    Vec3 e2 = t.p0 - t.p2; Vec3 c2 = Cross(e2, p - t.p2);
    return Dot(c0, n) >= -kEpsPointInTri
        && Dot(c1, n) >= -kEpsPointInTri
        && Dot(c2, n) >= -kEpsPointInTri;
}

// ---- Primitive classification -------------------------------------------

enum class PrimType : uint8_t { Aabb = 0, Obb = 1, Tri = 2 };

struct PrimRef {
    PrimType type;
    uint32_t index;
    AABB     bounds;
    Vec3     centroid;
};

// ---- Query I/O ----------------------------------------------------------

struct Hit {
    bool     hit = false;
    float    t = 1.0f;
    PrimType type = PrimType::Tri;
    uint32_t index = 0;
    Vec3     normal{0, 1, 0};
    uint32_t featureId = 0;  // packed: (prismFace<<8) | (sphereTriFeature)
    // Invariant: skin is a movement pullback margin, not a penetration flag.
    // startPenetrating is the explicit raw fact for initial-overlap hits.
    bool     startPenetrating = false;
    float    penetrationDepth = 0.0f;
};

struct OverlapContact {
    Vec3     normal{0, 1, 0};
    float    depth    = 0.0f;
    PrimType type     = PrimType::Aabb;
    uint32_t index    = 0;
    uint32_t featureId = 0;
};

struct SweepCapsuleInput {
    Vec3  segA0;    // bottom sphere center at t=0
    Vec3  segB0;    // top sphere center at t=0
    float radius;   // capsule half-width
    Vec3  delta;    // displacement over t in [0,1]
};

struct SweepConfig {
    float skin       = 1e-4f;   // contact offset
    float tieEpsT    = 1e-6f;   // tolerance for t tie-breaks
    bool  twoSidedTris = true;
};

// ---- Sweep filter (Bullet-equivalent callback predicate) ----------------
// Applied inside SweepCapsuleClosestHit_Fast after narrowphase, before
// BetterHit selection. Rejects candidates where Dot(hitNormal, refDir) < minDot.
//
// Per-stage usage (matches Bullet btKinematicClosestNotMeConvexResultCallback):
//   StepUp:   refDir = -up,  minDot = maxSlopeCos  (only ceilings block)
//   StepMove: refDir = -dir, minDot = ε_approach    (only approach surfaces block)
//   StepDown: refDir = +up,  minDot = maxSlopeCos  (only walkable ground blocks)
//
// Default: active=false → no filtering → backward compatible.

struct SweepFilter {
    Vec3  refDir{0, 1, 0};  // reference direction for dot test
    float minDot = -2.0f;   // minimum Dot(hitNormal, refDir) to accept
    bool  active = false;    // false = no filtering
    bool  filterInitialOverlap = false; // apply filter on t==0 when rejectInitialOverlap=false
};

static_assert(sizeof(AABB) == 24, "AABB must be 24 bytes POD");

}}} // namespace Engine::Collision::sq
