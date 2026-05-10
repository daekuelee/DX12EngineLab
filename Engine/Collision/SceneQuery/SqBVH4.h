#pragma once
// =========================================================================
// SSOT: docs/audits/scenequery/09-scalar-bvh4-core.md
// SSOT: docs/audits/scenequery/10-bvh4-physx-gap-roadmap.md
// SSOT: docs/audits/scenequery/11-bvh4-simd-child-test-prototype.md
// SSOT: docs/audits/scenequery/12-bvh4-simd-soa-traversal-hardening.md
// REF: docs/reference/physx/contracts/bv4-layout-traversal.md
//
// Scalar BVH4 is an experimental four-child SceneQuery backend core. The SIMD
// child-test path is a packetized AABB rejection prototype over the same nodes.
//
// Invariant: BVH4 traversal reuses the same primitive/contact collectors as BinaryBVH.
// =========================================================================

#include "SqQuery.h"
#include "../../Math/MathCommon.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#if EL_MATH_ENABLE_SIMD
#include <immintrin.h>
#endif

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

struct BVH4NodeBoundsSoA {
    float minX[4]{};
    float minY[4]{};
    float minZ[4]{};
    float maxX[4]{};
    float maxY[4]{};
    float maxZ[4]{};
    uint32_t activeMask = 0;
};

struct BVH4Node {
    BVH4Slot slots[4]{};
    BVH4NodeBoundsSoA boundsSoA{};
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

enum class BVH4ChildTestPath : uint8_t {
    Scalar,
    Packet
};

inline uint32_t CountBVH4Mask(uint32_t mask)
{
    mask &= 0xFu;
    uint32_t count = 0;
    while (mask) {
        count += mask & 1u;
        mask >>= 1u;
    }
    return count;
}

inline void RefreshBVH4NodeBoundsSoA(BVH4Node& node)
{
    BVH4NodeBoundsSoA soa{};
    for (uint32_t i = 0; i < 4; ++i) {
        const BVH4Slot& slot = node.slots[i];
        if (!slot.active)
            continue;

        soa.activeMask |= (1u << i);
        soa.minX[i] = slot.bounds.minX;
        soa.minY[i] = slot.bounds.minY;
        soa.minZ[i] = slot.bounds.minZ;
        soa.maxX[i] = slot.bounds.maxX;
        soa.maxY[i] = slot.bounds.maxY;
        soa.maxZ[i] = slot.bounds.maxZ;
    }
    node.boundsSoA = soa;
}

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
        RefreshBVH4NodeBoundsSoA(node);
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

    RefreshBVH4NodeBoundsSoA(bvh.nodes[nodeIndex]);
    return nodeIndex;
}

struct BVH4SweepChildHit {
    uint32_t slotIndex = 0;
    float tEnter = 0.0f;
    float tExit = 1.0f;
};

inline bool BVH4SweepChildHitLess(const BVH4SweepChildHit& a, const BVH4SweepChildHit& b)
{
    if (a.tEnter != b.tEnter)
        return a.tEnter < b.tEnter;
    return a.slotIndex < b.slotIndex;
}

inline uint32_t GatherBVH4SweepChildHits(
    const BVH4Node& node,
    const AABB& cap0,
    const Vec3& delta,
    const NodeTask& parent,
    float bestT,
    BVH4SweepChildHit* outHits,
    QueryMetrics& metrics)
{
    uint32_t hitCount = 0;

    for (uint32_t i = 0; i < node.childCount; ++i) {
        const BVH4Slot& slot = node.slots[i];
        if (!slot.active)
            continue;

        float cE = parent.tEnter;
        float cL = parent.tExit;
        if (cL > bestT)
            cL = bestT;

        ++metrics.nodeAabbTests;
        if (!AabbAabb_SweepInterval(cap0, delta, slot.bounds, cE, cL)) {
            ++metrics.nodeAabbRejects;
            continue;
        }
        if (cE >= bestT) {
            ++metrics.nodeTimePrunes;
            continue;
        }

        outHits[hitCount++] = { i, cE, cL };
    }

    std::sort(outHits, outHits + hitCount, BVH4SweepChildHitLess);
    return hitCount;
}

