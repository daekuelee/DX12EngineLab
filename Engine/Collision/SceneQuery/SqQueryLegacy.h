#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqQuery.h
//
// TERMINOLOGY:
//   QueryScratch - caller-owned fixed-capacity memory for BVH traversal
//   NodeTask     - BVH traversal stack entry: node index + time window
//   BetterHit    - deterministic hit comparison (t -> type -> index -> feat)
//
// POLICY:
//   - QueryScratch is caller-allocated on stack. No heap allocation during query.
//   - BVH traversal is deterministic and prefers nearer child windows first.
//   - Hit selection uses BetterHit cascade for stable tie-breaking.
//   - Early-out: prune nodes whose tEnter >= current best t.
//   - Stack overflow falls back to a linear scan instead of losing candidates.
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
//   - docs/agent-context/scenequery-refactor.md
//   - docs/reference/physx/contracts/scenequery-pipeline.md
//   - docs/reference/physx/contracts/mesh-sweeps-ordering.md
// =========================================================================

#include "SqNarrowphaseLegacy.h"
#include "SqBVH.h"
#include "SqBroadphase.h"
#include "SqPrimitiveTests.h"

namespace Engine { namespace Collision { namespace sq {

// ---- Traversal stack entry ----------------------------------------------

struct NodeTask {
    uint32_t node;
    float    tEnter;
    float    tExit;
};

// ---- Caller-owned scratch memory ----------------------------------------
// 512 entries for traversal tasks. Overflow falls back to a full scan.

struct QueryScratch {
    static constexpr uint32_t Capacity = 512;

    NodeTask stack[Capacity];
    uint32_t sp = 0;
    uint32_t maxSp = 0;
    bool overflowed = false;
};

inline void ResetQueryScratch(QueryScratch& scratch)
{
    scratch.sp = 0;
    scratch.maxSp = 0;
    scratch.overflowed = false;
}

inline bool PushQueryTask(QueryScratch& scratch, const NodeTask& task)
{
    if (scratch.sp >= QueryScratch::Capacity) {
        scratch.overflowed = true;
        return false;
    }

    scratch.stack[scratch.sp++] = task;
    if (scratch.maxSp < scratch.sp)
        scratch.maxSp = scratch.sp;
    return true;
}

// ---- Deterministic hit comparison (SSOT tie-break cascade) ---------------
// Order: smallest t -> feature class -> lowest PrimType -> lowest prim index -> lowest featureId.
// Feature class derives from packed feature id:
// face (0) < edge (1) < vertex (2) < prism-side (3).
// Within tieEpsT, all levels participate.
inline bool BetterHit(float tNew, PrimType typeNew, uint32_t idxNew, uint32_t featNew,
                      float tBest, PrimType typeBest, uint32_t idxBest, uint32_t featBest,
                      float epsT)
{
    if (tNew < tBest) return true;
    if (Abs(tNew - tBest) <= epsT) {
        const int clsNew = FeatureClassFromPacked(featNew);
        const int clsBest = FeatureClassFromPacked(featBest);
        if (clsNew != clsBest) return clsNew < clsBest;
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
    bool rejectInitialOverlap,
    const SweepFilter* filter,
    float& outT, Vec3& outN, uint32_t& outFeat,
    bool& outStartPenetrating,
    float& outPenetrationDepth)
{
    switch (pref.type) {
        case PrimType::Tri:
            return SweepCapsuleTri_PhysXLike_TOI01(
                in, bvh.tris[pref.index], cfg, outT, outN, outFeat,
                outStartPenetrating, outPenetrationDepth,
                rejectInitialOverlap, filter);

        case PrimType::Aabb:
            return SweepCapsuleAabb_PhysXLike_TOI01(
                in, bvh.aabbs[pref.index], cfg, outT, outN, outFeat,
                outStartPenetrating, outPenetrationDepth,
                rejectInitialOverlap, filter);

        case PrimType::Obb:
            return SweepCapsuleObb_PhysXLike_TOI01(
                in, bvh.obbs[pref.index], cfg, outT, outN, outFeat,
                outStartPenetrating, outPenetrationDepth,
                rejectInitialOverlap, filter);

        default:
            return false;
    }
}

inline void ConsiderSweepCapsulePrim(
    const StaticBVH& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    const AABB& cap0,
    const PrimRef& pref,
    float tEnter,
    float tExit,
    const SweepFilter& filter,
    bool rejectInitialOverlap,
    Hit& best)
{
    if (tExit > best.t) tExit = best.t;

    if (!AabbAabb_SweepInterval(cap0, in.delta, pref.bounds, tEnter, tExit))
        return;
    if (tEnter >= best.t)
        return;

    float t; Vec3 n; uint32_t f;
    bool startPenetrating = false;
    float penetrationDepth = 0.0f;
    if (!SweepCapsulePrim_TOI01(bvh, in, cfg, pref,
                               rejectInitialOverlap,
                               filter.active ? &filter : nullptr,
                               t, n, f, startPenetrating,
                               penetrationDepth))
        return;

    if (filter.active) {
        const bool applyFilter = !startPenetrating || filter.filterInitialOverlap;
        if (applyFilter && Dot(n, filter.refDir) < filter.minDot)
            return;
    }

    if (t < tEnter || t > tExit)
        return;

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
        best.startPenetrating = startPenetrating;
        best.penetrationDepth = penetrationDepth;
    }
}

inline Hit SweepCapsuleClosestHit_LinearFallback(
    const StaticBVH& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    const SweepFilter& filter,
    bool rejectInitialOverlap)
{
    Hit best{};
    best.hit = false;
    best.t = 1.0f;

    if (IsEmptyBVH(bvh))
        return best;

    const AABB cap0 = CapsuleAabbAtT(in, 0.0f, cfg.skin);
    for (uint32_t i = 0; i < static_cast<uint32_t>(bvh.prims.size()); ++i) {
        const PrimRef& pref = bvh.prims[i];
        ConsiderSweepCapsulePrim(bvh, in, cfg, cap0, pref, 0.0f, best.t,
                                 filter, rejectInitialOverlap, best);
    }

    return best;
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
//      - Internal: push farther child first so nearer child is popped first
//   4. Return best Hit
inline Hit SweepCapsuleClosestHit_Fast(
    const StaticBVH& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    QueryScratch& scratch,
    const SweepFilter& filter = SweepFilter{},
    bool rejectInitialOverlap = false)
{
    Hit best{};
    best.hit = false;
    best.t = 1.0f;

    ResetQueryScratch(scratch);

    if (IsEmptyBVH(bvh))
        return best;

    // Moving capsule AABB at t=0 expanded by skin (match narrowphase radius+skin)
    AABB cap0 = CapsuleAabbAtT(in, 0.0f, cfg.skin);

    float rE = 0.0f;
    float rL = best.t;
    if (!AabbAabb_SweepInterval(cap0, in.delta, bvh.nodes[bvh.root].bounds, rE, rL))
        return best;

    PushQueryTask(scratch, { bvh.root, rE, rL });

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
                ConsiderSweepCapsulePrim(bvh, in, cfg, cap0, pref,
                                         task.tEnter, task.tExit,
                                         filter, rejectInitialOverlap, best);
            }
            continue;
        }

