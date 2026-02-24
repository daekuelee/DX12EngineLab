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
#include <limits>

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
// Cyrus-Beck clipping produces a face candidate for segments parallel to (or hovering
// over) the triangle face, ensuring featureId==0 when the closest point is on the face.
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

    // epsD2 tie-break: when two candidates are within this tolerance,
    // prefer the one with the lower featureId (face < edge < vertex).
    constexpr float epsD2 = 1e-8f;

    // -- General face candidate via Cyrus-Beck clipping --
    // Projects segment onto the triangle plane and clips against the 3
    // half-space edges. If any portion of the segment projects inside the
    // triangle, the closest point on that portion to the plane wins.
    float d2Face = (std::numeric_limits<float>::max)();
    Vec3  faceSeg{}, faceTri{};
    if (n2 > kEpsSq) {
        Vec3  nU  = n * (1.0f / std::sqrt(n2));
        Vec3  d   = s1 - s0;
        float dn  = Dot(nU, d);
        float h0  = Dot(nU, s0 - tri.p0);

        // Triangle edges and their inward half-space normals
        const Vec3 edges[3] = { tri.p1 - tri.p0, tri.p2 - tri.p1, tri.p0 - tri.p2 };
        const Vec3 oppo[3]  = { tri.p2, tri.p0, tri.p1 };          // opposite vertex
        const Vec3 orig[3]  = { tri.p0, tri.p1, tri.p2 };          // edge origin

        float tEnter = 0.0f, tExit = 1.0f;
        bool  valid  = true;
        for (int i = 0; i < 3; ++i) {
            Vec3  mi  = Cross(nU, edges[i]);                         // outward or inward
            float opp = Dot(mi, oppo[i] - orig[i]);                  // sign of opposite vtx
            if (opp < 0.0f) mi = mi * (-1.0f);                       // orient inward
            float dmi = Dot(mi, d);
            float hmi = Dot(mi, s0 - orig[i]);
            if (Abs(dmi) > kEpsSq) {
                float tc = -hmi / dmi;
                if (dmi < 0.0f) { if (tc < tExit) tExit = tc; }      // exiting half-space
                else            { if (tc > tEnter) tEnter = tc; }     // entering half-space
            } else {
                if (hmi < -kEpsPointInTri) { valid = false; break; }  // entirely outside
            }
        }
        if (valid && tEnter <= tExit + kEpsPointInTri) {
            if (tEnter > tExit) tEnter = tExit;                      // clamp numerical noise
            float tStar;
            if (Abs(dn) >= 1e-10f) {
                tStar = -h0 / dn;
                tStar = (tStar < tEnter) ? tEnter : (tStar > tExit ? tExit : tStar);
            } else {
                tStar = tEnter;
            }
            float hStar = h0 + tStar * dn;
            d2Face = hStar * hStar;
            faceSeg = s0 + d * tStar;
            faceTri = faceSeg + nU * (-hStar);                        // project onto plane
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
    if (d < best - epsD2 || (d < best + epsD2 && feat1 < bestFeat))
    { best = d; bestSeg = s1; bestTri = q1; bestFeat = feat1; }

    // Segment -> triangle edges
    Vec3 cSeg, cEdge;
    d = DistSegmentSegmentSq(s0, s1, tri.p0, tri.p1, &cSeg, &cEdge);
    if (d < best - epsD2 || (d < best + epsD2 && 1u < bestFeat))
    { best = d; bestSeg = cSeg; bestTri = cEdge; bestFeat = 1; }

    d = DistSegmentSegmentSq(s0, s1, tri.p1, tri.p2, &cSeg, &cEdge);
    if (d < best - epsD2 || (d < best + epsD2 && 2u < bestFeat))
    { best = d; bestSeg = cSeg; bestTri = cEdge; bestFeat = 2; }

    d = DistSegmentSegmentSq(s0, s1, tri.p2, tri.p0, &cSeg, &cEdge);
    if (d < best - epsD2 || (d < best + epsD2 && 3u < bestFeat))
    { best = d; bestSeg = cSeg; bestTri = cEdge; bestFeat = 3; }

    // Face candidate competes (featureId 0 — lowest, always wins ties)
    if (d2Face < best - epsD2 || (d2Face < best + epsD2 && 0u < bestFeat))
    { best = d2Face; bestSeg = faceSeg; bestTri = faceTri; bestFeat = 0; }

    if (outSegQ) *outSegQ = bestSeg;
    if (outTriQ) *outTriQ = bestTri;
    if (outTriFeatureId) *outTriFeatureId = bestFeat;
    return best;
}


// ---- Segment to AABB distance squared (exact piecewise-quadratic) -------
// Returns squared distance between closest point pair on segment [segA,segB]
// and AABB box. Optionally writes closest points.
//
// CONTRACT: exact global minimum, O(1) bounded, no heap, deterministic.
// ALGORITHM: f(t) = |S(t) - Clamp(S(t), box)|^2 is piecewise-quadratic.
//   Breakpoints at slab crossings divide [0,1] into ≤7 intervals. Each
//   interval has analytical quadratic minimum. Global min across all.
// EPSILON POLICY:
//   kEpsParallel (1e-12) — skip near-zero direction components
//   kTMerge (1e-6, matches SweepConfig::tieEpsT) — breakpoint de-duplication
//   kEpsSq (1e-20) — "is quadratic coefficient zero?"
//   epsD2 (1e-8, matches DistSegmentTriangleSq) — distance tie-break
inline float DistSegmentAABBSq(const Vec3& segA, const Vec3& segB,
                                const AABB& box,
                                Vec3* outSegQ = nullptr,
                                Vec3* outBoxQ = nullptr)
{
    const Vec3 D = segB - segA;
    const float a3[3] = {segA.x, segA.y, segA.z};
    const float d3[3] = {D.x, D.y, D.z};
    const float lo3[3] = {box.minX, box.minY, box.minZ};
    const float hi3[3] = {box.maxX, box.maxY, box.maxZ};

    // Breakpoint merge tolerance in t-space, matching SweepConfig::tieEpsT (1e-6f).
    // Prevents zero-width intervals that would make midpoint regime selection ambiguous.
    constexpr float kTMerge = 1e-6f;

    // Distance tie-break tolerance (squared world-space), same as DistSegmentTriangleSq.
    constexpr float epsD2 = 1e-8f;

    // ---- Collect breakpoints: t where S_i(t) crosses slab boundary ------
    float bp[8];
    int n = 0;
    bp[n++] = 0.0f;
    bp[n++] = 1.0f;

    for (int i = 0; i < 3; ++i) {
        if (Abs(d3[i]) <= kEpsParallel) continue;
        const float inv = 1.0f / d3[i];
        const float tLo = (lo3[i] - a3[i]) * inv;
        const float tHi = (hi3[i] - a3[i]) * inv;
        if (tLo > 0.0f && tLo < 1.0f) bp[n++] = tLo;
        if (tHi > 0.0f && tHi < 1.0f) bp[n++] = tHi;
    }

    // ---- Sort (insertion sort, ≤8 elements, bounded O(1)) ---------------
    for (int i = 1; i < n; ++i) {
        float key = bp[i];
        int j = i - 1;
        while (j >= 0 && bp[j] > key) { bp[j+1] = bp[j]; --j; }
        bp[j+1] = key;
    }

    // ---- Remove near-duplicates (in-place, stable, preserves order) -----
    {
        int dst = 1;
        for (int src = 1; src < n; ++src)
            if (bp[src] - bp[dst-1] > kTMerge) bp[dst++] = bp[src];
        n = dst;
    }

    // ---- Evaluate each interval's quadratic minimum ---------------------
    float bestD2 = (std::numeric_limits<float>::max)();
    float bestT  = 0.0f;

    for (int seg = 0; seg + 1 < n; ++seg) {
        const float tL = bp[seg], tR = bp[seg+1];
        const float tMid = (tL + tR) * 0.5f;

        // Build f(t) = qa*t^2 + qb*t + qc for this interval.
        // Per axis: if below slab, e_i(t) = (a_i + t*d_i) - lo_i
        //           if above slab, e_i(t) = (a_i + t*d_i) - hi_i
        //           if inside slab, contribution = 0
        // f(t) = sum_active(d_i^2 * t^2 + 2*d_i*(a_i - target_i)*t + (a_i - target_i)^2)
        float qa = 0.0f, qb = 0.0f, qc = 0.0f;
        for (int i = 0; i < 3; ++i) {
            const float valMid = a3[i] + tMid * d3[i];
            float target;
            if      (valMid < lo3[i]) target = lo3[i];
            else if (valMid > hi3[i]) target = hi3[i];
            else continue;  // inside slab: zero contribution
            const float off = a3[i] - target;
            qa += d3[i] * d3[i];
            qb += 2.0f * d3[i] * off;
            qc += off * off;
        }

        // Minimize qa*t^2 + qb*t + qc on [tL, tR]
        float tStar;
        if (qa > kEpsSq) {
            tStar = -qb / (2.0f * qa);
            tStar = (tStar < tL) ? tL : (tStar > tR ? tR : tStar);
        } else {
            // Linear or constant: min at endpoint with smaller value
            float fL = qb * tL + qc;  // qa≈0, omit qa*tL^2
            float fR = qb * tR + qc;
            tStar = (fL <= fR) ? tL : tR;
        }

        float d2 = qa * tStar * tStar + qb * tStar + qc;

        // Deterministic tie-break: prefer smaller t (same pattern as DistSegmentTriangleSq)
        if (d2 < bestD2 - epsD2 || (d2 < bestD2 + epsD2 && tStar < bestT)) {
            bestD2 = d2;
            bestT = tStar;
        }
    }

    // ---- Output closest points ------------------------------------------
    Vec3 qSeg = segA + D * bestT;
    if (outSegQ) *outSegQ = qSeg;
    if (outBoxQ) {
        // Inline clamp (avoids dependency on SqPrimitiveTests.h, keeps SqDistance.h standalone)
        auto cl = [](float x, float lo, float hi) -> float {
            return x < lo ? lo : (x > hi ? hi : x);
        };
        *outBoxQ = { cl(qSeg.x, box.minX, box.maxX),
                     cl(qSeg.y, box.minY, box.maxY),
                     cl(qSeg.z, box.minZ, box.maxZ) };
    }
    return bestD2 > 0.0f ? bestD2 : 0.0f;
}

}}} // namespace Engine::Collision::sq
