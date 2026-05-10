# BVH4 SIMD Child Test Prototype

Updated: 2026-05-11

## 1. Purpose

This document records the first EngineLab SIMD-shaped BVH4 child AABB rejection
prototype.

This is not a PhysX BV4 clone, not a flattened mesh midphase, not a quantized
node stream, and not a KCC behavior fix. It only adds a separate harness backend
that packet-tests up to four BVH4 child AABBs before reusing the same primitive
leaf callbacks and result collectors as the scalar BVH4 path.

## 2. Reference Contract Imported

From the reviewed PhysX BV4 contracts, this session imports only these
principles:

- BV4/BVH4 child rejection is a candidate filter, not exact collision.
- Exact primitive hit/contact generation stays in leaf callbacks.
- SIMD belongs around four-child child-bound tests after scalar equivalence is
  measurable.
- SIMD must be gated and have a scalar fallback path.
- Performance must not be claimed from a single isolated SIMD-looking change.

Reference evidence:

| Source | Imported principle |
|---|---|
| `docs/reference/physx/contracts/bv4-layout-traversal.md:66` | Node rejection is AABB-level only; exact hit semantics are deferred to leaf callbacks. |
| `docs/reference/physx/contracts/bv4-layout-traversal.md:67` | BV4 AABB sweep rejection is SIMD-shaped with vector comparisons and mask extraction. |
| `docs/reference/physx/contracts/bv4-layout-traversal.md:90` | SIMD should be introduced around SoA child bound tests only after scalar contract equivalence. |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:220` | PhysX SIMD paths are gated/backend-specific. |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:246` | Four-child slab traversal uses a swizzled four-child layout. |
| `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md:269` | BV4 performance is a stack of layout, ordering, callback, and query-specific choices. |

## 3. EngineLab Implementation Shape

| Area | Current behavior |
|---|---|
| Node cache | `BVH4Node` keeps the existing readable `BVH4Slot slots[4]` layout and adds `BVH4NodeBoundsSoA boundsSoA` as a cached packet view. Evidence: `Engine/Collision/SceneQuery/SqBVH4.h:41`, `Engine/Collision/SceneQuery/SqBVH4.h:51`. |
| Build refresh | `BuildBVH4NodeRange` refreshes the SoA cache for both leaf and internal nodes. Evidence: `Engine/Collision/SceneQuery/SqBVH4.h:82`, `Engine/Collision/SceneQuery/SqBVH4.h:172`, `Engine/Collision/SceneQuery/SqBVH4.h:207`. |
| SIMD gate | `SqBVH4.h` uses `EL_MATH_ENABLE_SIMD`; scalar fallback mirrors the same public backend when SIMD is unavailable. Evidence: `Engine/Collision/SceneQuery/SqBVH4.h:22`, `Engine/Collision/SceneQuery/SqBVH4.h:322`. |
| Sweep packet helper | `GatherBVH4SweepChildHitsSimd` packet-tests child bounds and still emits `BVH4SweepChildHit` entries sorted by `tEnter`. Evidence: `Engine/Collision/SceneQuery/SqBVH4.h:300`, `Engine/Collision/SceneQuery/SqBVH4.h:367`. |
| Overlap packet helper | `GatherBVH4OverlapChildMaskSimd` returns an accepted child mask and leaves exact contact generation to `ConsiderBVH4LeafOverlap`. Evidence: `Engine/Collision/SceneQuery/SqBVH4.h:371`, `Engine/Collision/SceneQuery/SqBVH4.h:760`. |
| Public backend | `SweepCapsuleClosestHit_BVH4SimdChildTest` and `OverlapCapsuleContacts_BVH4SimdChildTest` are separate harness paths, not replacements for `ScalarBVH4`. Evidence: `Engine/Collision/SceneQuery/SqBVH4.h:665`, `Engine/Collision/SceneQuery/SqBVH4.h:721`. |
| Metrics | `nodeAabbPackets` and `nodeAabbPacketLanes` expose how often the packet path actually runs. Evidence: `Engine/Collision/SceneQuery/SqMetrics.h:35`, `Engine/Collision/SceneQuery/SqMetrics.h:77`, `Engine/Collision/SceneQuery/SqMetrics.h:123`. |
| Harness | `SceneQueryBackendId::SimdBVH4` is compared against `LinearFallback`, `BinaryBVH`, and `ScalarBVH4`. Evidence: `Engine/Collision/SceneQuery/SqBackendHarness.h:19`, `Engine/Collision/SceneQuery/SqBackendHarness.cpp:486`. |

