#pragma once
// =========================================================================
// SSOT: docs/audits/scenequery/09-scalar-bvh4-core.md
// REF: docs/reference/physx/contracts/bv4-layout-traversal.md
//
// Scalar BVH4 is an experimental four-child SceneQuery backend core. It is
// intended as future mesh-midphase groundwork and must not encode KCC policy.
//
// Invariant: BVH4 traversal reuses the same primitive collectors as BinaryBVH.
// =========================================================================

#include "SqQuery.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace Engine { namespace Collision { namespace sq {

struct BVH4BuildCtx {
    uint32_t leafSize = 4;
    float centroidEps = 1e-6f;
};

struct BVH4Slot {
    AABB bounds{};
    uint32_t index = 0; // leaf: primIdx start, internal: node index
    uint32_t count = 0; // leaf: primitive count, internal: 0
    bool active = false;
    bool leaf = false;
};

struct BVH4Node {
    BVH4Slot slots[4]{};
    uint32_t childCount = 0;
};

struct StaticBVH4 {
    StaticBVH sourceView{};
    std::vector<uint32_t> primIdx;
    std::vector<BVH4Node> nodes;
    uint32_t root = 0;
};

inline bool IsEmptyBVH4(const StaticBVH4& bvh)
{
    return bvh.nodes.empty() || bvh.sourceView.prims.empty();
}

