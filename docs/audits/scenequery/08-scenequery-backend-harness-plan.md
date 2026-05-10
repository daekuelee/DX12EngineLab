# SceneQuery Backend Harness Plan

Updated: 2026-05-10

## 1. Purpose

This document is the SSOT for the SceneQuery backend harness.

The harness exists to keep BVH backend work honest before adding SIMD traversal
or mesh-midphase variants. It compares correctness, raw query metrics, timing,
and PhysX contract alignment across backends.

This is not a KCC test harness. It must not encode walkable, step, floor,
recovery, or velocity policy.

## 2. Reference Contract

PhysX contract used:

- `docs/reference/physx/contracts/bv4-layout-traversal.md`

Imported principles:

- BVH/BV4 traversal is a candidate accelerator.
- Exact primitive hits and final closest/contact selection live outside node
  layout.
- Traversal order is an optimization, not semantic truth.
- SIMD belongs first around child AABB rejection, after scalar correctness.
- KCC movement policy must not enter BVH traversal.

## 3. Harness API

Implemented entry points:

- `Engine::Collision::sq::RunSceneQueryBackendSelfTest()`
- `Engine::Collision::sq::RunSceneQueryBackendBenchmark(...)`
- `Engine::Collision::sq::FormatSceneQueryBackendBenchmarkReport(...)`

Current backend set:

| Backend | Role |
|---|---|
| `LinearFallback` | correctness oracle for small deterministic fixtures |
| `BinaryBVH` | current production backend |
| `ScalarBVH4` | scalar four-child prototype backend for future mesh-midphase work |

Future backend set:

| Backend | Role |
|---|---|
| `SimdBVH4` | future SIMD/SoA child AABB rejection backend |

## 4. Fixtures

Smoke fixtures:

| Fixture | Purpose |
|---|---|
| empty BVH | no-hit/no-contact and no overflow |
| single AABB hit | basic sweep correctness |
| single AABB miss | broadphase false-positive tolerance without false hit |
| equal-time duplicate AABB hits | deterministic `BetterHit` tie-break |
| small overlap contacts | overlap contact equivalence after sorting |

Manual benchmark fixture:

- Dense AABB grid, default `20x20`.
- Deterministic sweep batch, default `128` queries.
- Reports work-count metrics and timing.
- Does not define pass/fail on speed.

Open diagnostic fixture:

- More than `kMaxOverlapContacts` overlapping contacts.
- This exposes top-k traversal-order risk.
- It currently reports `overlapTopologyRisk=no` after the BVH4 overlap path
  switched to total-order contact retention when its contact buffer is full.

## 5. Comparison Rules

Sweep hit equivalence:

- `hit`, `type`, `index`, `featureId`, `startPenetrating`: exact match.
- `t`: `1e-5f` tolerance.
- `normal`: dot product at least `0.999f`, unless both are near-zero.
- `penetrationDepth`: `1e-4f` tolerance.

Overlap equivalence:

- Compare sorted contact arrays.
- `type`, `index`, `featureId`: exact match.
- `depth`: `1e-4f` tolerance.
- `normal`: same normal rule as sweep.

Metrics:

- Metrics must not affect query results.
- Work-count metrics are useful for algorithm comparison.
- Timing is report-only and must not be used as a smoke-test pass/fail signal.

## 6. Allowed Claims

Allowed after this harness exists:

> SceneQuery has a debug backend harness that compares `LinearFallback` and
> `BinaryBVH` for deterministic fixtures and records traversal metrics.

Allowed after scalar BVH4 passes the same harness:

> Scalar BVH4 preserves existing SceneQuery hit/contact semantics on deterministic
> fixtures and can be compared against binary BVH with shared metrics.

Not allowed:

> PhysX-equivalent BVH4.

Not allowed:

> SIMD BVH4 performance improvement.

Not allowed:

> KCC collision is fixed by BVH harness work.

## 7. Next Steps

1. Run Debug build so smoke self-test compiles and executes.
2. Add a manual trigger later for `RunSceneQueryBackendBenchmark`.
3. Factor shared collector/retention helpers between `BinaryBVH` and `BVH4`.
4. Add a larger deterministic random fixture before making speed claims.
5. Only after scalar equivalence stays stable, add SIMD child AABB rejection.