#if EL_MATH_ENABLE_SIMD
inline uint32_t RefineBVH4SweepAxisPacket(
    float aMin,
    float aMax,
    float velocity,
    const float* bMin,
    const float* bMax,
    __m128& enter,
    __m128& exit,
    uint32_t mask)
{
    const __m128 bMinV = _mm_loadu_ps(bMin);
    const __m128 bMaxV = _mm_loadu_ps(bMax);

    if (Abs(velocity) < kEpsParallel) {
        const __m128 aMinV = _mm_set1_ps(aMin);
        const __m128 aMaxV = _mm_set1_ps(aMax);
        const __m128 geMin = _mm_cmpge_ps(aMaxV, bMinV);
        const __m128 leMax = _mm_cmple_ps(aMinV, bMaxV);
        return mask & static_cast<uint32_t>(_mm_movemask_ps(_mm_and_ps(geMin, leMax)));
    }

    const __m128 invV = _mm_set1_ps(1.0f / velocity);
    const __m128 aMinV = _mm_set1_ps(aMin);
    const __m128 aMaxV = _mm_set1_ps(aMax);
    const __m128 t0 = _mm_mul_ps(_mm_sub_ps(bMinV, aMaxV), invV);
    const __m128 t1 = _mm_mul_ps(_mm_sub_ps(bMaxV, aMinV), invV);
    const __m128 axisEnter = _mm_min_ps(t0, t1);
    const __m128 axisExit = _mm_max_ps(t0, t1);

    enter = _mm_max_ps(enter, axisEnter);
    exit = _mm_min_ps(exit, axisExit);
    return mask & static_cast<uint32_t>(_mm_movemask_ps(_mm_cmple_ps(enter, exit)));
}
#endif

inline uint32_t GatherBVH4SweepChildHitsPacket(
    const BVH4Node& node,
    const AABB& cap0,
    const Vec3& delta,
    const NodeTask& parent,
    float bestT,
    BVH4SweepChildHit* outHits,
    QueryMetrics& metrics)
{
    const uint32_t activeMask = node.boundsSoA.activeMask;
    const uint32_t activeCount = CountBVH4Mask(activeMask);
    if (!activeMask)
        return 0;

    ++metrics.nodeAabbPackets;
    metrics.nodeAabbPacketLanes += activeCount;
    metrics.nodeAabbTests += activeCount;

    uint32_t intervalMask = activeMask;
    float enterLane[4]{};
    float exitLane[4]{};

#if EL_MATH_ENABLE_SIMD
    __m128 enter = _mm_set1_ps(parent.tEnter);
    __m128 exit = _mm_set1_ps((std::min)(parent.tExit, bestT));

    intervalMask = RefineBVH4SweepAxisPacket(
        cap0.minX, cap0.maxX, delta.x,
        node.boundsSoA.minX, node.boundsSoA.maxX, enter, exit, intervalMask);
    intervalMask = RefineBVH4SweepAxisPacket(
        cap0.minY, cap0.maxY, delta.y,
        node.boundsSoA.minY, node.boundsSoA.maxY, enter, exit, intervalMask);
    intervalMask = RefineBVH4SweepAxisPacket(
        cap0.minZ, cap0.maxZ, delta.z,
        node.boundsSoA.minZ, node.boundsSoA.maxZ, enter, exit, intervalMask);

    _mm_storeu_ps(enterLane, enter);
    _mm_storeu_ps(exitLane, exit);
#else
    for (uint32_t i = 0; i < 4; ++i) {
        if (!(activeMask & (1u << i)))
            continue;

        float cE = parent.tEnter;
        float cL = (std::min)(parent.tExit, bestT);
        if (!AabbAabb_SweepInterval(cap0, delta, node.slots[i].bounds, cE, cL)) {
            intervalMask &= ~(1u << i);
            continue;
        }
        enterLane[i] = cE;
        exitLane[i] = cL;
    }
#endif

    metrics.nodeAabbRejects += activeCount - CountBVH4Mask(intervalMask);

    uint32_t hitCount = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        if (!(intervalMask & (1u << i)))
            continue;
        if (enterLane[i] >= bestT) {
            ++metrics.nodeTimePrunes;
            continue;
        }
        outHits[hitCount++] = { i, enterLane[i], exitLane[i] };
    }

    std::sort(outHits, outHits + hitCount, BVH4SweepChildHitLess);
    return hitCount;
}