namespace detail {

struct BVH4RangeInfo {
    AABB bounds{};
    AABB centroidBounds{};
};

inline BVH4RangeInfo ComputeBVH4RangeInfo(const StaticBVH4& bvh,
                                          uint32_t start,
                                          uint32_t count)
{
    const float inf = std::numeric_limits<float>::infinity();
    BVH4RangeInfo info{};
    info.bounds = {+inf, +inf, +inf, -inf, -inf, -inf};
    info.centroidBounds = info.bounds;

    for (uint32_t i = 0; i < count; ++i) {
        const PrimRef& p = bvh.sourceView.prims[bvh.primIdx[start + i]];
        info.bounds = UnionAABB(info.bounds, p.bounds);
        info.centroidBounds.minX = (std::min)(info.centroidBounds.minX, p.centroid.x);
        info.centroidBounds.minY = (std::min)(info.centroidBounds.minY, p.centroid.y);
        info.centroidBounds.minZ = (std::min)(info.centroidBounds.minZ, p.centroid.z);
        info.centroidBounds.maxX = (std::max)(info.centroidBounds.maxX, p.centroid.x);
        info.centroidBounds.maxY = (std::max)(info.centroidBounds.maxY, p.centroid.y);
        info.centroidBounds.maxZ = (std::max)(info.centroidBounds.maxZ, p.centroid.z);
    }

    return info;
}

inline float CentroidAxisValue(const PrimRef& p, int axis)
{
    return axis == 0 ? p.centroid.x : axis == 1 ? p.centroid.y : p.centroid.z;
}

inline void SortBVH4Range(StaticBVH4& bvh, uint32_t start, uint32_t count, int axis)
{
    auto first = bvh.primIdx.begin() + start;
    auto last = first + count;
    std::stable_sort(first, last, [&](uint32_t ia, uint32_t ib) {
        const PrimRef& a = bvh.sourceView.prims[ia];
        const PrimRef& b = bvh.sourceView.prims[ib];
        const float ca = CentroidAxisValue(a, axis);
        const float cb = CentroidAxisValue(b, axis);
        if (ca < cb) return true;
        if (ca > cb) return false;
        if (static_cast<uint8_t>(a.type) != static_cast<uint8_t>(b.type))
            return static_cast<uint8_t>(a.type) < static_cast<uint8_t>(b.type);
        return a.index < b.index;
    });
}

inline bool ShouldMakeBVH4Leaf(uint32_t count, const AABB& centroidBounds,
                               const BVH4BuildCtx& ctx)
{
    return count <= ctx.leafSize || DegenerateCentroids(centroidBounds, ctx.centroidEps);
}

inline uint32_t BuildBVH4NodeRange(StaticBVH4& bvh,
                                   uint32_t start,
                                   uint32_t count,
                                   const BVH4BuildCtx& ctx)
{
    const uint32_t nodeIndex = static_cast<uint32_t>(bvh.nodes.size());
    bvh.nodes.push_back(BVH4Node{});

    const BVH4RangeInfo info = ComputeBVH4RangeInfo(bvh, start, count);
    if (ShouldMakeBVH4Leaf(count, info.centroidBounds, ctx)) {
        BVH4Node& node = bvh.nodes[nodeIndex];
        node.childCount = 1;
        node.slots[0].active = true;
        node.slots[0].leaf = true;
        node.slots[0].bounds = info.bounds;
        node.slots[0].index = start;
        node.slots[0].count = count;
        return nodeIndex;
    }

    const int axis = ChooseAxis(info.centroidBounds);
    SortBVH4Range(bvh, start, count, axis);

    const uint32_t childCount = (std::min)(4u, count);
    const uint32_t baseCount = count / childCount;
    const uint32_t remainder = count % childCount;

    uint32_t rangeStart = start;
    bvh.nodes[nodeIndex].childCount = childCount;

    for (uint32_t child = 0; child < childCount; ++child) {
        const uint32_t rangeCount = baseCount + (child < remainder ? 1u : 0u);
        const BVH4RangeInfo childInfo = ComputeBVH4RangeInfo(bvh, rangeStart, rangeCount);

        BVH4Slot slot{};
        slot.active = true;
        slot.bounds = childInfo.bounds;
        if (ShouldMakeBVH4Leaf(rangeCount, childInfo.centroidBounds, ctx)) {
            slot.leaf = true;
            slot.index = rangeStart;
            slot.count = rangeCount;
        } else {
            slot.leaf = false;
            slot.index = BuildBVH4NodeRange(bvh, rangeStart, rangeCount, ctx);
            slot.count = 0;
        }

        bvh.nodes[nodeIndex].slots[child] = slot;
        rangeStart += rangeCount;
    }

    return nodeIndex;
}

struct BVH4SlotTask {
    uint32_t slotIndex = 0;
    float tEnter = 0.0f;
    float tExit = 1.0f;
};

inline bool BVH4SlotTaskLess(const BVH4SlotTask& a, const BVH4SlotTask& b)
{
    if (a.tEnter != b.tEnter)
        return a.tEnter < b.tEnter;
    return a.slotIndex < b.slotIndex;
}

inline void InsertBVH4OverlapContactTopK(
    OverlapContact* outContacts,
    uint32_t maxContacts,
    uint32_t& contactCount,
    const OverlapContact& contact,
    QueryMetrics* metrics)
{
    if (metrics)
        ++metrics->contactsGenerated;

    if (contactCount < maxContacts) {
        outContacts[contactCount++] = contact;
        return;
    }

    uint32_t worst = 0;
    for (uint32_t i = 1; i < contactCount; ++i) {
        if (OverlapContactBetter(outContacts[worst], outContacts[i]))
            worst = i;
    }

    if (OverlapContactBetter(contact, outContacts[worst])) {
        if (metrics)
            ++metrics->contactsEvicted;
        outContacts[worst] = contact;
    }
}

inline void ConsiderBVH4LeafSweep(
    const StaticBVH4& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    const AABB& cap0,
    const BVH4Slot& slot,
    float tEnter,
    float tExit,
    const SweepFilter& filter,
    bool rejectInitialOverlap,
    Hit& best,
    QueryMetrics* metrics)
{
    if (metrics)
        ++metrics->leafNodesVisited;

    for (uint32_t i = 0; i < slot.count; ++i) {
        const PrimRef& pref = bvh.sourceView.prims[bvh.primIdx[slot.index + i]];
        ConsiderSweepCapsulePrim(bvh.sourceView, in, cfg, cap0, pref,
                                 tEnter, tExit, filter, rejectInitialOverlap,
                                 best, metrics);
    }
}

inline void ConsiderBVH4LeafOverlap(
    const StaticBVH4& bvh,
    const Vec3& segA,
    const Vec3& segB,
    float radius,
    const AABB& capBounds,
    const BVH4Slot& slot,
    OverlapContact* outContacts,
    uint32_t maxContacts,
    uint32_t& contactCount,
    QueryMetrics* metrics)
{
    if (metrics)
        ++metrics->leafNodesVisited;

    for (uint32_t i = 0; i < slot.count; ++i) {
        const PrimRef& pref = bvh.sourceView.prims[bvh.primIdx[slot.index + i]];
        if (metrics)
            ++metrics->primitiveAabbTests;
        if (!TestAabbAabb(capBounds, pref.bounds)) {
            if (metrics)
                ++metrics->primitiveAabbRejects;
            continue;
        }

        OverlapContact contact{};
        if (metrics)
            ++metrics->narrowphaseCalls;
        if (!OverlapCapsulePrim(bvh.sourceView, segA, segB, radius, pref, contact))
            continue;

        if (metrics) {
            ++metrics->rawHits;
            ++metrics->acceptedHits;
        }

        contact.type = pref.type;
        contact.index = pref.index;
        InsertBVH4OverlapContactTopK(outContacts, maxContacts, contactCount, contact, metrics);
    }
}

} // namespace detail