        auto makeChildTask = [&](uint32_t child, NodeTask& out) -> bool {
            float cE = task.tEnter;
            float cL = task.tExit;
            if (cL > best.t) cL = best.t;

            if (!AabbAabb_SweepInterval(cap0, in.delta, bvh.nodes[child].bounds, cE, cL))
                return false;
            if (cE >= best.t) return false;

            out = { child, cE, cL };
            return true;
        };

        // Deterministic near-first traversal: push farther child first because
        // the stack is LIFO. Equal tEnter keeps left before right.
        NodeTask leftTask{};
        NodeTask rightTask{};
        const bool leftHit = makeChildTask(node.left, leftTask);
        const bool rightHit = makeChildTask(node.right, rightTask);
        if (leftHit && rightHit) {
            const bool leftFirst = leftTask.tEnter <= rightTask.tEnter;
            PushQueryTask(scratch, leftFirst ? rightTask : leftTask);
            PushQueryTask(scratch, leftFirst ? leftTask : rightTask);
        } else if (rightHit) {
            PushQueryTask(scratch, rightTask);
        } else if (leftHit) {
            PushQueryTask(scratch, leftTask);
        }
    }

    if (scratch.overflowed)
        return SweepCapsuleClosestHit_LinearFallback(
            bvh, in, cfg, filter, rejectInitialOverlap);

