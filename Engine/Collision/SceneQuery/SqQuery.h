#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqQuery.h
//
// TERMINOLOGY:
//   QueryScratch - caller-owned stack memory for BVH traversal (6 KB)
//   NodeTask     - BVH traversal stack entry: node index + time window
//   BetterHit    - deterministic hit comparison (t -> type -> index -> feat)
//
// POLICY:
//   - QueryScratch is caller-allocated on stack. No heap allocation during query.
//   - BVH traversal is deterministic: right-child pushed first (left popped first = DFS).
//   - Hit selection uses BetterHit cascade for stable tie-breaking.
//   - Early-out: prune nodes whose tEnter >= current best t.
//
// CONTRACT:
//   - Standalone: includes SqNarrowphase.h, SqBVH.h, SqBroadphase.h.
//   - SweepCapsuleClosestHit_Fast is the ONLY public query entry point.
//   - StaticBVH must be immutable during query lifetime.
//   - QueryScratch.sp is reset to 0 at function entry.
//
// PROOF POINTS:
//   - [PR3.6] Empty BVH: returns Hit{hit=false, t=1.0}
//   - [PR3.6] Single AABB primitive: returns valid hit with correct normal
//   - [PR3.6] Deterministic: same inputs -> same Hit output
//
// REFERENCES:
//   - ex.cpp lines 822-948 (golden SSOT)
// =========================================================================

#include "SqNarrowphase.h"
#include "SqBVH.h"
#include "SqBroadphase.h"

namespace Engine { namespace Collision { namespace sq {

// ---- Traversal stack entry ----------------------------------------------

struct NodeTask {
    uint32_t node;
    float    tEnter;
    float    tExit;
};

// ---- Caller-owned scratch memory ----------------------------------------
// 512 entries * 12 bytes = 6144 bytes. Sufficient for balanced BVH.

struct QueryScratch {
    NodeTask stack[512];
    uint32_t sp = 0;
};

// ---- Deterministic hit comparison (SSOT tie-break cascade) ---------------
// Order: smallest t -> lowest PrimType -> lowest prim index -> lowest featureId.
// Within tieEpsT, all four levels participate.
inline bool BetterHit(float tNew, PrimType typeNew, uint32_t idxNew, uint32_t featNew,
                      float tBest, PrimType typeBest, uint32_t idxBest, uint32_t featBest,
                      float epsT)
{
    if (tNew < tBest) return true;
    if (Abs(tNew - tBest) < epsT) {
        if ((uint8_t)typeNew != (uint8_t)typeBest) return (uint8_t)typeNew < (uint8_t)typeBest;
        if (idxNew != idxBest) return idxNew < idxBest;
        return featNew < featBest;
    }
    return false;
}

// ---- Per-primitive narrowphase dispatch ----------------------------------

inline bool SweepCapsulePrim_TOI01(
    const StaticBVH& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    const PrimRef& pref,
    float& outT, Vec3& outN, uint32_t& outFeat)
{
    switch (pref.type) {
        case PrimType::Tri:
            return SweepCapsuleTri_PhysXLike_TOI01(
                in, bvh.tris[pref.index], cfg, outT, outN, outFeat);

        case PrimType::Aabb:
            return SweepCapsuleAabb_PhysXLike_TOI01(
                in, bvh.aabbs[pref.index], cfg, outT, outN, outFeat);

        case PrimType::Obb:
            return SweepCapsuleObb_PhysXLike_TOI01(
                in, bvh.obbs[pref.index], cfg, outT, outN, outFeat);

        default:
            return false;
    }
}

// =========================================================================
// Main query: capsule sweep closest hit via BVH DFS
// =========================================================================
//
// Consumes: immutable StaticBVH, SweepCapsuleInput, SweepConfig, QueryScratch
// Produces: Hit (earliest contact along displacement delta)
//
// Algorithm:
//   1. Compute capsule AABB at t=0 expanded by skin (matches narrowphase radius+skin)
//   2. Test root node time-window; push onto stack if valid
//   3. DFS loop: pop node, prune by tEnter >= best.t
//      - Leaf: test each primitive (time-window + narrowphase + BetterHit)
//      - Internal: push children (right first for deterministic left-first popping)
//   4. Return best Hit
inline Hit SweepCapsuleClosestHit_Fast(
    const StaticBVH& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    QueryScratch& scratch)
{
    Hit best{};
    best.hit = false;
    best.t = 1.0f;

    // Moving capsule AABB at t=0 expanded by skin (match narrowphase radius+skin)
    AABB cap0 = CapsuleAabbAtT(in, 0.0f, cfg.skin);

    scratch.sp = 0;

    float rE, rL;
    if (!AabbAabb_SweepInterval01(cap0, in.delta, bvh.nodes[bvh.root].bounds, rE, rL))
        return best;

    scratch.stack[scratch.sp++] = { bvh.root, rE, rL };

    while (scratch.sp) {
        NodeTask task = scratch.stack[--scratch.sp];

        if (task.tEnter >= best.t) continue;
        if (task.tExit  > best.t)  task.tExit = best.t;
        if (task.tEnter > task.tExit) continue;

        const BVHNode& node = bvh.nodes[task.node];

        // Leaf node: test primitives
        if (node.primCount) {
            for (uint32_t k = 0; k < node.primCount; ++k) {
                const PrimRef& pref = bvh.prims[bvh.primIdx[node.primStart + k]];

                float pE, pL;
                if (!AabbAabb_SweepInterval01(cap0, in.delta, pref.bounds, pE, pL))
                    continue;

                float lo = (std::max)(task.tEnter, pE);
                float hi = (std::min)(task.tExit,  pL);
                if (lo >= best.t || lo > hi) continue;

                float t; Vec3 n; uint32_t f;
                if (!SweepCapsulePrim_TOI01(bvh, in, cfg, pref, t, n, f))
                    continue;

                if (t < lo || t > hi) continue;

                if (!best.hit || BetterHit(t, pref.type, pref.index, f,
                                            best.t, best.type, best.index,
                                            best.featureId, cfg.tieEpsT))
                {
                    best.hit = true;
                    best.t = t;
                    best.type = pref.type;
                    best.index = pref.index;
                    best.normal = n;
                    best.featureId = f;
                }
            }
            continue;
        }

        // Internal node: push children
        // SSOT: right first, left last (left popped first = deterministic DFS order)
        auto pushChild = [&](uint32_t child) {
            float cE, cL;
            if (!AabbAabb_SweepInterval01(cap0, in.delta, bvh.nodes[child].bounds, cE, cL))
                return;
            float lo = (std::max)(task.tEnter, cE);
            float hi = (std::min)(task.tExit,  cL);
            if (hi > best.t) hi = best.t;
            if (lo > hi) return;
            scratch.stack[scratch.sp++] = { child, lo, hi };
        };

        // Deterministic order: right pushed first, left pushed last (popped first)
        pushChild(node.right);
        pushChild(node.left);
    }

    return best;
}

}}} // namespace Engine::Collision::sq
