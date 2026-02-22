#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqNarrowphase.h
//
// TERMINOLOGY:
//   TOI             - Time Of Impact, t in [0,1]
//   Extrusion prism - triangle offset by capsule half-segment vector, forming
//                     7 faces (1 cap + 3 edge quads as 6 tris)
//   Feature         - sub-element hit: 0=face, 1-3=edges, 4-6=vertices
//   FeatureId       - packed bits: (prismFace<<8)|(sphereTriFeature) for triangles;
//                     (triId<<16)|(prismFace<<8)|(feat) for boxes
//
// POLICY:
//   - Epsilon values come from SweepConfig (skin, tieEpsT) or SqMath.h constants.
//   - No ad-hoc float literals except documented numerical guards.
//   - Tie-break cascade: t -> alignment -> featureId (within narrowphase).
//   - All functions are pure. No side effects, no mutable statics.
//
// CONTRACT:
//   - Standalone: includes SqDistance.h and SqIntersect.h (transitive SqTypes.h).
//   - PhysX-like capsule-triangle: extrude triangle by half-segment, sweep
//     sphere center against prism faces.
//   - Box support: triangulate AABB/OBB surface into 12 triangles, then
//     apply same extrusion+sweep per surface triangle.
//   - Initial overlap: segment-triangle distance check at t=0.
//   - Colinear shortcut: if capsule axis || motion, use front-sphere only.
//
// PROOF POINTS:
//   - [PR3.4] SweepSphereTri: face hit normal opposes motion direction
//   - [PR3.4] SweepCapsuleTri: initial overlap returns t=0, feat=0xFFFFFFFF
//   - [PR3.4] Box triangulation: 12 tris cover all 6 faces (2 tris each)
//
// REFERENCES:
//   - ex.cpp lines 578-1137 (golden SSOT)
//   - PhysX capsule-triangle sweep (GJK-free prism decomposition)
// =========================================================================

#include "SqDistance.h"
#include "SqIntersect.h"
#include "SqPrimitiveTests.h"  // ClosestPointPointAABB (used by OverlapCapsuleAabb)
#include <limits>

