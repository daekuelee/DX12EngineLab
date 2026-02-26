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
//   - SweepCapsulePolicy is the policy-driven entry point.
//   - SweepCapsuleClosestBlock_Fast is a compatibility wrapper.
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
#include "SqCallbacks.h"
#include "SqPrimitiveTests.h"
#include <algorithm>
#include <vector>

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
    float& outT, Vec3& outN, uint32_t& outFeat)
{
    switch (pref.type) {
        case PrimType::Tri:
            return SweepCapsuleTri_PhysXLike_TOI01(
                in, bvh.tris[pref.index], cfg, outT, outN, outFeat,
                rejectInitialOverlap, filter);

        case PrimType::Aabb:
            return SweepCapsuleAabb_PhysXLike_TOI01(
                in, bvh.aabbs[pref.index], cfg, outT, outN, outFeat,
                rejectInitialOverlap, filter);

        case PrimType::Obb:
            return SweepCapsuleObb_PhysXLike_TOI01(
                in, bvh.obbs[pref.index], cfg, outT, outN, outFeat,
                rejectInitialOverlap, filter);

        default:
            return false;
    }
}

struct SweepTouch {
    float t = 1.0f;
    PrimType type = PrimType::Aabb;
    uint32_t index = 0;
    Vec3 normal{0, 1, 0};
    uint32_t featureId = 0;
    bool isInitialOverlap = false;
};

struct SweepPolicyResult {
    Hit block{};
    std::vector<SweepTouch> touches;
    SweepPolicyDebugTrace debug{};

    inline bool HasBlock() const { return block.hit; }
};

static constexpr uint32_t kSweepTouchReserveDefault = 32;

// Bind callback debug pointer if callback type exposes `policyDebug`.
template <typename CallbackT>
inline auto BindPolicyDebugTrace(CallbackT& callback,
                                 SweepPolicyDebugTrace* debugTrace,
                                 int) -> decltype(callback.policyDebug = debugTrace, void())
{
    callback.policyDebug = debugTrace;
}

template <typename CallbackT>
inline void BindPolicyDebugTrace(CallbackT&,
                                 SweepPolicyDebugTrace*,
                                 long)
{
}

