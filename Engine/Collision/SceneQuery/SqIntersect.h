#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqIntersect.h
//
// TERMINOLOGY:
//   Segment  - finite line from p0 to p1, parameter t in [0,1]
//   Sphere   - center + radius
//   Cylinder - finite cylinder around segment [c0,c1] with radius r
//              (curved surface only; endcap hits come from vertex spheres)
//
// POLICY:
//   - All functions return earliest intersection t in [0,1].
//   - No side effects. Pure geometry tests.
//
// CONTRACT:
//   - Standalone: includes only SqTypes.h.
//   - IntersectSegmentCylinder01 tests curved surface only. Endcap spheres
//     are handled separately by vertex tests in the narrowphase.
//
// PROOF POINTS:
//   - [PR3.2] Segment through sphere center: t == 0 (starts inside)
//   - [PR3.2] Segment tangent to sphere: valid hit at tangent point
//
// REFERENCES:
//   - ex.cpp lines 476-573 (golden SSOT)
//   - Ericson, RTCD Chapter 5 (segment-sphere, segment-cylinder)
// =========================================================================

#include "SqTypes.h"
#include <limits>

namespace Engine { namespace Collision { namespace sq {

// ---- Closest point on segment -------------------------------------------
inline Vec3 ClosestPointOnSegment(const Vec3& A, const Vec3& B, const Vec3& P)
{
    Vec3 AB = B - A;
    float ab2 = Dot(AB, AB);
    if (ab2 < kEpsSq) return A;
    float t = Dot(P - A, AB) / ab2;
    t = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    return A + AB*t;
}

// ---- Moving point segment vs sphere (earliest t in [0,1]) ---------------
// Segment [p0, p1] sweeps a point. Returns earliest t when point touches sphere.
// If point starts inside sphere, returns t=0.
inline bool IntersectSegmentSphere01(const Vec3& p0, const Vec3& p1,
                                     const Vec3& c, float r, float& outT)
{
    Vec3 d = p1 - p0;
    Vec3 m = p0 - c;

    float a = Dot(d, d);
    float b = Dot(m, d);
    float c0 = Dot(m, m) - r*r;

    if (c0 <= 0.0f) { outT = 0.0f; return true; }  // starts inside
    if (a < kEpsSq) return false;  // degenerate segment

    float disc = b*b - a*c0;
    if (disc < 0.0f) return false;

    float s = std::sqrt(disc);
    float t = (-b - s) / a;
    if (t < 0.0f || t > 1.0f) return false;

    outT = t;
    return true;
}

// ---- Moving point segment vs finite cylinder (curved surface only) ------
// Cylinder wraps segment [c0,c1] with radius r.
// Returns earliest t in [0,1] where point hits the curved surface.
// Endcap hits are NOT tested here (use vertex sphere tests instead).
//
// Algorithm: project motion onto plane perpendicular to cylinder axis,
// solve 2D circle intersection, then validate axial range.
inline bool IntersectSegmentCylinder01(const Vec3& p0, const Vec3& p1,
                                       const Vec3& c0, const Vec3& c1,
                                       float r, float& outT)
{
    Vec3 A = c1 - c0;
    float L2 = Dot(A, A);
    if (L2 < kEpsSq) return false;  // degenerate edge -> vertex spheres handle it

    float L = std::sqrt(L2);
    Vec3 U = A * (1.0f / L);

    Vec3 V = p1 - p0;
    Vec3 m = p0 - c0;

    Vec3 Vperp = V - U * Dot(V, U);
    Vec3 mperp = m - U * Dot(m, U);

    float qa = Dot(Vperp, Vperp);
    float qb = 2.0f * Dot(mperp, Vperp);
    float qc = Dot(mperp, mperp) - r*r;

    float bestT = std::numeric_limits<float>::infinity();
    bool hit = false;

    auto acceptRoot = [&](float tCand) {
        if (tCand < 0.0f || tCand > 1.0f) return;
        float s = Dot(m + V*tCand, U);
        if (s < 0.0f || s > L) return;
        if (tCand < bestT) { bestT = tCand; hit = true; }
    };

    if (qa > kEpsParallel) {
        float disc = qb*qb - 4.0f*qa*qc;
        if (disc >= 0.0f) {
            float sdisc = std::sqrt(disc);
            float t0 = (-qb - sdisc) / (2.0f*qa);
            float t1 = (-qb + sdisc) / (2.0f*qa);
            if (t0 > t1) std::swap(t0, t1);
            acceptRoot(t0);
            acceptRoot(t1);
        }
    } else {
        // Nearly parallel to cylinder axis
        float dist2 = Dot(mperp, mperp);
        if (dist2 <= r*r) {
            float s0 = Dot(m, U);
            float sV = Dot(V, U);
            if (Abs(sV) < kEpsParallel) {
                if (s0 >= 0.0f && s0 <= L) { outT = 0.0f; return true; }
            } else {
                float tE = (0.0f - s0) / sV;
                float tL = (L    - s0) / sV;
                if (tE > tL) std::swap(tE, tL);
                float tCand = (std::max)(tE, 0.0f);
                if (tCand <= tL && tCand <= 1.0f) { bestT = tCand; hit = true; }
            }
        }
    }

    if (!hit) return false;
    outT = bestT;
    return true;
}

}}} // namespace Engine::Collision::sq