## 4. What This Deliberately Does Not Do

- It does not flatten nodes into a PhysX-style encoded stream.
- It does not add P/N directional ordering.
- It does not quantize bounds.
- It does not change primitive exact tests.
- It does not change `BetterHit`, `OverlapContactBetter`, KCC policy, floor
  logic, MTD/recovery, or movement modes.
- It does not prove SIMD is faster.

## 5. Correctness Contract

The `SimdBVH4` backend must match `LinearFallback` for the harness fixtures.
Equivalence is checked through:

- `SameHit` for capsule sweep closest-hit output;
- `SameContacts` for overlap contact output;
- equal-time sweep tie-break fixture;
- equal-depth overlap top-K fixture;
- dense-grid benchmark mismatch count.

The packet helper is allowed to change only candidate rejection cost, not final
hit/contact semantics.

## 6. Verification Snapshot

Command:

```bash
/bin/bash -lc "printf '%s\n' '#include \"Engine/Collision/SceneQuery/SqBackendHarness.h\"' '#include <cstdio>' 'int main(){ Engine::Collision::sq::RunSceneQueryBackendSelfTest(); auto r = Engine::Collision::sq::RunSceneQueryBackendBenchmark(); char buf[4096]; Engine::Collision::sq::FormatSceneQueryBackendBenchmarkReport(r, buf, sizeof(buf)); std::puts(buf); return r.correctnessPassed ? 0 : 2; }' > /tmp/run_sq_harness.cpp && g++ -pipe -std=c++17 -D_DEBUG -I. /tmp/run_sq_harness.cpp Engine/Collision/SceneQuery/SqBackendHarness.cpp -o /tmp/run_sq_harness && /tmp/run_sq_harness"
```

Observed output:

```text
SceneQuery backend benchmark
correctness=pass overlapTopologyRisk=no grid=20x20 queries=128
LinearFallback: ns/query=73860.0 nodeAabbTests=0 nodePackets=0 packetLanes=0 primitiveAabbTests=51200 narrowphaseCalls=128 maxStack=0 fallback=0 mismatches=0
BinaryBVH: ns/query=65569.1 nodeAabbTests=1920 nodePackets=0 packetLanes=0 primitiveAabbTests=384 narrowphaseCalls=128 maxStack=5 fallback=0 mismatches=0
ScalarBVH4: ns/query=65820.7 nodeAabbTests=2048 nodePackets=0 packetLanes=0 primitiveAabbTests=231 narrowphaseCalls=128 maxStack=7 fallback=0 mismatches=0
SimdBVH4: ns/query=65366.6 nodeAabbTests=2048 nodePackets=512 packetLanes=2048 primitiveAabbTests=231 narrowphaseCalls=128 maxStack=7 fallback=0 mismatches=0
```

MSBuild verification:

```bash
MSBuild.exe DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64
MSBuild.exe DX12EngineLab.sln /m /p:Configuration=Release /p:Platform=x64
```

Both builds completed with `0 Error(s)`. The remaining `C4819` warnings are
pre-existing code page warnings in multiple files.

## 7. Performance Interpretation

`nodeAabbPackets` and `nodeAabbPacketLanes` are visibility counters, not success
criteria. A good run should show:

- `correctness=pass`;
- `SimdBVH4` mismatches equal `0`;
- `SimdBVH4` has nonzero `nodePackets` and `packetLanes`;
- no `fallback` use.

Timing may be worse than scalar in this prototype because the tree is still
recursive/AoS, bounds are cached rather than packed, and exact leaf work is
unchanged. That is acceptable for this session.

## 8. Allowed Claims

Allowed:

> Added a gated `SimdBVH4` harness backend that packet-tests four child AABBs and
> verifies final sweep/overlap results against the linear oracle.

Allowed:

> Added metrics that show whether the BVH4 child packet path is actually being
> exercised.

Not allowed:

> Implemented PhysX BV4.

Not allowed:

> SIMD BVH4 is now faster.

Not allowed:

> KCC collision bugs are fixed by the BVH4 SIMD path.

## 9. Next Session Boundary

The next useful BVH4 session should not start by adding more intrinsics.

Recommended next steps:

1. Add a larger deterministic miss-heavy/hit-heavy matrix
   before changing layout.
2. Only after metrics show the packet path is worthwhile, consider a flattened
   node stream or more PhysX-like child ordering.
