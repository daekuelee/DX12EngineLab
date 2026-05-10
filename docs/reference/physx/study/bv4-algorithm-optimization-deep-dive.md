# PhysX Study Map: BV4 Algorithm And Optimization Deep Dive

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx`
- Related contract cards:
  - `docs/reference/physx/contracts/bv4-layout-traversal.md`
  - `docs/reference/physx/contracts/mesh-sweeps-ordering.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

This study map records the PhysX BV4 design at the level needed to improve a
DX12EngineLab BVH4 implementation without copying PhysX code.

The important conclusion is that PhysX BV4 is not just "a 4-child BVH". It is a
mesh midphase package made of:

- a build-time repack from a binary AABB tree into four-slot BV4 nodes;
- an encoded flattened stream that stores leaf ranges or child offsets;
- query-specific parameter structs and leaf callback functors;
- ordered vs no-order traversal chosen by closest-hit vs any-hit semantics;
- SIMD-shaped child AABB rejection around quantized or swizzled node storage.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/`
- `.ref/PhysX_4.0/physx/source/geomutils/src/`

No EngineLab production source was inspected for this study map.

## Located Raw Files

| File | Why it matters |
|---|---|
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h` | packed node fields, child data encoding, `BV4Tree` owner |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h` | swizzled four-child storage, primitive count decode, quantized coeff setup |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp` | binary-to-BV4 grouping, leaf packing, epsilon bounds, quantized flattening |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h` | process stream wrappers, stack traversal, PNS ordering |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h` | SIMD-shaped AABB sweep rejection |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs.h` | swizzled slab traversal support and four-lane min/max setup |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs_KajiyaOrdered.h` | four-child SIMD slab traversal and ordered pruning |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h` | ordered node dispatch: child AABB accepted => leaf callback or push child |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamNoOrder_SegmentAABB.h` | any-hit/no-order early-exit traversal |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_BoxSweep_Params.h` | sweep precomputation and query param ownership |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep.cpp` | capsule sweep dispatch through ordered/no-order streams |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweepAA.cpp` | axis-aligned capsule sweep path with segment-AABB rejection |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h` | leaf exact triangle tests and closest-hit state update |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h` | RTREE/BV4 dispatch table and platform/SIMD gating |
| `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp` | public mesh sweep wrapper, identity vs scaled path, MTD handoff |

## Reading Order

1. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:219`
   - Start here to see that BV4 is a selectable mesh midphase backend, not a
     general movement policy layer.
2. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:211`
   - Read the encoded `mData` contract for leaf vs child state.
3. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:529`
   - Read leaf primitive packing and epsilon inflation.
4. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:572`
   - Read binary P/N tree repacking into PP/PN/NP/NN four-slot nodes.
5. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:803`
   - Read flattening and quantization containment correction.
6. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:36`
   - Read the process-stream layering comment.
7. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweepAA.cpp:90`
   - Read query dispatch: no-order for any-hit, ordered for closest-hit.
8. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:35`
   - Read the SIMD-shaped AABB sweep rejection.
9. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h:242`
   - Read leaf callback exact triangle testing and result tightening.

## Semantic Model

### 1. BV4 is midphase candidate acceleration

`GuMidphaseInterface.h` stores separate tables for raycast, overlap, and sweep
queries, and the BV4 entries are selected only for BVH34 meshes on Intel/SIMD
enabled builds. The capsule sweep table chooses `sweepCapsule_MeshGeom_BV4`
when `PX_INTEL_FAMILY && !defined(PX_SIMD_DISABLED)` and otherwise routes BV4
requests to unsupported functions. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:181`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:279`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:377`.

This means BV4's job is to enumerate plausible mesh triangles quickly. It does
not own character walkability, stair logic, floor support, or recovery policy.

### 2. Node traversal is not the hit policy

The BV4 internal header states the root process-stream function traverses nodes,
process-node functions handle each traversed node, and leaf functors are called
when a leaf is found. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:36`.

The ordered segment-AABB process node tests a child bound, then either calls
`LeafTestT::doLeafTest` for leaves or pushes child data onto the stack for
internal nodes. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h:35`.

The no-order version returns early when a leaf functor reports success, which
matches any-hit semantics rather than closest-hit semantics. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamNoOrder_SegmentAABB.h:35`.

### 3. Closest sweep uses ordered traversal plus mutable best distance

For capsule sweep, PhysX picks no-order traversal when `mEarlyExit` is true and
ordered traversal otherwise. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep.cpp:76`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep.cpp:83`.

The axis-aligned capsule sweep path follows the same split using
`processStreamRayNoOrder` for any-hit and `processStreamRayOrdered` for closest.
Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweepAA.cpp:90`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweepAA.cpp:97`.

