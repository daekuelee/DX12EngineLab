#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqDistance.h
//
// TERMINOLOGY:
//   DistXxxYyySq - squared distance between primitive X and primitive Y
//   outQ         - optional output: closest point on the respective primitive
//
// POLICY:
//   - All functions are pure: inputs only, no side effects.
//   - Squared distances avoid sqrt where possible.
//   - Segment parameter s/t in [0,1].
//
// CONTRACT:
//   - Standalone: includes only SqTypes.h. No other SQ headers.
//   - Ericson-style closest-point algorithms (deterministic, no branching ambiguity).
//   - Optional outFeatureId: 0=face, 1-3=edges (p0p1,p1p2,p2p0), 4-6=vertices (p0,p1,p2).
//
// PROOF POINTS:
//   - [PR3.2] DistPointTriangleSq(vertex, tri) == 0 for each triangle vertex
//   - [PR3.2] DistSegmentTriangleSq returns 0 when segment pierces triangle
//
// REFERENCES:
//   - Ericson, "Real-Time Collision Detection" (2005), Chapter 5
//   - ex.cpp lines 312-470 (golden SSOT)
// =========================================================================

#include "SqTypes.h"

namespace Engine { namespace Collision { namespace sq {

// ---- Point to triangle distance squared (Ericson style) -----------------
// Returns squared distance; optionally writes closest point on triangle.
inline float DistPointTriangleSq(const Vec3& p, const Triangle& t,
                                 Vec3* outQ = nullptr,
                                 uint32_t* outFeatureId = nullptr)
{
    Vec3 a = t.p0, b = t.p1, c = t.p2;
    Vec3 ab = b - a;
    Vec3 ac = c - a;
    Vec3 ap = p - a;

    float d1 = Dot(ab, ap);
    float d2 = Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) { if (outQ) *outQ = a; if (outFeatureId) *outFeatureId = 4; return LenSq(ap); }

    Vec3 bp = p - b;
    float d3 = Dot(ab, bp);
    float d4 = Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) { if (outQ) *outQ = b; if (outFeatureId) *outFeatureId = 5; return LenSq(bp); }

    float vc = d1*d4 - d3*d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        Vec3 q = a + ab*v;
        if (outQ) *outQ = q;
        if (outFeatureId) *outFeatureId = 1;
        return LenSq(p - q);
    }

    Vec3 cp = p - c;
    float d5 = Dot(ab, cp);
    float d6 = Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) { if (outQ) *outQ = c; if (outFeatureId) *outFeatureId = 6; return LenSq(cp); }

    float vb = d5*d2 - d1*d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        Vec3 q = a + ac*w;
        if (outQ) *outQ = q;
        if (outFeatureId) *outFeatureId = 3;
        return LenSq(p - q);
    }

    float va = d3*d6 - d5*d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        Vec3 q = b + (c - b)*w;
        if (outQ) *outQ = q;
        if (outFeatureId) *outFeatureId = 2;
        return LenSq(p - q);
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    Vec3 q = a + ab*v + ac*w;
    if (outQ) *outQ = q;
    if (outFeatureId) *outFeatureId = 0;
    return LenSq(p - q);
}