namespace Engine { namespace Collision { namespace sq {

// ---- Epsilon constants for narrowphase ----------------------------------
inline constexpr float kNpEpsAlign = 1e-6f;   // alignment tie-break tolerance
inline constexpr float kNpColinearEps = 1e-4f; // capsule axis || motion threshold

// =========================================================================
// Sphere vs Triangle TOI (7 features: face + 3 edges + 3 vertices)
// =========================================================================
//
// Consumes: sphere center c0, radius r, displacement delta, triangle tri
// Produces: earliest t in [0,1], contact normal (opposes motion), feature index
//
// Feature encoding:
//   0 = face (plane hit), 1-3 = edge cylinders, 4-6 = vertex spheres
//
// Tie-break: smallest t; within tieEpsT: largest alignment; then smallest feature ID.
inline bool SweepSphereTri_TOI01(const Vec3& c0, float r, const Vec3& delta,
                                 const Triangle& tri, bool twoSided,
                                 float& outT, Vec3& outN, uint32_t& outF)
{
    const float epsT = kNpEpsAlign;

    Vec3 p1 = c0 + delta;

    // Initial overlap: true sphere-triangle distance
    Vec3 q0;
    if (DistPointTriangleSq(c0, tri, &q0) <= r*r) {
        outT = 0.0f;
        Vec3 fallback = NormalizeSafe(delta * -1.0f, {0,1,0});
        outN = NormalizeSafe(c0 - q0, fallback);
        outF = 0;
        if (Dot(outN, delta) > 0.0f) outN = outN * -1.0f;
        return true;
    }

    // Degenerate triangle detection
    Vec3 nd = Cross(tri.p1 - tri.p0, tri.p2 - tri.p0);
    float nd2 = LenSq(nd);
    bool degenerate = (nd2 <= kEpsSq);
    Vec3 n = degenerate ? Vec3{0,1,0} : NormalizeSafe(nd, {0,1,0});

    // One-sided: cull whole triangle if backfacing motion
    if (!twoSided) {
        if (LenSq(delta) > kEpsSq && Dot(n, delta) > 0.0f) return false;
    }

    Vec3 dirU = NormalizeSafe(delta, {0,1,0});

    float bestT = std::numeric_limits<float>::infinity();
    float bestAlign = -std::numeric_limits<float>::infinity();
    Vec3  bestN{0,1,0};
    uint32_t bestF = 0;

    auto consider = [&](float tCand, const Vec3& nCand, uint32_t fCand) {
        if (tCand < 0.0f || tCand > 1.0f) return;

        Vec3 nn = NormalizeSafe(nCand, n);
        float align = -Dot(nn, dirU);

        if (tCand < bestT - epsT) {
            bestT = tCand; bestN = nn; bestF = fCand; bestAlign = align;
            return;
        }

        if (Abs(tCand - bestT) <= epsT) {
            if (align > bestAlign + kNpEpsAlign) {
                bestT = tCand; bestN = nn; bestF = fCand; bestAlign = align;
                return;
            }
            if (Abs(align - bestAlign) <= kNpEpsAlign && fCand < bestF) {
                bestT = tCand; bestN = nn; bestF = fCand; bestAlign = align;
                return;
            }
        }
    };

    // 1) Face candidate (plane intersection)
    if (!degenerate) {
        float dist0 = Dot(n, c0 - tri.p0);
        float distV = Dot(n, delta);

        if (Abs(distV) > kEpsParallel) {
            auto tryPlane = [&](float targetDist, uint32_t fId) {
                float t = (targetDist - dist0) / distV;
                if (t < 0.0f || t > 1.0f) return;

                Vec3 ct = c0 + delta*t;
                float distT = Dot(n, ct - tri.p0);
                Vec3 proj = ct - n*distT;

                if (PointInTri(proj, tri, n)) {
                    Vec3 nFace = (distT >= 0.0f) ? n : (n * -1.0f);
                    consider(t, nFace, fId);
                }
            };

            if (twoSided) {
                tryPlane(+r, 0);
                tryPlane(-r, 0);
            } else {
                if (dist0 > r && distV < 0.0f) tryPlane(+r, 0);
            }
        }
    }

    // 2) Edge cylinders (features 1-3)
    float tEdge;

    if (IntersectSegmentCylinder01(c0, p1, tri.p0, tri.p1, r, tEdge)) {
        Vec3 ct = c0 + delta*tEdge;
        Vec3 q = ClosestPointOnSegment(tri.p0, tri.p1, ct);
        consider(tEdge, ct - q, 1);
    }
    if (IntersectSegmentCylinder01(c0, p1, tri.p1, tri.p2, r, tEdge)) {
        Vec3 ct = c0 + delta*tEdge;
        Vec3 q = ClosestPointOnSegment(tri.p1, tri.p2, ct);
        consider(tEdge, ct - q, 2);
    }
    if (IntersectSegmentCylinder01(c0, p1, tri.p2, tri.p0, r, tEdge)) {
        Vec3 ct = c0 + delta*tEdge;
        Vec3 q = ClosestPointOnSegment(tri.p2, tri.p0, ct);
        consider(tEdge, ct - q, 3);
    }

    // 3) Vertex spheres (features 4-6)
    float tV;
    if (IntersectSegmentSphere01(c0, p1, tri.p0, r, tV)) {
        Vec3 ct = c0 + delta*tV;
        consider(tV, ct - tri.p0, 4);
    }
    if (IntersectSegmentSphere01(c0, p1, tri.p1, r, tV)) {
        Vec3 ct = c0 + delta*tV;
        consider(tV, ct - tri.p1, 5);
    }
    if (IntersectSegmentSphere01(c0, p1, tri.p2, r, tV)) {
        Vec3 ct = c0 + delta*tV;
        consider(tV, ct - tri.p2, 6);
    }

    if (!std::isfinite(bestT)) return false;

    outT = bestT;
    outN = bestN;
    outF = bestF;

    // Normal opposed to motion
    if (Dot(outN, delta) > 0.0f) outN = outN * -1.0f;

    return true;
}

// =========================================================================
// PhysX-like capsule vs triangle: extrude + sweep
// =========================================================================

// Build 7 extruded prism faces from triangle ± half-segment vector a.
// Face 0: one end-cap (chosen based on normal direction).
// Faces 1-6: three edge quads (two tris each).
inline uint32_t BuildExtrudedFaces7(const Triangle& src, const Vec3& a,
                                     Triangle outFaces[7])
{
    Vec3 p0  = src.p0 - a;
    Vec3 p1  = src.p1 - a;
    Vec3 p2  = src.p2 - a;
    Vec3 p0b = src.p0 + a;
    Vec3 p1b = src.p1 + a;
    Vec3 p2b = src.p2 + a;

    Vec3 nSrc = Cross(src.p1 - src.p0, src.p2 - src.p0);  // denormalized
    uint32_t k = 0;

    // One cap
    if (Dot(nSrc, a) >= 0.0f) outFaces[k++] = { p0b, p1b, p2b };
    else                       outFaces[k++] = { p0,  p1,  p2  };

    // Edge 1-2 quad
    outFaces[k++] = { p1,  p1b, p2b };
    outFaces[k++] = { p1,  p2b, p2  };

    // Edge 2-0 quad
    outFaces[k++] = { p2,  p2b, p0b };
    outFaces[k++] = { p2,  p0b, p0  };

    // Edge 0-1 quad
    outFaces[k++] = { p0,  p0b, p1b };
    outFaces[k++] = { p0,  p1b, p1  };

    return k;
}

// Capsule vs triangle TOI using PhysX-like extrusion.
//
// Consumes: SweepCapsuleInput (segA0, segB0, radius, delta), source triangle, config
// Produces: earliest t in [0,1], contact normal, packed featureId
//
// Algorithm:
//   1. Check initial overlap via DistSegmentTriangleSq
//   2. Degenerate capsule -> fallback to sphere sweep
//   3. Colinear shortcut: capsule axis || motion -> front-sphere only
//   4. Extrude triangle by half-segment -> 7 prism faces
//   5. Sweep sphere center against each prism face
//   6. Return earliest hit with packed featureId
inline bool SweepCapsuleTri_PhysXLike_TOI01(
    const SweepCapsuleInput& in,
    const Triangle& srcTri,
    const SweepConfig& cfg,
    float& outT,
    Vec3& outN,
    uint32_t& outFeat)
{
    const float r = in.radius + cfg.skin;

    if (LenSq(in.delta) <= kEpsSq) return false;

    // Midpoint and half-segment
    Vec3 c0 = (in.segA0 + in.segB0) * 0.5f;
    Vec3 a  = (in.segA0 - in.segB0) * 0.5f;

    // Initial overlap: capsule segment vs triangle
    Vec3 qSeg, qTri;
    if (DistSegmentTriangleSq(in.segA0, in.segB0, srcTri, &qSeg, &qTri) <= r*r) {
        outT = 0.0f;
        Vec3 fallback = NormalizeSafe(in.delta * -1.0f, {0,1,0});
        outN = NormalizeSafe(qSeg - qTri, fallback);
        outFeat = 0xFFFFFFFFu;
        if (Dot(outN, in.delta) > 0.0f) outN = outN * -1.0f;
        return true;
    }

    // Degenerate capsule -> sphere
    float a2 = LenSq(a);
    if (a2 <= kEpsSq) {
        return SweepSphereTri_TOI01(c0, r, in.delta, srcTri, cfg.twoSidedTris,
                                    outT, outN, outFeat);
    }

    // Colinear shortcut: capsule axis || motion direction
    Vec3 dirU = NormalizeSafe(in.delta, {0,1,0});
    float halfHeight = std::sqrt(a2);
    Vec3 axisU = a * (1.0f / halfHeight);
    if (Abs(Dot(axisU, dirU)) > 1.0f - kNpColinearEps) {
        Vec3 frontCenter = c0 + dirU * halfHeight;
        return SweepSphereTri_TOI01(frontCenter, r, in.delta, srcTri,
                                    cfg.twoSidedTris, outT, outN, outFeat);
    }

    // Extrude triangle and sweep sphere center vs prism faces
    Triangle faces[7];
    uint32_t faceCount = BuildExtrudedFaces7(srcTri, a, faces);

    float bestT = std::numeric_limits<float>::infinity();
    Vec3  bestN = {0,1,0};
    uint32_t bestF = 0;

    for (uint32_t i = 0; i < faceCount; ++i) {
        float t; Vec3 n; uint32_t f;
        // Artificial faces: always two-sided
        if (!SweepSphereTri_TOI01(c0, r, in.delta, faces[i], true, t, n, f))
            continue;

        uint32_t feat = (i << 8) | (f & 0xFF);

        if (!std::isfinite(bestT) || (t < bestT) ||
            (Abs(t - bestT) < cfg.tieEpsT && feat < bestF))
        {
            bestT = t;
            bestN = n;
            bestF = feat;
        }
    }

    if (!std::isfinite(bestT)) return false;
    outT = bestT;
    outN = bestN;
    outFeat = bestF;
    return true;
}

// =========================================================================
// Box support: triangulate AABB/OBB surfaces, then capsule-sweep
// =========================================================================

// 12 triangles over 8 corner vertices (stable index ordering)
static inline const uint8_t* BoxTriIndices36()
{
    static const uint8_t Indices[] = {
        0,2,1,  0,3,2,   // -Z face
        1,6,5,  1,2,6,   // +X face
        5,7,4,  5,6,7,   // +Z face
        4,3,0,  4,7,3,   // -X face
        3,6,2,  3,7,6,   // +Y face
        5,0,1,  5,4,0    // -Y face
    };
    return Indices;
}

// AABB -> 8 corner points (stable ordering:
// 0:(-,-,-) 1:(+,-,-) 2:(+,+,-) 3:(-,+,-)
// 4:(-,-,+) 5:(+,-,+) 6:(+,+,+) 7:(-,+,+))
static inline void BuildAabbPoints8(const AABB& b, Vec3 outP[8])
{
    const float x0 = b.minX, y0 = b.minY, z0 = b.minZ;
    const float x1 = b.maxX, y1 = b.maxY, z1 = b.maxZ;
    outP[0] = {x0,y0,z0}; outP[1] = {x1,y0,z0};
    outP[2] = {x1,y1,z0}; outP[3] = {x0,y1,z0};
    outP[4] = {x0,y0,z1}; outP[5] = {x1,y0,z1};
    outP[6] = {x1,y1,z1}; outP[7] = {x0,y1,z1};
}

// OBB -> 8 corner points (same index pattern as AABB)
static inline void BuildObbPoints8(const OBB& o, Vec3 outP[8])
{
    Vec3 ex = o.axisX * o.half.x;
    Vec3 ey = o.axisY * o.half.y;
    Vec3 ez = o.axisZ * o.half.z;

    outP[0] = o.center - ex - ey - ez;
    outP[1] = o.center + ex - ey - ez;
    outP[2] = o.center + ex + ey - ez;
    outP[3] = o.center - ex + ey - ez;
    outP[4] = o.center - ex - ey + ez;
    outP[5] = o.center + ex - ey + ez;
    outP[6] = o.center + ex + ey + ez;
    outP[7] = o.center - ex + ey + ez;
}

// 8 points -> 12 surface triangles
static inline void BuildBoxSurfaceTris12(const Vec3 p[8], Triangle outTris[12])
{
    const uint8_t* idx = BoxTriIndices36();
    for (uint32_t i = 0; i < 12; ++i) {
        uint8_t a = idx[i*3+0], b = idx[i*3+1], c = idx[i*3+2];
        outTris[i] = { p[a], p[b], p[c] };
    }
}

// Capsule vs box surface (12 triangles) using extrusion+sweep.
// FeatureId packing for boxes: (triId<<16) | (prismFace<<8) | (sphereTriFeat)
static inline bool SweepCapsuleBox_TrisExtruded_TOI01(
    const SweepCapsuleInput& in,
    const Triangle boxSurfaceTris[12],
    const SweepConfig& cfg,
    float& outT, Vec3& outN, uint32_t& outFeat)
{
    const float r = in.radius + cfg.skin;
    if (LenSq(in.delta) <= kEpsSq) return false;

    Vec3 c0 = (in.segA0 + in.segB0) * 0.5f;
    Vec3 a  = (in.segA0 - in.segB0) * 0.5f;

    // Initial overlap: capsule segment vs box surface (12 tris)
    {
        float bestD2 = std::numeric_limits<float>::infinity();
        Vec3 bestSeg{}, bestTri{};
        for (uint32_t i = 0; i < 12; ++i) {
            Vec3 qSeg, qTri;
            float d2 = DistSegmentTriangleSq(in.segA0, in.segB0,
                                              boxSurfaceTris[i], &qSeg, &qTri);
            if (d2 < bestD2) { bestD2 = d2; bestSeg = qSeg; bestTri = qTri; }
        }
        if (bestD2 <= r*r) {
            outT = 0.0f;
            Vec3 fallback = NormalizeSafe(in.delta * -1.0f, {0,1,0});
            outN = NormalizeSafe(bestSeg - bestTri, fallback);
            if (Dot(outN, in.delta) > 0.0f) outN = outN * -1.0f;
            outFeat = 0xFFFFFFFFu;
            return true;
        }
    }

    // Degenerate capsule -> sphere vs box
    const float a2 = LenSq(a);
    if (a2 <= kEpsSq) {
        float bestT = std::numeric_limits<float>::infinity();
        float bestAlign = -std::numeric_limits<float>::infinity();
        uint32_t bestF = 0;
        Vec3 bestN{0,1,0};
        Vec3 dirU = NormalizeSafe(in.delta, {0,1,0});

        auto consider = [&](float t, const Vec3& n, uint32_t feat) {
            const float epsA = kNpEpsAlign;
            Vec3 nn = NormalizeSafe(n, {0,1,0});
            float align = -Dot(nn, dirU);
            if (t < bestT - cfg.tieEpsT) {
                bestT=t; bestN=nn; bestF=feat; bestAlign=align; return;
            }
            if (Abs(t-bestT) <= cfg.tieEpsT) {
                if (align > bestAlign + epsA) {
                    bestT=t; bestN=nn; bestF=feat; bestAlign=align; return;
                }
                if (Abs(align-bestAlign) <= epsA && feat < bestF) {
                    bestT=t; bestN=nn; bestF=feat; bestAlign=align;
                }
            }
        };

        for (uint32_t i = 0; i < 12; ++i) {
            float t; Vec3 n; uint32_t f;
            if (!SweepSphereTri_TOI01(c0, r, in.delta, boxSurfaceTris[i],
                                       true, t, n, f))
                continue;
            consider(t, n, (i << 8) | (f & 0xFF));
        }

        if (!std::isfinite(bestT)) return false;
        outT = bestT; outN = bestN; outFeat = bestF;
        if (Dot(outN, in.delta) > 0.0f) outN = outN * -1.0f;
        return true;
    }

    // Main path: extrude each face tri by capsule half-segment and sphere-sweep
    float bestT = std::numeric_limits<float>::infinity();
    float bestAlign = -std::numeric_limits<float>::infinity();
    Vec3  bestN{0,1,0};
    uint32_t bestF = 0;

    Vec3 dirU = NormalizeSafe(in.delta, {0,1,0});

    auto consider = [&](float t, const Vec3& n, uint32_t feat) {
        const float epsA = kNpEpsAlign;
        Vec3 nn = NormalizeSafe(n, {0,1,0});
        float align = -Dot(nn, dirU);
        if (t < bestT - cfg.tieEpsT) {
            bestT=t; bestN=nn; bestF=feat; bestAlign=align; return;
        }
        if (Abs(t-bestT) <= cfg.tieEpsT) {
            if (align > bestAlign + epsA) {
                bestT=t; bestN=nn; bestF=feat; bestAlign=align; return;
            }
            if (Abs(align-bestAlign) <= epsA && feat < bestF) {
                bestT=t; bestN=nn; bestF=feat; bestAlign=align;
            }
        }
    };

    for (uint32_t triId = 0; triId < 12; ++triId) {
        Triangle faces[7];
        uint32_t faceCount = BuildExtrudedFaces7(boxSurfaceTris[triId], a, faces);

        for (uint32_t prismFace = 0; prismFace < faceCount; ++prismFace) {
            float t; Vec3 n; uint32_t f;
            if (!SweepSphereTri_TOI01(c0, r, in.delta, faces[prismFace],
                                       true, t, n, f))
                continue;
            uint32_t feat = (triId << 16) | (prismFace << 8) | (f & 0xFF);
            consider(t, n, feat);
        }
    }

    if (!std::isfinite(bestT)) return false;
    outT = bestT;
    outN = bestN;
    outFeat = bestF;
    if (Dot(outN, in.delta) > 0.0f) outN = outN * -1.0f;
    return true;
}

// ---- Convenience wrappers: AABB and OBB capsule sweep -------------------

inline bool SweepCapsuleAabb_PhysXLike_TOI01(
    const SweepCapsuleInput& in, const AABB& box, const SweepConfig& cfg,
    float& outT, Vec3& outN, uint32_t& outFeat)
{
    Vec3 p[8]; Triangle tris[12];
    BuildAabbPoints8(box, p);
    BuildBoxSurfaceTris12(p, tris);
    return SweepCapsuleBox_TrisExtruded_TOI01(in, tris, cfg, outT, outN, outFeat);
}

inline bool SweepCapsuleObb_PhysXLike_TOI01(
    const SweepCapsuleInput& in, const OBB& box, const SweepConfig& cfg,
    float& outT, Vec3& outN, uint32_t& outFeat)
{
    Vec3 p[8]; Triangle tris[12];
    BuildObbPoints8(box, p);
    BuildBoxSurfaceTris12(p, tris);
    return SweepCapsuleBox_TrisExtruded_TOI01(in, tris, cfg, outT, outN, outFeat);
}

// =========================================================================
// Overlap narrowphase: capsule vs AABB / OBB / Triangle
// =========================================================================
// Returns true if overlapping, fills outContact with push-out normal + depth.
// Normal points AWAY from the primitive (direction to push capsule out).
// Depth is positive when overlapping.

// ---- Capsule vs AABB overlap (Minkowski-style: segment-to-AABB distance) --
// Expand AABB by capsule radius, test capsule segment vs expanded AABB.
// MTD direction = normalized(closestOnSegment - closestOnAABB).
// featureId = face index 0..5 for min-penetration axis.
inline bool OverlapCapsuleAabb(const Vec3& segA, const Vec3& segB, float radius,
                                const AABB& box, OverlapContact& out)
{
    // Find closest point on capsule segment to box
    // Strategy: for each point on the segment, find closest on AABB,
    // take the pair with minimum distance.
    // Simplified: sample segment endpoints + closest point to AABB center.
    Vec3 boxCenter = AABBCenter(box);

    // Project box center onto capsule segment to find the nearest segment point
    Vec3 segDir = segB - segA;
    float segLen2 = LenSq(segDir);
    float t = 0.0f;
    if (segLen2 > kEpsSq) {
        t = Dot(boxCenter - segA, segDir) / segLen2;
        t = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
    Vec3 segPoint = segA + segDir * t;

    // Closest point on AABB to this segment point
    Vec3 cp = ClosestPointPointAABB(segPoint, box);
    Vec3 diff = segPoint - cp;
    float dist2 = LenSq(diff);

    if (dist2 > radius * radius) return false;  // no overlap

    float dist = std::sqrt(dist2);
    out.depth = radius - dist;

    if (dist > kEpsSq) {
        // Normal case: push along segment-point to closest-on-box direction
        out.normal = diff * (1.0f / dist);
    } else {
        // Segment point is inside the AABB: use min-penetration axis
        // Compute penetration depth along each of 6 faces
        float pen[6] = {
            segPoint.x - box.minX,   // -X face (id=0)
            box.maxX - segPoint.x,   // +X face (id=1)
            segPoint.y - box.minY,   // -Y face (id=2)
            box.maxY - segPoint.y,   // +Y face (id=3)
            segPoint.z - box.minZ,   // -Z face (id=4)
            box.maxZ - segPoint.z    // +Z face (id=5)
        };
        const Vec3 normals[6] = {
            {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1}
        };

        float minPen = pen[0];
        uint32_t minFace = 0;
        for (uint32_t i = 1; i < 6; ++i) {
            if (pen[i] < minPen) { minPen = pen[i]; minFace = i; }
        }
        out.normal = normals[minFace];
        out.depth = minPen + radius;
        out.featureId = minFace;
        return true;
    }

    // Determine face ID for the contact point
    // Use the axis most aligned with the normal
    float ax = Abs(out.normal.x), ay = Abs(out.normal.y), az = Abs(out.normal.z);
    if (ax >= ay && ax >= az)
        out.featureId = (out.normal.x > 0.0f) ? 1 : 0;
    else if (ay >= az)
        out.featureId = (out.normal.y > 0.0f) ? 3 : 2;
    else
        out.featureId = (out.normal.z > 0.0f) ? 5 : 4;

    return true;
}

// ---- Capsule vs OBB overlap (transform to local AABB, reuse kernel) -------
inline bool OverlapCapsuleObb(const Vec3& segA, const Vec3& segB, float radius,
                               const OBB& obb, OverlapContact& out)
{
    // Transform capsule endpoints into OBB local space (axis-aligned)
    Vec3 dA = segA - obb.center;
    Vec3 dB = segB - obb.center;
    Vec3 localA = { Dot(dA, obb.axisX), Dot(dA, obb.axisY), Dot(dA, obb.axisZ) };
    Vec3 localB = { Dot(dB, obb.axisX), Dot(dB, obb.axisY), Dot(dB, obb.axisZ) };

    // Create local AABB centered at origin
    AABB localBox = { -obb.half.x, -obb.half.y, -obb.half.z,
                       obb.half.x,  obb.half.y,  obb.half.z };

    OverlapContact localContact;
    if (!OverlapCapsuleAabb(localA, localB, radius, localBox, localContact))
        return false;

    // Transform normal back to world space
    out.normal = obb.axisX * localContact.normal.x
               + obb.axisY * localContact.normal.y
               + obb.axisZ * localContact.normal.z;
    out.normal = NormalizeSafe(out.normal, {0, 1, 0});
    out.depth = localContact.depth;
    out.featureId = localContact.featureId;
    return true;
}

// ---- Capsule vs Triangle overlap (segment-triangle distance) --------------
inline bool OverlapCapsuleTri(const Vec3& segA, const Vec3& segB, float radius,
                               const Triangle& tri, OverlapContact& out)
{
    Vec3 qSeg, qTri;
    uint32_t feat;
    float dist2 = DistSegmentTriangleSq(segA, segB, tri, &qSeg, &qTri, &feat);

    if (dist2 > radius * radius) return false;

    float dist = std::sqrt(dist2);
    out.depth = radius - dist;

    if (dist > kEpsSq) {
        out.normal = (qSeg - qTri) * (1.0f / dist);
    } else {
        // Inside triangle: push along triangle normal
        Vec3 n = TriNormalUnit(tri);
        Vec3 mid = (segA + segB) * 0.5f;
        float side = Dot(mid - tri.p0, n);
        out.normal = (side >= 0.0f) ? n : (n * -1.0f);
        out.depth = radius;
    }

    out.featureId = feat;
    return true;
}

}}} // namespace Engine::Collision::sq
