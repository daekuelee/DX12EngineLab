# PhysX BV4 Layout And Traversal Study Map

status: draft
authority: study-map-only
primary_contract: `docs/reference/physx/contracts/bv4-layout-traversal.md`

## Purpose

This map records where PhysX 4.0 BV4 behavior is implemented so future DX12EngineLab BVH4 work does not restart from broad source searching. Use it to navigate raw source. Use the contract card as the reviewed behavior summary.

## Source Roots Checked

- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/`
- `.ref/PhysX_4.0/physx/source/geomutils/src/`

No production EngineLab source was inspected for this study map.

## Reading Order

1. `GuBV4.h`
   - Start at `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:196` for packed quantized bounds.
   - Continue at `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:215` for `BVDataPackedT`.
   - Continue at `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:241` for `BV4Tree` ownership fields.
2. `GuBV4_Common.h`
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:186` for swizzled four-child storage.
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:240` for packed leaf primitive count decode.
3. `GuBV4Build.cpp`
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:529` for leaf primitive packing.
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:572` for binary-to-BV4 PNS packing.
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:803` and `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:903` for flattening and child offset encoding.
4. `GuBV4_Internal.h`
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:36` for the high-level traversal layering comment.
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:57` for stream dispatch.
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:220` for ordered child traversal and PNS use.
5. `GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h`
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h:33` for the node overlap vs leaf callback split.
6. `GuBV4_AABBAABBSweepTest.h`
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:35` for SIMD-shaped AABB rejection.
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:69` for the quantized variant.
7. `GuMidphaseInterface.h`
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:269` for compile/platform dispatch.
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:377` for mesh sweep dispatch.
8. `GuMidphaseBV4.cpp`
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:750` for capsule sweep leaf callback handoff.
   - Read `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:759` for capsule mesh sweep finalization and MTD handling.

## Runtime Flow

```text
Midphase sweep entry
  -> backend table selects BV4 path when supported
  -> BV4 traversal stream dispatch
  -> child AABB overlap rejection
  -> leaf primitive callback
  -> exact primitive sweep / hit processing
  -> public PxSweepHit finalization
```

## Build Flow

```text
source binary tree
  -> BV4 temporary node groups with P/N and PP/PN/NP/NN ordering
  -> packed slot data with leaf ranges or child offsets
  -> flattened node stream stored in BV4Tree::mNodes
```

## Key Types And Roles

| Type / Function | Role | Evidence |
|---|---|---|
| `QuantizedAABB` | compact quantized center/extents storage | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:196` |
| `BVDataPackedT` | per-child packed bound plus leaf/child encoded data | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:215` |
| `BVDataSwizzledQ` | four-child SoA-style swizzled storage | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:186` |
| `BV4Tree` | runtime tree owner and quantization coefficients | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:241` |
| `_BuildBV4` | binary tree to BV4 grouping | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:572` |
| `_FlattenQ` | flattened stream emission for quantized BV4 | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:803` |
| `processStreamOrdered` | ordered traversal stream dispatch | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:57` |
| `BV4_ProcessNodeOrdered` | child bound rejection and leaf/child dispatch | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h:33` |
| `BV4_SegmentAABBOverlap` | vectorized child AABB rejection | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:35` |
| `sweepCapsule_MeshGeom_BV4` | capsule-vs-mesh BV4 sweep wrapper | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:759` |

## Concepts To Preserve In EngineLab Work

- Four-child node layout and SIMD traversal are performance structure, not collision policy.
- Exact primitive hit acceptance should remain callback/collector-owned.
- Initial overlap and MTD are query-result semantics, not BVH child layout semantics.
- BVH traversal order can be deterministic and optimized without pretending traversal order is the final hit policy.
- If EngineLab adds BVH4, first make scalar BVH4 behavior match the existing BVH contract; add SIMD after metrics prove where traversal time is spent.

## Deep-Dive Companion

- `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md`
  expands this map with build algorithm details, ordered/no-order traversal
  semantics, SIMD/slab notes, and EngineLab BVH4 improvement sequencing.

## Open Follow-Up Questions

- Does EngineLab want BVH4 as a replacement for the current BVH, or as a second backend for comparison?
- Should EngineLab use packed AoS nodes first, then introduce SoA/SIMD layout later?
- Which metrics will define success: candidate count, primitive test count, traversal node visits, branch count, query time, or all of them?
- What public collector contracts are needed before BVH4 can safely support closest/any/all/stage-filtered KCC queries?
