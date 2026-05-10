# BVH4 PhysX Gap Roadmap

Updated: 2026-05-11

## 0. One-Line Verdict

Current `ScalarBVH4` is a useful scalar four-child candidate backend, but it is
not a PhysX BV4 clone: PhysX BV4 is a mesh-midphase system built around binary
tree repacking, encoded flattened streams, ordered/no-order traversal modes,
query-owned leaf callbacks, and SIMD-shaped child AABB rejection.

The next correct direction is not "add SIMD now." The next direction is:

1. make collector/result policy backend-independent,
2. expand deterministic metrics,
3. clean scalar traversal modes,
4. then prototype SIMD child AABB rejection,
5. only later experiment with flattening and quantization.

## 1. Evidence Base

| Source | Role |
|---|---|
| `Engine/Collision/SceneQuery/SqBVH4.h:21` | current `BVH4BuildCtx` and scalar BVH4 build knobs |
| `Engine/Collision/SceneQuery/SqBVH4.h:26` | current `BVH4Slot` stores direct bounds/index/count/flags |
| `Engine/Collision/SceneQuery/SqBVH4.h:39` | current `StaticBVH4` stores `sourceView`, `primIdx`, recursive `nodes` |
| `Engine/Collision/SceneQuery/SqBVH4.h:86` | current build uses stable sort over centroid/type/index |
| `Engine/Collision/SceneQuery/SqBVH4.h:132` | current build partitions into up to four equal ranges |
| `Engine/Collision/SceneQuery/SqBVH4.h:176` | current BVH4 overlap has a private top-K helper |
| `Engine/Collision/SceneQuery/SqBVH4.h:204` | current BVH4 sweep leaf path reuses `ConsiderSweepCapsulePrim` |
| `Engine/Collision/SceneQuery/SqBVH4.h:272` | current `BuildStaticBVH4` builds from existing `StaticBVH` |
| `Engine/Collision/SceneQuery/SqBVH4.h:294` | current `SweepCapsuleClosestHit_BVH4` is closest-only |
| `Engine/Collision/SceneQuery/SqBVH4.h:385` | current `OverlapCapsuleContacts_BVH4` is top-K overlap-only |
| `Engine/Collision/SceneQuery/SqQueryLegacy.h:99` | current shared `BetterHit` closest-hit tie-break |
| `Engine/Collision/SceneQuery/SqQueryLegacy.h:141` | current shared per-primitive sweep dispatcher |
| `Engine/Collision/SceneQuery/SqQueryLegacy.h:407` | current shared `OverlapContactBetter` ordering |
| `Engine/Collision/SceneQuery/SqBackendHarness.cpp:138` | harness dispatches sweep across `LinearFallback`, `BinaryBVH`, `ScalarBVH4` |
| `Engine/Collision/SceneQuery/SqBackendHarness.cpp:169` | harness dispatches overlap across the same backend set |
| `docs/audits/scenequery/09-scalar-bvh4-core.md:55` | current local harness output and limitations |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:15` | PhysX BV4 is more than a generic 4-child BVH |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:75` | PhysX BV4 is mesh-midphase candidate acceleration |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:89` | PhysX node traversal is not hit policy |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:105` | PhysX closest sweep uses ordered traversal plus mutable best distance |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:151` | PhysX repacks binary P/N tree into four slots |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:165` | PhysX uses flattened streams and child offsets |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:218` | PhysX SIMD/optimization model |
| `docs/reference/physx/contracts/bv4-layout-traversal.md:30` | contract summary: BV4 is traversal/layout performance structure |

## 2. Current EngineLab BVH4 Snapshot

| Area | Current behavior | Evidence | Immediate implication |
|---|---|---|---|
| Ownership | `SqBVH4.h` states BVH4 must not encode KCC policy. | `Engine/Collision/SceneQuery/SqBVH4.h:3` | Correct boundary. Keep KCC policy out. |
| Build input | `BuildStaticBVH4` consumes an existing `StaticBVH` and preserves it as `sourceView`. | `Engine/Collision/SceneQuery/SqBVH4.h:272` | Good fallback/debug path, but memory duplicates binary data. |
| Build split | Sorts by centroid/type/index, then divides into up to four equal ranges. | `Engine/Collision/SceneQuery/SqBVH4.h:86`, `Engine/Collision/SceneQuery/SqBVH4.h:132` | Deterministic, but not PhysX P/N repack and not SAH. |
| Node layout | `BVH4Node` owns four AoS `BVH4Slot`s with direct flags and indices. | `Engine/Collision/SceneQuery/SqBVH4.h:26`, `Engine/Collision/SceneQuery/SqBVH4.h:34` | Readable scalar layout, not packed/flattened/SIMD-friendly yet. |
| Sweep traversal | `SweepCapsuleClosestHit_BVH4` tests four child AABBs, sorts by `tEnter`, then visits leaves and pushes children. | `Engine/Collision/SceneQuery/SqBVH4.h:294`, `Engine/Collision/SceneQuery/SqBVH4.h:332`, `Engine/Collision/SceneQuery/SqBVH4.h:355` | Closest-only traversal exists, but no explicit collector mode. |
| Sweep leaf | Leaf path calls shared `ConsiderSweepCapsulePrim`. | `Engine/Collision/SceneQuery/SqBVH4.h:204` | Good: exact primitive semantics are mostly shared. |
| Overlap traversal | Overlap walks children in slot order and uses BVH4-private top-K helper. | `Engine/Collision/SceneQuery/SqBVH4.h:385`, `Engine/Collision/SceneQuery/SqBVH4.h:176` | Deterministic now, but retention policy is duplicated. |
| Metrics | Backend is tracked through `QueryBackend::BVH4`. | `Engine/Collision/SceneQuery/SqMetrics.h:21` | Enough for first comparison, not enough for full performance story. |
| Harness | Harness compares `LinearFallback`, `BinaryBVH`, and `ScalarBVH4`. | `Engine/Collision/SceneQuery/SqBackendHarness.cpp:138`, `Engine/Collision/SceneQuery/SqBackendHarness.cpp:169` | Good base for staged changes. |

## 3. PhysX BV4 Structural Model

PhysX BV4 is a mesh-midphase backend selected through mesh concrete type and
platform/SIMD gates, not a universal scene query policy object. Evidence:
`docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:75`.

The PhysX design layers are:

```text
mesh midphase dispatch
  -> binary AABB tree repack into four-slot BV4 groups
  -> encoded flattened node stream
  -> ordered/no-order process stream
  -> SIMD child AABB rejection
  -> query-specific leaf functor
  -> closest/any/MTD result update outside node layout
