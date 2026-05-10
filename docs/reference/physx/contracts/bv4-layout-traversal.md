# PhysX BV4 Layout And Traversal Contract

trust_level: raw-source-backed
review_status: draft
source_engine: PhysX 4.0
scope: BV4 node layout, binary-tree-to-BV4 packing, flattened traversal stream, SIMD AABB rejection, mesh sweep dispatch

## Evidence

- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:196`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:211`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:215`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:241`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:529`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:548`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:572`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:728`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:803`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:867`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:903`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:956`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:186`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:240`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:36`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:57`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:220`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h:33`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:35`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:69`
- `.ref/PhysX_4.0/physx/source/geomutils/src/GuAABBTreeQuery.h:127`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:269`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:377`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:750`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:759`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:843`

## Contract Summary

PhysX BV4 is a midphase acceleration structure that repacks a binary source tree into a flattened four-slot node stream. Each slot stores a child bound plus encoded data that is either a leaf primitive range or a child node offset/type. Traversal is split into three layers:

1. stream dispatch chooses quantized/non-quantized and ordered/unordered processing,
2. node processing rejects child bounds with a vectorized AABB test,
3. leaf callbacks run exact primitive tests and update the query result.

The important contract is not "BV4 returns a final collision answer by itself." BV4 is a candidate and ordering accelerator. Exact triangle/capsule/box/convex semantics live in leaf callbacks and midphase hit finalization.

## Inputs

- Source mesh bounds and primitive index ranges used by the BV4 builder.
- Query-time `BV4Tree`, local query data, leaf test functor, and mutable query distance/result state.
- Sweep-specific expansion data such as AABB extents/inflation before child bound rejection.

## Outputs

- Build-time output: a flattened BV4 node stream stored through `BV4Tree::mNodes`.
- Traversal-time output: leaf callback invocations for potentially intersecting primitive ranges.
- Mesh sweep output: callback-owned `SweepHit` / `PxSweepHit` result after exact primitive tests and finalization.

## Invariants

- A packed BV4 slot encodes leaf/child state in `mData`; leaf primitive ranges are compacted into the low bits and child entries encode offset plus child type. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:215`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:529`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:903`.
- Leaf primitive counts are constrained by the compact encoding. The builder asserts the primitive count is below the packed limit, and the decode path extracts the count from the encoded primitive index. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:529`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:240`.
- BV4 construction keeps a P/N ordering model from the binary tree and repacks grandchildren as PP/PN/NP/NN when both binary children are internal. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:572`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:728`.
- Flattening writes encoded child data and then recursively emits child nodes. The child offset in the slot points into the flattened stream, not back to the original binary tree. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:903`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:956`.
- Ordered traversal uses direction-derived ordering plus the encoded PNS information; the first visited child is an ordering optimization, not a semantic guarantee that the first leaf is the final closest hit. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:220`.
- Node rejection is an AABB-level test. It must not be treated as exact primitive collision. Exact hit semantics are deferred to `doLeafTest` and sweep callbacks. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:36`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h:33`.
- The BV4 AABB sweep rejection path is SIMD-shaped: it evaluates multiple child boxes with vector comparisons and mask extraction. Quantized variants dequantize child bounds before the same rejection pattern. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:35`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:69`.
- BV4 midphase sweep dispatch is selected by mesh concrete type and platform/SIMD availability. It is not a KCC movement policy layer. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:269`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:377`.

## Edge Cases

- Builder-time epsilon can inflate child bounds during packing. This improves conservative inclusion but also means BV4 broadphase acceptance is intentionally not exact primitive contact. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:548`.
- Quantized flattening corrects quantized extents so the dequantized bound still contains the original source bound. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:867`.
- Mesh sweep wrappers have explicit initial-overlap handling. For capsule-vs-mesh BV4 sweep, a zero-distance hit can be converted through the MTD path when the query asks for it. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:759`.
- Identity-scale and non-identity-scale mesh sweep paths are not equivalent internally. Capsule BV4 sweep has a direct identity path and a callback/finalization path for scaled cases. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:759`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:843`.

## Filtering And Ordering

- BV4 child traversal order can reduce work and improve early pruning, but correctness still depends on leaf exact tests and callback result update.
- Binary AABB tree query code demonstrates the same separation: leaf tests can shrink the accepted maximum distance through the callback, and traversal then uses the reduced distance for later pruning. Evidence: `.ref/PhysX_4.0/physx/source/geomutils/src/GuAABBTreeQuery.h:127`.
- BV4 should therefore be modeled as "candidate acceleration plus deterministic traversal policy", not as the owner of walkable, stair, support, or KCC-specific filtering.

## Application To DX12EngineLab

When implementing or auditing an EngineLab BVH4/BV4-like path, keep these boundaries:

- `SceneQuery`/BVH traversal should own candidate enumeration, child ordering, conservative AABB rejection, and exact primitive callback invocation.
- Closest/any/all result collection should live above raw child rejection, not inside hard-coded node traversal branches.
- KCC concepts such as walkable, floor, perch, step-up, step-down, and recovery must not be encoded into BVH node layout or BVH child ordering.
- SIMD should be introduced around SoA child bound tests only after the scalar contract is identical and measurable.

## Audit Questions

- Does the EngineLab BVH path distinguish "AABB candidate accepted" from "primitive hit accepted"?
- Does the traversal return enough information for a collector/filter layer to implement closest, any, all, or stage-specific hit policy?
- Does the node layout make leaf range limits explicit and testable?
- Is child ordering deterministic for equal or near-equal distances?
- Are inflated/skin bounds separated from exact geometry recovery semantics?