The leaf callback runs exact capsule-triangle sweep and then updates
`mStabbedFace.mDistance`, triangle id, best distance, best alignment, and best
triangle normal only if `keepTriangle` accepts the candidate. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h:242`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h:260`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h:263`.

When a closer hit is accepted, node sorting can shrink the traversal bound by
calling `setupRayData` or `ShrinkOBB`. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h:276`.

## Build Algorithm

### 1. Leaf packing

PhysX packs leaf primitive ranges into `mData`: leaf status is the low bit and
the primitive payload is shifted. The source leaf primitive count is asserted to
fit below 16, and the offset/count is packed before the leaf bit is added.
Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:529`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:221`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:240`.

### 2. Conservative build-time inflation

When build `epsilon` is non-zero, PhysX expands child extents for both leaves
and internal child bounds. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:542`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:557`.

This is a conservative inclusion rule. It must not be confused with exact
penetration or KCC skin/recovery semantics.

### 3. Binary P/N tree repack into four slots

The builder starts from a binary AABB tree. It groups children using the binary
tree's positive/negative order and, when possible, stores grandchildren as
`PP`, `PN`, `NP`, `NN` in a four-slot node. The comments explicitly preserve
the original order to reuse PNS sorting bits. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:572`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:587`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:728`.

This is not the same as naively splitting primitives into four equal partitions.
The 4-way layout is tied to preserving directional child-order metadata from a
binary source tree.

### 4. Flattened stream and child offsets

Flattening writes each slot's encoded data into a contiguous stream and stores
child offsets rather than pointers. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:903`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:911`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:956`.

`BVDataPackedT` exposes `getChildOffset`, `getChildType`, and `getChildData`,
all derived from the packed `mData`. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:211`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:221`.

### 5. Quantization is corrected to remain conservative

The quantized flatten path adjusts quantized extents/centers until the
dequantized box still contains the original bound. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:840`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:847`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:875`.

This matters because quantization must create false positives rather than false
negatives. A BVH4 implementation should treat quantization as a storage/layout
optimization only after scalar containment tests exist.

## Traversal Algorithm

### Ordered traversal

For non-slab ordered traversal, PhysX computes a direction mask from the local
direction sign bits and uses PNS ordering to choose child processing order.
Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:113`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:223`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:230`.

The alternative ordered path first computes a bitmask of accepted child bounds,
then uses branch-expanded PNS blocks to push accepted children in direction-aware
order. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:240`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:262`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:270`.

### No-order traversal

No-order traversal walks the stack without PNS ordering and allows early return
from `LeafTestT::doLeafTest`. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:184`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamNoOrder_SegmentAABB.h:43`.

This is correct for any-hit queries but not a replacement for closest-hit
queries.

## SIMD And Optimization Model

### 1. SIMD is gated and backend-specific

BV4 mesh midphase functions are only selected in the midphase tables when the
build is Intel-family and SIMD is enabled. Otherwise the BV4 table entries
report unsupported operation. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:181`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:205`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:279`.

### 2. Non-slab AABB sweep rejection uses vector compares and movemask

The segment-AABB sweep rejection loads direction/data/extents into vector
registers, compares absolute deltas against expanded extents, uses movemask to
reject on XYZ lanes, then performs a second vectorized cross-axis rejection.
Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:35`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:41`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:49`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:60`.

The quantized overload first dequantizes center/extents from packed integer data
into vectors, then runs the same rejection shape. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:69`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:73`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:79`.

### 3. Slab traversal uses swizzled four-child layout

When `GU_BV4_USE_SLABS` is enabled, `BVDataSwizzledQ` stores per-axis arrays for
four children plus four `mData` entries. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:191`.

The slabs helper says the ray traversal is based on Kay/Kajiya slabs and uses
SIMD to do four ray-vs-AABB tests at a time. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs.h:38`.

The slab initialization computes reciprocal direction vectors and splats per
axis values for vectorized four-child tests. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs.h:109`.

The ordered slab path loads four child min/max X/Y/Z lanes, runs the slab test,
uses a movemask-style code to skip rejected children, then calls leaf tests or
queues child nodes. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs_KajiyaOrdered.h:88`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs_KajiyaOrdered.h:96`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs_KajiyaOrdered.h:122`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs_KajiyaOrdered.h:129`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs_KajiyaOrdered.h:196`.

### 4. Performance is a stack of small choices, not one trick

PhysX combines compact node data, optional quantized bounds, four-child grouping
from a binary source tree, directional PNS ordering for closest queries,
no-order early exit for any-hit queries, mutable best distance that tightens
later traversal, aligned parameter blocks, query-specific leaf functors, and
separate identity-scale/scaled mesh paths. Evidence:
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:215`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:572`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:223`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h:272`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweepAA.cpp:45`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:772`,
`.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp:826`.

## Extensibility Model