inline StaticBVH4 BuildStaticBVH4(const StaticBVH& source,
                                  const BVH4BuildCtx& ctx = {})
{
    StaticBVH4 bvh{};
    bvh.sourceView = source;

    bvh.primIdx.resize(bvh.sourceView.prims.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(bvh.primIdx.size()); ++i)
        bvh.primIdx[i] = i;

    if (bvh.sourceView.prims.empty()) {
        bvh.nodes.push_back(BVH4Node{});
        bvh.root = 0;
        return bvh;
    }

    bvh.nodes.reserve((bvh.sourceView.prims.size() + 3) / 4);
    bvh.root = detail::BuildBVH4NodeRange(
        bvh, 0, static_cast<uint32_t>(bvh.sourceView.prims.size()), ctx);
    return bvh;
}

inline Hit SweepCapsuleClosestHit_BVH4(
    const StaticBVH4& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    QueryScratch& scratch,
    const SweepFilter& filter = SweepFilter{},
    bool rejectInitialOverlap = false)
{
    Hit best{};
    best.hit = false;
    best.t = 1.0f;

    ResetQueryScratch(scratch, QueryKind::SweepCapsuleClosest, QueryBackend::BVH4);
    if (IsEmptyBVH4(bvh))
        return best;

    const AABB cap0 = CapsuleAabbAtT(in, 0.0f, cfg.skin);
    PushQueryTask(scratch, { bvh.root, 0.0f, best.t });

    while (scratch.sp) {
        NodeTask task = scratch.stack[--scratch.sp];
        ++scratch.metrics.nodesPopped;

        if (task.tEnter >= best.t) {
            ++scratch.metrics.nodeTimePrunes;
            continue;
        }
        if (task.tExit > best.t)
            task.tExit = best.t;
        if (task.tEnter > task.tExit) {
            ++scratch.metrics.nodeTimePrunes;
            continue;
        }

        const BVH4Node& node = bvh.nodes[task.node];
        detail::BVH4SlotTask hits[4]{};
        uint32_t hitCount = 0;

        for (uint32_t i = 0; i < node.childCount; ++i) {
            const BVH4Slot& slot = node.slots[i];
            if (!slot.active)
                continue;

            float cE = task.tEnter;
            float cL = task.tExit;
            if (cL > best.t)
                cL = best.t;

            ++scratch.metrics.nodeAabbTests;
            if (!AabbAabb_SweepInterval(cap0, in.delta, slot.bounds, cE, cL)) {
                ++scratch.metrics.nodeAabbRejects;
                continue;
            }
            if (cE >= best.t) {
                ++scratch.metrics.nodeTimePrunes;
                continue;
            }

            hits[hitCount++] = {i, cE, cL};
        }

        std::sort(hits, hits + hitCount, detail::BVH4SlotTaskLess);

        for (uint32_t i = 0; i < hitCount; ++i) {
            const BVH4Slot& slot = node.slots[hits[i].slotIndex];
            if (slot.leaf) {
                detail::ConsiderBVH4LeafSweep(
                    bvh, in, cfg, cap0, slot, hits[i].tEnter, hits[i].tExit,
                    filter, rejectInitialOverlap, best, &scratch.metrics);
            }
        }

        for (uint32_t i = hitCount; i > 0; --i) {
            const detail::BVH4SlotTask& hit = hits[i - 1];
            const BVH4Slot& slot = node.slots[hit.slotIndex];
            if (!slot.leaf)
                PushQueryTask(scratch, { slot.index, hit.tEnter, hit.tExit });
        }
    }

    if (scratch.overflowed) {
        Hit fallback = SweepCapsuleClosestHit_LinearFallback(
            bvh.sourceView, in, cfg, filter, rejectInitialOverlap, &scratch.metrics);
        FinishSweepQueryMetrics(scratch.metrics, fallback);
        return fallback;
    }

    FinishSweepQueryMetrics(scratch.metrics, best);
    return best;
}