```

Key PhysX contracts to preserve conceptually:

- BV4 traversal is candidate acceleration, not KCC or gameplay policy.
- Ordered and no-order traversal correspond to closest-hit and any-hit query
  modes.
- Exact primitive hit generation lives in leaf callbacks.
- Mutable best distance tightens later traversal.
- SIMD is concentrated around child AABB rejection.
- Quantization is conservative storage and needs containment proof.

## 4. High-Density Comparison Matrix

| Aspect | EngineLab current | PhysX BV4 | Gap | Risk | Next action |
|---|---|---|---|---|---|
| Backend ownership / midphase boundary | `ScalarBVH4` is a SceneQuery backend prototype, not runtime-selected midphase. Evidence: `SqBVH4.h:3`, `SqBackendHarness.cpp:138`. | BV4 is selected from mesh midphase tables. Evidence: `bv4-algorithm-optimization-deep-dive.md:75`. | EngineLab has backend comparison, not mesh-midphase abstraction. | Low now; Medium when tri-mesh arrives. | Keep `ScalarBVH4` harness-only until backend selection contract exists. |
| Build input model | Builds from existing `StaticBVH`. Evidence: `SqBVH4.h:272`. | Builds/repackages mesh AABB tree data into BV4 tree. Evidence: `bv4-algorithm-optimization-deep-dive.md:151`. | EngineLab duplicates source binary BVH instead of producing a standalone midphase asset. | Medium memory/design ambiguity. | Keep `sourceView` for oracle, but document future ownership before runtime use. |
| Split/repack algorithm | Stable sort and four equal ranges. Evidence: `SqBVH4.h:86`, `SqBVH4.h:132`. | Preserves binary P/N order and packs PP/PN/NP/NN groups. Evidence: `bv4-algorithm-optimization-deep-dive.md:151`. | Current build is deterministic but not PhysX-style directional repack. | Medium performance and ordering gap. | Defer P/N repack until collector and benchmark are stable. |
| Node memory layout | AoS `BVH4Node { BVH4Slot slots[4] }`. Evidence: `SqBVH4.h:26`, `SqBVH4.h:34`. | Packed/swizzled slots and encoded `mData`. Evidence: `bv4-algorithm-optimization-deep-dive.md:165`, `bv4-algorithm-optimization-deep-dive.md:246`. | Readable scalar layout, not SIMD/cache optimized. | Low correctness, Medium performance. | Add SoA cache later; do not replace scalar layout first. |
| Leaf primitive packing | Leaf stores `index` and `count` directly. Evidence: `SqBVH4.h:26`. | Leaf range encoded in packed data. Evidence: `bv4-algorithm-optimization-deep-dive.md:131`. | EngineLab simpler and safer, less compact. | Low. | Do not pack until flatten experiment proves value. |
| Flattening / child offset encoding | Recursive vector nodes; child references are node indices. Evidence: `SqBVH4.h:39`, `SqBVH4.h:151`. | Flattened stream stores child offsets. Evidence: `bv4-algorithm-optimization-deep-dive.md:165`. | Current layout has pointer-like index graph, not stream traversal. | Medium for cache/SIMD. | Session 5 flattened stream experiment after metrics expansion. |
| Traversal stack model | Uses shared `QueryScratch` stack with `NodeTask`. Evidence: `SqBVH4.h:311`, `SqQueryLegacy.h:45`. | PhysX process stream pops encoded child data. Evidence: `bv4-algorithm-optimization-deep-dive.md:89`. | Different stack payload; EngineLab keeps explicit time windows. | Low correctness. | Keep explicit `tEnter/tExit` until collector abstraction exists. |
| Closest vs any-hit traversal mode | Only closest sweep and overlap top-K are exposed. Evidence: `SqBVH4.h:294`, `SqBVH4.h:385`. | No-order early exit for any-hit; ordered traversal for closest. Evidence: `bv4-algorithm-optimization-deep-dive.md:105`. | EngineLab cannot compare any-hit traversal behavior yet. | Medium extensibility. | Add collector mode design before any-hit implementation. |
| Child ordering | Sorts child hits by computed `tEnter`. Evidence: `SqBVH4.h:169`, `SqBVH4.h:355`. | Uses direction-derived PNS ordering. Evidence: `bv4-algorithm-optimization-deep-dive.md:190`. | EngineLab ordering is query-window based, not build-time directional metadata. | Low correctness, Medium speed. | Keep `tEnter` order for scalar; measure before PNS-like work. |
| Collector/result ownership | Sweep uses shared `ConsiderSweepCapsulePrim`; overlap has BVH4-private top-K. Evidence: `SqBVH4.h:204`, `SqBVH4.h:176`. | Leaf functors own exact test/result update outside node layout. Evidence: `bv4-algorithm-optimization-deep-dive.md:89`, `bv4-algorithm-optimization-deep-dive.md:105`. | Sweep is close; overlap retention diverges. | Medium future divergence. | Session 1: shared collector/retention helpers. |
| Mutable best distance pruning | Traversal clamps `task.tExit` and child `cL` by `best.t`. Evidence: `SqBVH4.h:317`, `SqBVH4.h:337`. | Leaf callbacks update best distance and shrink traversal. Evidence: `bv4-algorithm-optimization-deep-dive.md:125`. | Concept exists, but not formalized as collector state. | Medium API clarity. | Move mutable max distance into explicit collector state. |
| Overlap top-K determinism | BVH4 private helper uses `OverlapContactBetter` on full buffer. Evidence: `SqBVH4.h:176`. | PhysX comparison here is indirect; BV4 traversal should not own final hit policy. Evidence: `bv4-algorithm-optimization-deep-dive.md:89`. | EngineLab fixed BVH4 risk locally, but BinaryBVH helper still differs. | Medium backend divergence. | Session 1 must unify overlap top-K retention. |
| SIMD readiness | Current layout scalar AoS; no SIMD path. Evidence: `SqBVH4.h:26`. | PhysX SIMD targets child AABB rejection and swizzled layout. Evidence: `bv4-algorithm-optimization-deep-dive.md:218`. | Cannot add clean SIMD without layout/collector cleanup. | High if SIMD added now. | Session 4 only after sessions 1-3 pass. |
| Quantization / conservative containment | No quantization. Evidence: `SqBVH4.h:26`. | Quantized bounds are corrected to remain conservative. Evidence: `bv4-algorithm-optimization-deep-dive.md:178`. | No containment tests exist for quantized bounds. | High if attempted early. | Session 6 audit/probe only, no production quantization. |
| Benchmark and metrics quality | Dense-grid 128 query snapshot exists. Evidence: `09-scalar-bvh4-core.md:55`. | PhysX speed comes from many layers, not one trick. Evidence: `bv4-algorithm-optimization-deep-dive.md:269`. | Current benchmark is too narrow for performance claims. | High portfolio overclaim risk. | Session 2 expands fixed-seed benchmark matrix. |

## 5. Roadmap Sessions

### Session 1: Collector Boundary Cleanup

Goal:

- Make `BinaryBVH`, `ScalarBVH4`, and future `SimdBVH4` share closest-hit and
  overlap retention semantics.

Why first:

- Current sweep leaf behavior already reuses `ConsiderSweepCapsulePrim`.
  Evidence: `SqBVH4.h:204`.
- Current overlap top-K has a BVH4-private helper. Evidence: `SqBVH4.h:176`.
- PhysX keeps node traversal and leaf/result policy separate. Evidence:
  `bv4-algorithm-optimization-deep-dive.md:89`.

Implementation target for the later coding session:

- Extract backend-independent overlap top-K helper that uses
  `OverlapContactBetter` for eviction when full.
- Keep `LinearFallback` as oracle.
- Do not add SIMD.
- Do not change KCC-facing behavior.

Acceptance:

- Existing harness still reports `correctness=pass`.
- `overlapTopologyRisk=no`.
- `BinaryBVH` and `ScalarBVH4` call the same retention helper.

### Session 2: Deterministic Benchmark Expansion

Goal:

- Make speed discussion meaningful before any SIMD or flattening work.

Add benchmark fixtures:

- miss-heavy grid;
- hit-heavy grid;
- equal-time sweep candidates;
- equal-depth overlap top-K pressure;
- stack pressure;
- larger deterministic fixed-seed batches.

Metrics to keep:

- `nodeAabbTests`;
- `primitiveAabbTests`;
- `narrowphaseCalls`;
- `nodeTimePrunes`;
- `maxStackDepth`;
- `fallbackCount`;
- `mismatches`;
- `ns/query`.

Acceptance:

- Performance report distinguishes candidate reduction from actual query time.
- No pass/fail threshold on speed.
- All backends compare against `LinearFallback`.

### Session 3: Scalar Traversal Mode Cleanup

Goal:

- Represent traversal mode explicitly: closest ordered, any-hit no-order,
  overlap all/top-K.

Why after Session 1:

- Traversal mode must feed collector behavior. If collector ownership is not
  fixed first, this becomes another one-off backend branch.

Rules:

- Keep scalar layout.
- Keep `tEnter` ordering for closest until metrics show PNS-style ordering is
  worth exploring.
- Any-hit can be added only if harness has any-hit oracle coverage.

Acceptance:

- The API names and metrics make query mode visible.
- No KCC policy enters SceneQuery.

### Session 4: SIMD/SoA Child AABB Rejection Prototype

Goal:

- Prototype SIMD only for four child AABB rejection.

Why not earlier:

- PhysX SIMD benefit is tied to swizzled child-bound layout and query-specific
  params. Evidence: `bv4-algorithm-optimization-deep-dive.md:246`.
- Current EngineLab node layout is AoS. Evidence: `SqBVH4.h:26`.

Rules:

- Keep scalar fallback path.
- Add separate helper or backend, not invasive replacement.
- Do not SIMD exact primitive narrowphase.
- Do not claim speedup until benchmark says so.

Acceptance:

- Correctness equal to scalar.
- Metrics show whether child AABB work decreased or only shifted overhead.

### Session 5: Flattened Node Stream Experiment

Goal:

- Compare recursive vector node layout against flattened stream layout.

Rules:

- Treat flattening as a separate backend/layout experiment.
- Do not combine with quantization in the same session.
- Preserve linear oracle and scalar BVH4 reference path.

Acceptance:

- Benchmark can compare recursive scalar vs flattened scalar.
- Document cache/stack/branch differences with metrics.

### Session 6: Quantization Feasibility Audit

Goal:

- Decide whether quantized BVH4 bounds are worth doing.

Rules:

- Audit/probe first, implementation later.
- Required invariant: dequantized bound must conservatively contain source
  bound.
- No quantized runtime path until containment probe exists.

Acceptance:

- A deterministic containment test exists.
- Quantization is allowed only if false negatives are proven impossible for the
  tested configuration.

## 6. Current Recommended Order

Do this order:

1. Session 1: collector boundary cleanup.
2. Session 2: deterministic benchmark expansion.
3. Session 3: scalar traversal mode cleanup.
4. Session 4: SIMD/SoA child AABB rejection prototype.
5. Session 5: flattened node stream experiment.
6. Session 6: quantization feasibility audit.

Do not swap Session 4 before Sessions 1-2. SIMD without shared collector and
better fixtures will only produce a faster-looking but less trustworthy branch.

## 7. Do / Do Not

Do:

- Treat BVH4 as candidate acceleration.
- Keep exact primitive tests shared.
- Make collector/result semantics backend-independent.
- Use fixed-seed deterministic benchmark data.
- Keep scalar path as correctness reference.
- Cite PhysX as contract/reference, not as copied code.

Do not:

- Claim "PhysX BV4 implemented."
- Claim SIMD performance without measured evidence.
- Add KCC step/floor/recovery policy into BVH traversal.
- Combine collector cleanup, SIMD, flattening, and quantization in one session.
- Delete `LinearFallback` or scalar BVH4 while experimenting.

## 8. Prompt Seeds For Later Sessions

### Session 1 Prompt

Audit and patch only the SceneQuery collector boundary so `BinaryBVH` and
`ScalarBVH4` share closest-hit and overlap top-K result semantics. Do not add
SIMD, flattening, quantization, or KCC policy. Use
`docs/audits/scenequery/10-bvh4-physx-gap-roadmap.md` as SSOT.

### Session 2 Prompt

Expand `SqBackendHarness` deterministic benchmark coverage for BVH backend
comparison. Add miss-heavy, hit-heavy, equal-time, equal-depth top-K, stack
pressure, and larger fixed-seed query batches. Do not change query semantics.
Use `LinearFallback` as oracle.

### Session 4 Prompt

Prototype SIMD/SoA child AABB rejection only after collector cleanup and
benchmark expansion pass. Keep scalar fallback and do not touch primitive
narrowphase. Compare against `ScalarBVH4` and `LinearFallback`.

## 9. Portfolio-Safe Wording

Safe:

> Implemented a scalar BVH4-style SceneQuery backend prototype and a backend
> harness comparing correctness and traversal metrics against binary BVH and a
> linear oracle.

Safe:

> Mined PhysX BV4 contracts to separate candidate acceleration, collector
> semantics, and SIMD child-bound rejection before attempting optimization.

Unsafe:

> Implemented PhysX BV4.

Unsafe:

> SIMD BVH4 is faster.

Unsafe:

> BVH4 fixes KCC collision.