// =========================================================================
// Main query: capsule sweep policy result via BVH DFS (callback policy)
// =========================================================================
//
// TEMPLATE CONTRACT:
//   Callback must provide:
//     QueryHitType PreFilter(const PrimRef&)
//     QueryHitType PostFilter(const PrimRef&, float t, const Vec3& n,
//                             uint32_t featureId, bool isInitialOverlap,
//                             bool rejectInitialOverlapEnabled)
//
// PARAMETERS:
//   narrowphaseFilter:
//     Optional low-level filter forwarded to narrowphase kernels.
//     Use nullptr for no narrowphase-time filtering.
//
// RETURN:
//   Policy output containing:
//     - closest blocking hit,
//     - touch list (trimmed by Cut-at-Block),
//     - debug counters.
template <typename Callback>
inline SweepPolicyResult SweepCapsulePolicy(
    const StaticBVH& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    QueryScratch& scratch,
    const SweepFilter* narrowphaseFilter,
    Callback& callback,
    bool rejectInitialOverlap = false)
{
    SweepPolicyResult out{};
    out.block.hit = false;
    out.block.t = 1.0f;
    out.touches.reserve(kSweepTouchReserveDefault);

    Callback callbackCopy = callback;
    BindPolicyDebugTrace(callbackCopy, &out.debug, 0);

    // Moving capsule AABB at t=0 expanded by skin (match narrowphase radius+skin)
    AABB cap0 = CapsuleAabbAtT(in, 0.0f, cfg.skin);

    scratch.sp = 0;

    float rE, rL;
    if (!AabbAabb_SweepInterval01(cap0, in.delta, bvh.nodes[bvh.root].bounds, rE, rL))
        return out;

    scratch.stack[scratch.sp++] = { bvh.root, rE, rL };

    while (scratch.sp) {
        NodeTask task = scratch.stack[--scratch.sp];

        if (task.tEnter >= out.block.t) continue;
        if (task.tExit  > out.block.t)  task.tExit = out.block.t;
        if (task.tEnter > task.tExit) continue;

        const BVHNode& node = bvh.nodes[task.node];

        // Leaf node: test primitives
        if (node.primCount) {
            for (uint32_t k = 0; k < node.primCount; ++k) {
                const PrimRef& pref = bvh.prims[bvh.primIdx[node.primStart + k]];

                if (callbackCopy.PreFilter(pref) == QueryHitType::None) {
                    out.debug.preFilterRejects++;
                    continue;
                }

                float pE, pL;
                if (!AabbAabb_SweepInterval01(cap0, in.delta, pref.bounds, pE, pL))
                    continue;

                float lo = (std::max)(task.tEnter, pE);
                float hi = (std::min)(task.tExit,  pL);
                if (lo >= out.block.t || lo > hi) continue;

                float t; Vec3 n; uint32_t f;
                if (!SweepCapsulePrim_TOI01(bvh, in, cfg, pref,
                                           rejectInitialOverlap,
                                           narrowphaseFilter,
                                           t, n, f))
                    continue;

                const bool isInitialOverlap = (t <= 0.0f);
                const QueryHitType hitType =
                    callbackCopy.PostFilter(pref, t, n, f, isInitialOverlap, rejectInitialOverlap);
                if (hitType == QueryHitType::None) continue;

                if (t < lo || t > hi) continue;

                if (hitType == QueryHitType::Touch) {
                    if (!out.HasBlock() || t <= (out.block.t + cfg.tieEpsT)) {
                        out.touches.push_back({ t, pref.type, pref.index, n, f, isInitialOverlap });
                        out.debug.acceptedTouches++;
                    }
                    continue;
                }

                if (!out.block.hit || BetterHit(t, pref.type, pref.index, f,
                                                out.block.t, out.block.type, out.block.index,
                                                out.block.featureId, cfg.tieEpsT))
                {
                    out.block.hit = true;
                    out.block.t = t;
                    out.block.type = pref.type;
                    out.block.index = pref.index;
                    out.block.normal = n;
                    out.block.featureId = f;

                    // Cut-at-Block: keep touches only up to earliest block.
                    auto newEnd = std::remove_if(out.touches.begin(), out.touches.end(),
                        [&](const SweepTouch& touch) { return touch.t > (out.block.t + cfg.tieEpsT); });
                    out.touches.erase(newEnd, out.touches.end());
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
            if (hi > out.block.t) hi = out.block.t;
            if (lo > hi) return;
            scratch.stack[scratch.sp++] = { child, lo, hi };
        };

        // Deterministic order: right pushed first, left pushed last (popped first)
        pushChild(node.right);
        pushChild(node.left);
    }

    std::sort(out.touches.begin(), out.touches.end(),
        [&](const SweepTouch& a, const SweepTouch& b) {
            return BetterHit(a.t, a.type, a.index, a.featureId,
                             b.t, b.type, b.index, b.featureId, cfg.tieEpsT);
        });

    return out;
}

// =========================================================================
// Main query: capsule sweep closest blocking hit via BVH DFS
// =========================================================================
//
// Compatibility wrapper that preserves the old function signature while
// routing through the policy-based core path.
inline Hit SweepCapsuleClosestBlock_Fast(
    const StaticBVH& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    QueryScratch& scratch,
    const SweepFilter& filter = SweepFilter{},
    bool rejectInitialOverlap = false)
{
    CharacterMoveSweepCallback callback;
    callback.filter = filter.active ? &filter : nullptr;
    callback.policyDebug = nullptr;

    SweepPolicyResult policy = SweepCapsulePolicy(
        bvh, in, cfg, scratch,
        filter.active ? &filter : nullptr,
        callback,
        rejectInitialOverlap);
    return policy.block;
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

inline uint32_t OverlapCapsuleContacts_Fast(
    const StaticBVH& bvh,
    const Vec3& segA, const Vec3& segB, float radius,
    OverlapContact* outContacts, uint32_t maxContacts,
    QueryScratch& scratch)
{
    if (maxContacts == 0) return 0;
    if (maxContacts > kMaxOverlapContacts) maxContacts = kMaxOverlapContacts;

    // Broadphase: static capsule AABB
    AABB capBounds = CapsuleAabbStatic(segA, segB, radius);

    scratch.sp = 0;
    uint32_t contactCount = 0;

    // Check root node overlap
    if (!TestAabbAabb(capBounds, bvh.nodes[bvh.root].bounds))
        return 0;

    // Push root (tEnter/tExit unused for overlap, but NodeTask struct requires them)
    scratch.stack[scratch.sp++] = { bvh.root, 0.0f, 1.0f };

    while (scratch.sp) {
        NodeTask task = scratch.stack[--scratch.sp];
        const BVHNode& node = bvh.nodes[task.node];

        // Leaf node: test primitives
        if (node.primCount) {
            for (uint32_t k = 0; k < node.primCount; ++k) {
                const PrimRef& pref = bvh.prims[bvh.primIdx[node.primStart + k]];

                // Broadphase: capsule AABB vs primitive AABB
                if (!TestAabbAabb(capBounds, pref.bounds))
                    continue;

                // Narrowphase
                OverlapContact contact;
                if (!OverlapCapsulePrim(bvh, segA, segB, radius, pref, contact))
                    continue;

                contact.type = pref.type;
                contact.index = pref.index;

                // Top-K insertion: if buffer full, evict shallowest
                if (contactCount < maxContacts) {
                    outContacts[contactCount++] = contact;
                } else {
                    // Find shallowest contact
                    uint32_t shallowest = 0;
                    for (uint32_t i = 1; i < contactCount; ++i) {
                        if (outContacts[i].depth < outContacts[shallowest].depth)
                            shallowest = i;
                    }
                    // Evict if new contact is deeper (strict >)
                    if (contact.depth > outContacts[shallowest].depth) {
                        outContacts[shallowest] = contact;
                    }
                }
            }
            continue;
        }

        // Internal node: push children (right first for deterministic left-first popping)
        if (TestAabbAabb(capBounds, bvh.nodes[node.right].bounds))
            scratch.stack[scratch.sp++] = { node.right, 0.0f, 1.0f };
        if (TestAabbAabb(capBounds, bvh.nodes[node.left].bounds))
            scratch.stack[scratch.sp++] = { node.left, 0.0f, 1.0f };
    }

    // Sort contacts deterministically: (-depth, type, index, featureId)
    std::sort(outContacts, outContacts + contactCount, OverlapContactBetter);

    return contactCount;
}

}}} // namespace Engine::Collision::sq