| Extension point | PhysX mechanism | Evidence | Lesson for EngineLab |
|---|---|---|---|
| Midphase backend selection | Function tables choose RTREE or BV4 by mesh concrete type and platform/SIMD support. | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:219`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:279` | Keep backend selection explicit; do not silently replace semantics. |
| Query behavior | `mEarlyExit` chooses no-order any-hit vs ordered closest. | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep.cpp:83` | Backend traversal must know collector mode, not KCC policy. |
| Exact primitive behavior | Leaf functors call query-specific exact tests. | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h:43` | Node code should only dispatch candidates; exact tests stay shared. |
| Result tightening | Leaf tests mutate best distance/alignment and shrink traversal bounds. | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h:263`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h:276` | Closest collector needs a mutable max distance that traversal can read. |
| Quantization | Flattening adjusts quantized bounds to stay conservative. | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp:875` | Quantization must be protected by containment tests before performance claims. |
| SIMD layout | Swizzled Q/NQ node layouts store four children by axis. | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:191`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h:214` | Introduce SoA node storage only after scalar result equivalence is locked. |
| Fallback | If `tree.mNodes` is absent, capsule sweep falls back to brute-force triangle tests. | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep.cpp:83` | Keep a linear oracle/fallback for correctness and debugging. |

## What To Import Into DX12EngineLab

Import as design contracts:

- BVH4 traversal is candidate acceleration, not movement policy.
- Scalar equivalence must come before SIMD.
- A backend-independent collector should own closest/any/all/top-K semantics.
- Ordered traversal should use a mutable max distance from the collector.
- Any-hit and closest-hit are different traversal modes.
- Future SIMD should target child AABB rejection first.
- Quantization/swizzled layout should wait until conservative containment probes
  exist.

Do not import yet:

- PhysX packed `mData` bit layout as-is.
- PhysX quantized bounds as the first EngineLab BVH4 storage format.
- PNS branch macros as code structure.
- PhysX CCT/KCC behavior into BVH traversal.
- SIMD before benchmark/probe coverage can prove correctness and usefulness.

## Proposed EngineLab BVH4 Improvement Sequence

1. Shared collector boundary:
   - Extract closest sweep and overlap top-K retention into backend-independent
     helpers.
   - Goal: `BinaryBVH`, `ScalarBVH4`, and future `SimdBVH4` cannot diverge in
     hit/contact semantics.
2. Scalar traversal cleanup:
   - Keep current scalar BVH4 as a correctness backend.
   - Add explicit collector modes: closest, any, all/top-K.
   - Keep `LinearFallback` as oracle.
3. Deterministic benchmark expansion:
   - Add fixed-seed query batches that cover miss-heavy, hit-heavy, equal-depth,
     and overflow-risk cases.
   - Track node visits, child AABB tests, primitive AABB tests, narrowphase
     calls, stack max, fallback, mismatches, and ns/query.
4. SIMD-ready layout experiment:
   - Add a separate SoA child-bound cache or node representation.
   - Do not delete scalar node path.
   - First SIMD target: four child AABB rejection.
5. Optional quantization:
   - Add only after containment tests prove dequantized bounds conservatively
     contain source bounds.

## Terms To Remember

- `BV4`: PhysX four-slot mesh midphase tree, not a generic KCC policy object.
- `PNS`: positive/negative subtree order bits used to pick direction-aware child
  traversal order.
- `processStream`: traversal driver that pops encoded child data and dispatches
  node processing.
- `LeafTestT`: query-specific exact primitive callback.
- `mEarlyExit`: any-hit mode selector that allows no-order early return.
- `mStabbedFace.mDistance`: mutable best distance used by leaf tests and
  traversal pruning.
- `swizzled`: four-child SoA layout used by SIMD slab traversal.
- `quantized`: compressed bound storage that must remain conservative.

## EngineLab Comparison Questions

- Does EngineLab have one backend-independent collector for closest/any/all/top-K
  semantics, or do backends duplicate retention policy?
- Can `ScalarBVH4` read the current collector max distance to prune later child
  bounds?
- Are any-hit and closest-hit query modes separated before traversal starts?
- Does the benchmark distinguish candidate reduction from actual query time?
- Is the planned SIMD work limited to child AABB rejection, or is it trying to
  SIMD exact primitive narrowphase too early?
- Is there a deterministic test that proves overlap top-K is traversal-order
  independent?
- Is future quantization protected by a containment probe?

## Next Reading Path

- `docs/reference/physx/contracts/bv4-layout-traversal.md`
- `docs/reference/physx/contracts/mesh-sweeps-ordering.md`
- `docs/reference/physx/study/mesh-sweeps-ordering-study-map.md`
- `docs/audits/scenequery/09-scalar-bvh4-core.md`