inline uint32_t GatherBVH4OverlapChildMaskPacket(
    const BVH4Node& node,
    const AABB& capBounds,
    QueryMetrics& metrics)
{
    const uint32_t activeMask = node.boundsSoA.activeMask;
    const uint32_t activeCount = CountBVH4Mask(activeMask);
    if (!activeMask)
        return 0;

    ++metrics.nodeAabbPackets;
    metrics.nodeAabbPacketLanes += activeCount;
    metrics.nodeAabbTests += activeCount;

#if EL_MATH_ENABLE_SIMD
    const __m128 capMinX = _mm_set1_ps(capBounds.minX);
    const __m128 capMinY = _mm_set1_ps(capBounds.minY);
    const __m128 capMinZ = _mm_set1_ps(capBounds.minZ);
    const __m128 capMaxX = _mm_set1_ps(capBounds.maxX);
    const __m128 capMaxY = _mm_set1_ps(capBounds.maxY);
    const __m128 capMaxZ = _mm_set1_ps(capBounds.maxZ);

    const __m128 x = _mm_and_ps(
        _mm_cmpge_ps(capMaxX, _mm_loadu_ps(node.boundsSoA.minX)),
        _mm_cmple_ps(capMinX, _mm_loadu_ps(node.boundsSoA.maxX)));
    const __m128 y = _mm_and_ps(
        _mm_cmpge_ps(capMaxY, _mm_loadu_ps(node.boundsSoA.minY)),
        _mm_cmple_ps(capMinY, _mm_loadu_ps(node.boundsSoA.maxY)));
    const __m128 z = _mm_and_ps(
        _mm_cmpge_ps(capMaxZ, _mm_loadu_ps(node.boundsSoA.minZ)),
        _mm_cmple_ps(capMinZ, _mm_loadu_ps(node.boundsSoA.maxZ)));

    const uint32_t acceptedMask = activeMask & static_cast<uint32_t>(
        _mm_movemask_ps(_mm_and_ps(_mm_and_ps(x, y), z)));
#else
    uint32_t acceptedMask = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        if ((activeMask & (1u << i)) && TestAabbAabb(capBounds, node.slots[i].bounds))
            acceptedMask |= (1u << i);
    }
#endif

    metrics.nodeAabbRejects += activeCount - CountBVH4Mask(acceptedMask);
    return acceptedMask;
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

inline void VisitBVH4SweepLeafHits(
    const StaticBVH4& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    const AABB& cap0,
    const BVH4Node& node,
    const BVH4SweepChildHit* hits,
    uint32_t hitCount,
    const SweepFilter& filter,
    bool rejectInitialOverlap,
    Hit& best,
    QueryMetrics* metrics)
{
    for (uint32_t i = 0; i < hitCount; ++i) {
        const BVH4Slot& slot = node.slots[hits[i].slotIndex];
        if (slot.leaf) {
            ConsiderBVH4LeafSweep(
                bvh, in, cfg, cap0, slot, hits[i].tEnter, hits[i].tExit,
                filter, rejectInitialOverlap, best, metrics);
        }
    }
}

inline void PushBVH4SweepChildNodes(
    const BVH4Node& node,
    const BVH4SweepChildHit* hits,
    uint32_t hitCount,
    QueryScratch& scratch)
{
    for (uint32_t i = hitCount; i > 0; --i) {
        const BVH4SweepChildHit& hit = hits[i - 1];
        const BVH4Slot& slot = node.slots[hit.slotIndex];
        if (!slot.leaf)
            PushQueryTask(scratch, { slot.index, hit.tEnter, hit.tExit });
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
        InsertOverlapContactTopK(outContacts, maxContacts, contactCount, contact, metrics);
    }
}