inline uint32_t OverlapCapsuleContacts_BVH4(
    const StaticBVH4& bvh,
    const Vec3& segA,
    const Vec3& segB,
    float radius,
    OverlapContact* outContacts,
    uint32_t maxContacts,
    QueryScratch& scratch)
{
    ResetQueryScratch(scratch, QueryKind::OverlapCapsuleContacts, QueryBackend::BVH4);
    if (maxContacts == 0) {
        FinishOverlapQueryMetrics(scratch.metrics, 0);
        return 0;
    }
    if (maxContacts > kMaxOverlapContacts)
        maxContacts = kMaxOverlapContacts;
    if (IsEmptyBVH4(bvh)) {
        FinishOverlapQueryMetrics(scratch.metrics, 0);
        return 0;
    }

    const AABB capBounds = CapsuleAabbStatic(segA, segB, radius);
    uint32_t contactCount = 0;
    PushQueryTask(scratch, { bvh.root, 0.0f, 1.0f });

    while (scratch.sp) {
        NodeTask task = scratch.stack[--scratch.sp];
        ++scratch.metrics.nodesPopped;

        const BVH4Node& node = bvh.nodes[task.node];

        for (uint32_t i = 0; i < node.childCount; ++i) {
            const BVH4Slot& slot = node.slots[i];
            if (!slot.active)
                continue;

            ++scratch.metrics.nodeAabbTests;
            if (!TestAabbAabb(capBounds, slot.bounds)) {
                ++scratch.metrics.nodeAabbRejects;
                continue;
            }

            if (slot.leaf) {
                detail::ConsiderBVH4LeafOverlap(
                    bvh, segA, segB, radius, capBounds, slot,
                    outContacts, maxContacts, contactCount, &scratch.metrics);
            } else {
                PushQueryTask(scratch, { slot.index, 0.0f, 1.0f });
            }
        }
    }

    if (scratch.overflowed) {
        const uint32_t fallbackCount = OverlapCapsuleContacts_LinearFallback(
            bvh.sourceView, segA, segB, radius, outContacts, maxContacts,
            &scratch.metrics);
        FinishOverlapQueryMetrics(scratch.metrics, fallbackCount);
        return fallbackCount;
    }

    std::sort(outContacts, outContacts + contactCount, OverlapContactBetter);
    FinishOverlapQueryMetrics(scratch.metrics, contactCount);
    return contactCount;
}

}}} // namespace Engine::Collision::sq
