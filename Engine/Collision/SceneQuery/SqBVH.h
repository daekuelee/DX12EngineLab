#pragma once
// =========================================================================
// SSOT: Engine/Collision/SceneQuery/SqBVH.h
//
// TERMINOLOGY:
//   BVH       - Bounding Volume Hierarchy (binary tree of AABBs)
//   StaticBVH - immutable BVH built once from a set of primitives
//   PrimRef   - reference to a primitive (type + index + bounds + centroid)
//   Leaf      - BVH node with primCount > 0 (stores primitives directly)
//
// POLICY:
//   - BVH is built deterministically: std::stable_sort on (centroid, type, index).
//   - Median split on longest axis of centroid bounding box.
//   - No SAH (surface area heuristic) — simple and deterministic is the goal.
//   - No dynamic updates. Rebuild if geometry changes.
//
// CONTRACT:
//   - Standalone: includes only SqTypes.h + <algorithm>.
//   - Build is deterministic: identical input -> identical BVH topology.
//   - StaticBVH owns node/primIdx vectors. Geometry pointers are borrowed (C++17).
//   - Empty BVH has a degenerate root but query code must treat prims.empty()
//     as no candidates, not as an internal node.
//
// PROOF POINTS:
//   - [PR3.5] BuildStaticBVH with 0 prims: root node exists, primCount=0
//   - [PR3.5] BuildStaticBVH with 1 prim: single leaf node
//   - [PR3.5] stable_sort preserves relative order of equal-key primitives
//
// REFERENCES:
//   - docs/agent-context/scenequery-refactor.md
//   - docs/reference/physx/contracts/scenequery-pipeline.md
//   - docs/reference/physx/contracts/mesh-sweeps-ordering.md
//   - Ericson, RTCD Chapter 6 (BVH construction)
// =========================================================================

#include "SqTypes.h"
#include <algorithm>

namespace Engine { namespace Collision { namespace sq {

// ---- BVH node -----------------------------------------------------------

struct BVHNode {
    AABB     bounds;
    uint32_t left = 0, right = 0;
    uint32_t primStart = 0, primCount = 0;  // leaf if primCount > 0
};

// ---- Static BVH ---------------------------------------------------------

struct StaticBVH {
    std::vector<BVHNode>   nodes;
    std::vector<uint32_t>  primIdx;   // reordered primitive indices
    std::vector<PrimRef>   prims;     // flattened primitive refs
    uint32_t               root = 0;