template <BVH4ChildTestPath Path>
inline Hit RunBVH4SweepClosest(
    const StaticBVH4& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    QueryScratch& scratch,
    const SweepFilter& filter,
    bool rejectInitialOverlap,
    QueryBackend backend)
{
    Hit best{};
    best.hit = false;
    best.t = 1.0f;

    ResetQueryScratch(scratch, QueryKind::SweepCapsuleClosest, backend);
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
        BVH4SweepChildHit hits[4]{};
        uint32_t hitCount = 0;
        if constexpr (Path == BVH4ChildTestPath::Packet) {
            hitCount = GatherBVH4SweepChildHitsPacket(
                node, cap0, in.delta, task, best.t, hits, scratch.metrics);
        } else {
            hitCount = GatherBVH4SweepChildHits(
                node, cap0, in.delta, task, best.t, hits, scratch.metrics);
        }

        VisitBVH4SweepLeafHits(
            bvh, in, cfg, cap0, node, hits, hitCount,
            filter, rejectInitialOverlap, best, &scratch.metrics);
        PushBVH4SweepChildNodes(node, hits, hitCount, scratch);
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

template <BVH4ChildTestPath Path>
inline uint32_t RunBVH4OverlapContacts(
    const StaticBVH4& bvh,
    const Vec3& segA,
    const Vec3& segB,
    float radius,
    OverlapContact* outContacts,
    uint32_t maxContacts,
    QueryScratch& scratch,
    QueryBackend backend)
{
    ResetQueryScratch(scratch, QueryKind::OverlapCapsuleContacts, backend);
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
        uint32_t acceptedMask = 0;
        if constexpr (Path == BVH4ChildTestPath::Packet) {
            acceptedMask = GatherBVH4OverlapChildMaskPacket(
                node, capBounds, scratch.metrics);
        }

        for (uint32_t i = 0; i < node.childCount; ++i) {
            const BVH4Slot& slot = node.slots[i];
            if (!slot.active)
                continue;

            if constexpr (Path == BVH4ChildTestPath::Packet) {
                if (!(acceptedMask & (1u << i)))
                    continue;
            } else {
                ++scratch.metrics.nodeAabbTests;
                if (!TestAabbAabb(capBounds, slot.bounds)) {
                    ++scratch.metrics.nodeAabbRejects;
                    continue;
                }
            }

            if (slot.leaf) {
                ConsiderBVH4LeafOverlap(
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
    return detail::RunBVH4SweepClosest<detail::BVH4ChildTestPath::Scalar>(
        bvh, in, cfg, scratch, filter, rejectInitialOverlap, QueryBackend::BVH4);
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
    return detail::RunBVH4OverlapContacts<detail::BVH4ChildTestPath::Scalar>(
        bvh, segA, segB, radius, outContacts, maxContacts, scratch,
        QueryBackend::BVH4);
}

inline Hit SweepCapsuleClosestHit_BVH4SimdChildTest(
    const StaticBVH4& bvh,
    const SweepCapsuleInput& in,
    const SweepConfig& cfg,
    QueryScratch& scratch,
    const SweepFilter& filter = SweepFilter{},
    bool rejectInitialOverlap = false)
{
    return detail::RunBVH4SweepClosest<detail::BVH4ChildTestPath::Packet>(
        bvh, in, cfg, scratch, filter, rejectInitialOverlap, QueryBackend::BVH4Simd);
}

inline uint32_t OverlapCapsuleContacts_BVH4SimdChildTest(
    const StaticBVH4& bvh,
    const Vec3& segA,
    const Vec3& segB,
    float radius,
    OverlapContact* outContacts,
    uint32_t maxContacts,
    QueryScratch& scratch)
{
    return detail::RunBVH4OverlapContacts<detail::BVH4ChildTestPath::Packet>(
        bvh, segA, segB, radius, outContacts, maxContacts, scratch,
        QueryBackend::BVH4Simd);
}

}}} // namespace Engine::Collision::sq