// ---- Segment to segment distance squared (Ericson style) ----------------
inline float DistSegmentSegmentSq(const Vec3& p1, const Vec3& q1,
                                  const Vec3& p2, const Vec3& q2,
                                  Vec3* c1 = nullptr, Vec3* c2 = nullptr)
{
    Vec3 d1 = q1 - p1;
    Vec3 d2 = q2 - p2;
    Vec3 r  = p1 - p2;
    float a = Dot(d1, d1);
    float e = Dot(d2, d2);
    float f = Dot(d2, r);

    float s = 0.0f, t = 0.0f;

    if (a <= kEpsSq && e <= kEpsSq) {
        if (c1) *c1 = p1;
        if (c2) *c2 = p2;
        return LenSq(p1 - p2);
    }
    if (a <= kEpsSq) {
        s = 0.0f;
        t = f / e;
        t = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    } else {
        float c = Dot(d1, r);
        if (e <= kEpsSq) {
            t = 0.0f;
            s = (-c) / a;
            s = (s < 0.0f) ? 0.0f : (s > 1.0f ? 1.0f : s);
        } else {
            float b = Dot(d1, d2);
            float denom = a*e - b*b;
            if (denom != 0.0f) s = (b*f - c*e) / denom;
            else s = 0.0f;
            s = (s < 0.0f) ? 0.0f : (s > 1.0f ? 1.0f : s);

            float tnom = b*s + f;
            if (tnom < 0.0f) {
                t = 0.0f;
                s = (-c) / a;
                s = (s < 0.0f) ? 0.0f : (s > 1.0f ? 1.0f : s);
            } else if (tnom > e) {
                t = 1.0f;
                s = (b - c) / a;
                s = (s < 0.0f) ? 0.0f : (s > 1.0f ? 1.0f : s);
            } else {
                t = tnom / e;
            }
        }
    }

    Vec3 cp1 = p1 + d1*s;
    Vec3 cp2 = p2 + d2*t;
    if (c1) *c1 = cp1;
    if (c2) *c2 = cp2;
    return LenSq(cp1 - cp2);
}

// ---- Segment to triangle distance squared (for capsule initial overlap) -
// Checks segment-plane intersection first, then falls back to edge/endpoint tests.
inline float DistSegmentTriangleSq(const Vec3& s0, const Vec3& s1,
                                   const Triangle& tri,
                                   Vec3* outSegQ = nullptr,
                                   Vec3* outTriQ = nullptr,
                                   uint32_t* outTriFeatureId = nullptr)
{
    Vec3 n = Cross(tri.p1 - tri.p0, tri.p2 - tri.p0);
    float n2 = LenSq(n);

    // Segment-plane intersection inside triangle => distance 0
    if (n2 > kEpsSq) {
        float d0 = Dot(n, s0 - tri.p0);
        float d1 = Dot(n, s1 - tri.p0);
        if ((d0 <= 0.0f && d1 >= 0.0f) || (d0 >= 0.0f && d1 <= 0.0f)) {
            float denom = (d0 - d1);
            if (Abs(denom) > kEpsSq) {
                float t = d0 / (d0 - d1);
                if (t >= 0.0f && t <= 1.0f) {
                    Vec3 p = s0 + (s1 - s0)*t;
                    if (PointInTri(p, tri, n)) {
                        if (outSegQ) *outSegQ = p;
                        if (outTriQ) *outTriQ = p;
                        if (outTriFeatureId) *outTriFeatureId = 0;
                        return 0.0f;
                    }
                }
            }
        }
    }

    // Endpoints -> triangle
    uint32_t bestFeat = 0;
    Vec3 q0, q1;
    uint32_t feat0, feat1;
    float best = DistPointTriangleSq(s0, tri, &q0, &feat0);
    Vec3 bestSeg = s0, bestTri = q0;
    bestFeat = feat0;

    float d = DistPointTriangleSq(s1, tri, &q1, &feat1);
    if (d < best) { best = d; bestSeg = s1; bestTri = q1; bestFeat = feat1; }

    // Segment -> triangle edges
    Vec3 cSeg, cEdge;
    d = DistSegmentSegmentSq(s0, s1, tri.p0, tri.p1, &cSeg, &cEdge);
    if (d < best) { best = d; bestSeg = cSeg; bestTri = cEdge; bestFeat = 1; }

    d = DistSegmentSegmentSq(s0, s1, tri.p1, tri.p2, &cSeg, &cEdge);
    if (d < best) { best = d; bestSeg = cSeg; bestTri = cEdge; bestFeat = 2; }

    d = DistSegmentSegmentSq(s0, s1, tri.p2, tri.p0, &cSeg, &cEdge);
    if (d < best) { best = d; bestSeg = cSeg; bestTri = cEdge; bestFeat = 3; }

    if (outSegQ) *outSegQ = bestSeg;
    if (outTriQ) *outTriQ = bestTri;
    if (outTriFeatureId) *outTriFeatureId = bestFeat;
    return best;
}


}}} // namespace Engine::Collision::sq