    return best;
}

// =========================================================================
// Overlap query: capsule overlap contacts via BVH DFS
// =========================================================================
//
// Consumes: immutable StaticBVH, capsule segment + radius, QueryScratch
// Produces: up to maxContacts OverlapContact entries, sorted deterministically
//
// Algorithm:
//   1. Compute static capsule AABB (broadphase bounds)
//   2. DFS traversal: nodes whose AABB overlaps capsule AABB
//   3. Leaf: test each primitive via narrowphase overlap kernel
//   4. Collect up to kMaxContacts, keep deepest via Top-K eviction
//   5. Sort contacts by (-depth, type, index, featureId) for determinism
//
// Determinism rules:
//   - BVH traversal: stack DFS, right pushed first, left popped first
//   - Contact sorting: total order via std::sort
//   - Top-K eviction: strict > on depth; same-depth kept in DFS order

inline constexpr uint32_t kMaxOverlapContacts = 32;

inline bool OverlapContactBetter(const OverlapContact& a, const OverlapContact& b)
{
    // Deepest first (descending depth)
    if (a.depth != b.depth) return a.depth > b.depth;
    // Tie-break cascade: type -> index -> featureId (ascending)
    if ((uint8_t)a.type != (uint8_t)b.type) return (uint8_t)a.type < (uint8_t)b.type;
    if (a.index != b.index) return a.index < b.index;
    return a.featureId < b.featureId;
}

inline void InsertOverlapContactTopK(
    OverlapContact* outContacts,
    uint32_t maxContacts,
    uint32_t& contactCount,
    const OverlapContact& contact)
{
    if (contactCount < maxContacts) {
        outContacts[contactCount++] = contact;
        return;
    }

    uint32_t shallowest = 0;
    for (uint32_t i = 1; i < contactCount; ++i) {
        if (outContacts[i].depth < outContacts[shallowest].depth)
            shallowest = i;
    }

    if (contact.depth > outContacts[shallowest].depth)
        outContacts[shallowest] = contact;
}

// Per-primitive overlap dispatch
inline bool OverlapCapsulePrim(
    const StaticBVH& bvh,
    const Vec3& segA, const Vec3& segB, float radius,
    const PrimRef& pref,
    OverlapContact& out)
{
    switch (pref.type) {
        case PrimType::Aabb:
            return OverlapCapsuleAabb(segA, segB, radius,
                                     bvh.aabbs[pref.index], out);
        case PrimType::Obb:
            return OverlapCapsuleObb(segA, segB, radius,
                                    bvh.obbs[pref.index], out);
        case PrimType::Tri:
            return OverlapCapsuleTri(segA, segB, radius,
                                    bvh.tris[pref.index], out);
        default:
            return false;
    }
}

inline uint32_t OverlapCapsuleContacts_LinearFallback(
    const StaticBVH& bvh,
    const Vec3& segA, const Vec3& segB, float radius,
    OverlapContact* outContacts, uint32_t maxContacts)
{
    if (maxContacts == 0) return 0;
    if (maxContacts > kMaxOverlapContacts) maxContacts = kMaxOverlapContacts;
    if (IsEmptyBVH(bvh)) return 0;

    const AABB capBounds = CapsuleAabbStatic(segA, segB, radius);
    uint32_t contactCount = 0;

    for (uint32_t i = 0; i < static_cast<uint32_t>(bvh.prims.size()); ++i) {
        const PrimRef& pref = bvh.prims[i];
        if (!TestAabbAabb(capBounds, pref.bounds))
            continue;

        OverlapContact contact;
        if (!OverlapCapsulePrim(bvh, segA, segB, radius, pref, contact))
            continue;

        contact.type = pref.type;
        contact.index = pref.index;
        InsertOverlapContactTopK(outContacts, maxContacts, contactCount, contact);
    }

    std::sort(outContacts, outContacts + contactCount, OverlapContactBetter);
    return contactCount;
}

inline uint32_t OverlapCapsuleContacts_Fast(
    const StaticBVH& bvh,
    const Vec3& segA, const Vec3& segB, float radius,
    OverlapContact* outContacts, uint32_t maxContacts,
    QueryScratch& scratch)
{
    if (maxContacts == 0) return 0;
    if (maxContacts > kMaxOverlapContacts) maxContacts = kMaxOverlapContacts;

    ResetQueryScratch(scratch);
    if (IsEmptyBVH(bvh)) return 0;

    AABB capBounds = CapsuleAabbStatic(segA, segB, radius);
    uint32_t contactCount = 0;

    if (!TestAabbAabb(capBounds, bvh.nodes[bvh.root].bounds))
        return 0;

    PushQueryTask(scratch, { bvh.root, 0.0f, 1.0f });

    while (scratch.sp) {
        NodeTask task = scratch.stack[--scratch.sp];
        const BVHNode& node = bvh.nodes[task.node];

        if (node.primCount) {
            for (uint32_t k = 0; k < node.primCount; ++k) {
                const PrimRef& pref = bvh.prims[bvh.primIdx[node.primStart + k]];
                if (!TestAabbAabb(capBounds, pref.bounds))
                    continue;

                OverlapContact contact;
                if (!OverlapCapsulePrim(bvh, segA, segB, radius, pref, contact))
                    continue;

                contact.type = pref.type;
                contact.index = pref.index;
                InsertOverlapContactTopK(outContacts, maxContacts, contactCount, contact);
            }
            continue;
        }

        if (TestAabbAabb(capBounds, bvh.nodes[node.right].bounds))
            PushQueryTask(scratch, { node.right, 0.0f, 1.0f });
        if (TestAabbAabb(capBounds, bvh.nodes[node.left].bounds))
            PushQueryTask(scratch, { node.left, 0.0f, 1.0f });
    }

    if (scratch.overflowed)
        return OverlapCapsuleContacts_LinearFallback(
            bvh, segA, segB, radius, outContacts, maxContacts);

    std::sort(outContacts, outContacts + contactCount, OverlapContactBetter);
    return contactCount;
}

}}} // namespace Engine::Collision::sq
