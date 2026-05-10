# Scalar BVH4 Core Report

Updated: 2026-05-10

## 1. Purpose

This document is the SSOT for the first scalar four-child SceneQuery backend.

The implementation is future mesh-midphase groundwork. It is not a PhysX BV4
clone, not SIMD BVH4, and not a KCC behavior fix.

## 2. Source Anchors

| File | Role |
|---|---|
| `Engine/Collision/SceneQuery/SqBVH4.h` | scalar four-child node layout, build, sweep traversal, overlap traversal |
| `Engine/Collision/SceneQuery/SqBackendHarness.h` | backend enum and benchmark report shape |
| `Engine/Collision/SceneQuery/SqBackendHarness.cpp` | correctness fixtures and backend comparison |
| `docs/reference/physx/contracts/bv4-layout-traversal.md` | reference contract for BV4 as candidate acceleration |

## 3. Imported Contract

From the PhysX BV4 contract, this session imports only these principles:

- four-child traversal is a candidate accelerator;
- node AABB rejection is not exact collision;
- exact primitive tests and final hit/contact selection remain in leaf callbacks;
- traversal order must not become semantic truth;
- SIMD belongs after scalar equivalence and metrics exist.

This session deliberately does not import PhysX packed node encoding,
quantization, P/N stream layout, or SIMD intrinsics.

## 4. Implemented Behavior

| Area | Current behavior |
|---|---|
| Build | `BuildStaticBVH4` creates a separate four-child tree over the existing `StaticBVH` primitive set. |
| Source data | `StaticBVH4::sourceView` keeps the original `StaticBVH` so overflow fallback sees the same primitive set. |
| Sweep | `SweepCapsuleClosestHit_BVH4` uses four-child AABB sweep rejection and reuses `ConsiderSweepCapsulePrim`. |
| Overlap | `OverlapCapsuleContacts_BVH4` uses four-child AABB rejection and reuses `OverlapCapsulePrim`. |
| Contact retention | BVH4 overlap top-K uses `OverlapContactBetter` when the buffer is full so equal-depth contacts do not depend on traversal order. |
| Metrics | `QueryBackend::BVH4` metrics are reported by the existing `QueryScratch` path. |

## 5. Verification

Command:

```bash
/bin/bash -lc "printf '%s\n' '#include \"Engine/Collision/SceneQuery/SqBackendHarness.h\"' '#include <cstdio>' 'int main(){ Engine::Collision::sq::RunSceneQueryBackendSelfTest(); auto r = Engine::Collision::sq::RunSceneQueryBackendBenchmark(); char buf[2048]; Engine::Collision::sq::FormatSceneQueryBackendBenchmarkReport(r, buf, sizeof(buf)); std::puts(buf); return r.correctnessPassed ? 0 : 2; }' > /tmp/run_sq_harness.cpp && g++ -pipe -std=c++17 -D_DEBUG -I. /tmp/run_sq_harness.cpp Engine/Collision/SceneQuery/SqBackendHarness.cpp -o /tmp/run_sq_harness && /tmp/run_sq_harness"
```

Observed output:

```text
SceneQuery backend benchmark
correctness=pass overlapTopologyRisk=no grid=20x20 queries=128
LinearFallback: ns/query=74186.7 primitiveAabbTests=51200 narrowphaseCalls=128 maxStack=0 fallback=0 mismatches=0
BinaryBVH: ns/query=64836.6 primitiveAabbTests=384 narrowphaseCalls=128 maxStack=5 fallback=0 mismatches=0
ScalarBVH4: ns/query=66258.3 primitiveAabbTests=231 narrowphaseCalls=128 maxStack=7 fallback=0 mismatches=0
```

Interpretation:

- Correctness fixtures passed against `LinearFallback`.
- The overlap topology risk probe currently reports `no`.
- `ScalarBVH4` reduced primitive AABB tests in this fixture versus `BinaryBVH`
  but did not beat it on this single timing run.
- This is not a stable performance claim. It is only a local harness snapshot.

## 6. Current Limitations

- The layout is scalar AoS, not SIMD SoA.
- Build quality is simple median partitioning, not PhysX BV4 packing.
- It still targets the current primitive set, not a real triangle mesh midphase.
- It duplicates some traversal code instead of using a backend-independent
  collector interface.
- It is connected to the debug harness and project, but not selected by runtime
  game collision code.

## 7. Allowed Claims

Allowed:

> Added a scalar four-child SceneQuery backend prototype and a harness path that
> compares it against `LinearFallback` and `BinaryBVH` for deterministic
> sweep/overlap fixtures.

Allowed:

> The local harness snapshot showed fewer primitive AABB tests than binary BVH on
> the dense-grid fixture, with correctness preserved.

Not allowed:

> Implemented PhysX BV4.

Not allowed:

> SIMD BVH4 is faster.

Not allowed:

> KCC collision bugs are fixed by BVH4.

## 8. Next Session Boundary

Next work should not start with SIMD intrinsics. The next useful session is a
collector/traversal cleanup:

1. extract shared sweep/overlap collector helpers so `BinaryBVH` and `BVH4`
   share hit/contact retention semantics;
2. add a larger randomized deterministic benchmark fixture;
3. only then add a SIMD child-AABB rejection experiment.