    // Borrowed geometry pointers (must outlive all queries against this BVH)
    const AABB*     aabbs     = nullptr;  uint32_t aabbCount = 0;
    const OBB*      obbs      = nullptr;  uint32_t obbCount  = 0;
    const Triangle* tris      = nullptr;  uint32_t triCount  = 0;
};

inline bool IsEmptyBVH(const StaticBVH& bvh)
{
    return bvh.nodes.empty() || bvh.prims.empty();
}

// ---- Build parameters ---------------------------------------------------

struct BuildCtx {
    uint32_t leafSize    = 4;      // max primitives per leaf
    float    centroidEps = 1e-6f;  // degenerate centroid bbox threshold
};

// ---- Internal build helpers ---------------------------------------------

inline int ChooseAxis(const AABB& cb) {
    float ex = cb.maxX - cb.minX;
    float ey = cb.maxY - cb.minY;
    float ez = cb.maxZ - cb.minZ;
    if (ex >= ey && ex >= ez) return 0;
    if (ey >= ez) return 1;
    return 2;
}

inline bool DegenerateCentroids(const AABB& cb, float eps) {
    return (cb.maxX - cb.minX) <= eps
        && (cb.maxY - cb.minY) <= eps
        && (cb.maxZ - cb.minZ) <= eps;
}

struct BuildResult { uint32_t node; AABB bounds; };

// Recursive build: partition primitives and construct nodes bottom-up.
// DETERMINISM: stable_sort on (centroid[axis], type, index).
inline BuildResult BuildRange(StaticBVH& bvh, uint32_t start, uint32_t count,
                               const BuildCtx& ctx)
{
    const float INF = std::numeric_limits<float>::infinity();
    AABB bounds{+INF,+INF,+INF, -INF,-INF,-INF};
    AABB cb = bounds;

    for (uint32_t i = 0; i < count; ++i) {
        const PrimRef& p = bvh.prims[bvh.primIdx[start + i]];
        bounds = UnionAABB(bounds, p.bounds);
        cb.minX = (std::min)(cb.minX, p.centroid.x);
        cb.minY = (std::min)(cb.minY, p.centroid.y);
        cb.minZ = (std::min)(cb.minZ, p.centroid.z);
        cb.maxX = (std::max)(cb.maxX, p.centroid.x);
        cb.maxY = (std::max)(cb.maxY, p.centroid.y);
        cb.maxZ = (std::max)(cb.maxZ, p.centroid.z);
    }

    if (count <= ctx.leafSize || DegenerateCentroids(cb, ctx.centroidEps)) {
        uint32_t idx = (uint32_t)bvh.nodes.size();
        BVHNode n{}; n.bounds = bounds; n.primStart = start; n.primCount = count;
        bvh.nodes.push_back(n);
        return {idx, bounds};
    }

    int axis = ChooseAxis(cb);
    auto first = bvh.primIdx.begin() + start;
    auto last  = first + count;

    // SSOT: Deterministic ordering — stable_sort on (centroid, type, index)
    std::stable_sort(first, last, [&](uint32_t ia, uint32_t ib) {
        const PrimRef& A = bvh.prims[ia];
        const PrimRef& B = bvh.prims[ib];
        float a = (axis==0) ? A.centroid.x : (axis==1) ? A.centroid.y : A.centroid.z;
        float b = (axis==0) ? B.centroid.x : (axis==1) ? B.centroid.y : B.centroid.z;
        if (a < b) return true;
        if (a > b) return false;
        if ((uint8_t)A.type != (uint8_t)B.type) return (uint8_t)A.type < (uint8_t)B.type;
        return A.index < B.index;
    });

    uint32_t mid = start + count / 2;

    BuildResult L = BuildRange(bvh, start, mid - start, ctx);
    BuildResult R = BuildRange(bvh, mid, (start + count) - mid, ctx);

    uint32_t idx = (uint32_t)bvh.nodes.size();
    BVHNode n{};
    n.bounds = UnionAABB(L.bounds, R.bounds);
    n.left = L.node;
    n.right = R.node;
    bvh.nodes.push_back(n);
    return {idx, bvh.nodes[idx].bounds};
}

// ---- Public API: build a static BVH from geometry arrays (C++17) --------

inline StaticBVH BuildStaticBVH(const AABB* aabbs, uint32_t aabbCount,
                                 const OBB*  obbs,  uint32_t obbCount,
                                 const Triangle* tris, uint32_t triCount,
                                 const BuildCtx& ctx = {})
{
    StaticBVH bvh;
    bvh.aabbs     = aabbs;  bvh.aabbCount = aabbCount;
    bvh.obbs      = obbs;   bvh.obbCount  = obbCount;
    bvh.tris      = tris;   bvh.triCount  = triCount;

    bvh.prims.reserve(aabbCount + obbCount + triCount);

    for (uint32_t i = 0; i < aabbCount; ++i) {
        PrimRef p{}; p.type = PrimType::Aabb; p.index = i;
        p.bounds = aabbs[i]; p.centroid = AABBCenter(aabbs[i]);
        bvh.prims.push_back(p);
    }
    for (uint32_t i = 0; i < obbCount; ++i) {
        AABB b = OBBWorldAABB(obbs[i]);
        PrimRef p{}; p.type = PrimType::Obb; p.index = i;
        p.bounds = b; p.centroid = AABBCenter(b);
        bvh.prims.push_back(p);
    }
    for (uint32_t i = 0; i < triCount; ++i) {
        AABB b = TriAABB(tris[i]);
        PrimRef p{}; p.type = PrimType::Tri; p.index = i;
        p.bounds = b; p.centroid = AABBCenter(b);
        bvh.prims.push_back(p);
    }

    bvh.primIdx.resize(bvh.prims.size());
    for (uint32_t i = 0; i < (uint32_t)bvh.primIdx.size(); ++i) bvh.primIdx[i] = i;

    bvh.nodes.reserve(bvh.prims.size() * 2);

    if (bvh.prims.empty()) {
        // Empty BVH: single degenerate root
        BVHNode n{};
        bvh.nodes.push_back(n);
        bvh.root = 0;
    } else {
        BuildResult r = BuildRange(bvh, 0, (uint32_t)bvh.prims.size(), ctx);
        bvh.root = r.node;
    }

    return bvh;
}

}}} // namespace Engine::Collision::sq
