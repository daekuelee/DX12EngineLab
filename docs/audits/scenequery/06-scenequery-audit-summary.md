# SceneQuery Audit Summary

Updated: 2026-05-10

## 1. Verdict

Current SceneQuery is usable as a small static-world closest-sweep/overlap
backend, but it is not yet a clean PhysX-like SceneQuery layer.

The most important risks are not BVH4 or raw performance. They are:

1. empty BVH traversal safety,
2. local stack overflow defense,
3. filter ownership leakage,
4. source/proxy hit normal ambiguity,
5. `CollisionWorldLegacy::queryMask` contract mismatch.

## 2. Top Findings

| Rank | Issue | Severity | Evidence | Next step |
|---:|---|---|---|---|
| 1 | Empty BVH can be traversed as internal root | P0 | `SqBVH.h:179-183`, `SqQueryLegacy.h:157-160`, `SqQueryLegacy.h:212-225`, `SqQueryLegacy.h:308-350` | Add empty BVH sweep/overlap probe, then early return guard. |
| 2 | `QueryScratch::stack[512]` has no overflow guard | P0 | `SqQueryLegacy.h:49-51`, `SqQueryLegacy.h:148`, `SqQueryLegacy.h:220`, `SqQueryLegacy.h:304`, `SqQueryLegacy.h:348-350` | Add max stack probe and guard/fallback. |
| 3 | Filter is applied in narrowphase and query layer | P1 | `SqNarrowphaseLegacy.h:63-80`, `SqQueryLegacy.h:175-189` | Add raw-hit vs accepted-hit probe before moving filter. |
| 4 | `SweepFilter` encodes KCC stage semantics | P1 | `SqTypes.h:147-163` | Move toward collector/query policy language; keep primitive helpers raw. |
| 5 | Capsule-box feature/normal can expose proxy triangle diagonals | P1 | `SqNarrowphaseLegacy.h:499-509`, `SqNarrowphaseLegacy.h:665-680` | Add box face diagonal sweep probe. |
| 6 | Overlap top-k same-depth retention depends on DFS topology | P1 | `SqQueryLegacy.h:250`, `SqQueryLegacy.h:327-354` | Add >32 same-depth contact probe. |
| 7 | `queryMask` is documented but ignored for solid sweep/contact overlap | P1 | `CollisionWorldLegacy.h:77-85`, `CollisionWorldLegacy.cpp:53-62`, `CollisionWorldLegacy.cpp:99-106` | Add mask-filter probe or narrow docs. |
| 8 | `penetrationDepth` on sweep `Hit` lacks stable MTD contract | P1 | `SqTypes.h:120-123`, `SqNarrowphaseLegacy.h:190-192`, `SqNarrowphaseLegacy.h:414-416`, `SqNarrowphaseLegacy.h:635-637` | Decide telemetry vs stable API. |

## 3. PhysX Comparison

The current local architecture only partially maps to PhysX:

- PhysX SceneQuery separates public API, pruner traversal, exact geometry, query
  filtering, block/touch classification, and result buffers.
- EngineLab currently has one static BVH, closest sweep, overlap contact query,
  and a normal-dot filter.
- PhysX BV4 evidence in the existing contracts belongs to mesh midphase, not a
  direct mandate to replace EngineLab's top-level static binary BVH.

Reference:

- `scenequery-pipeline.md:29-46`
- `query-filtering.md:21-45`
- `mesh-sweeps-ordering.md:23-44`

## 4. Recommended Order

1. Fix/probe P0 traversal safety before KCC consumes more SceneQuery behavior.
2. Add raw-hit/accepted-hit debug probe before changing filter ownership.
3. Add source/proxy normal probes for box diagonal and capsule-triangle edge
   cases.
4. Decide `queryMask` truth at `CollisionWorldLegacy`.
5. Only then consider collector/filter refactor.
6. Defer BVH4 until a real triangle-mesh midphase or performance profile demands it.

## 5. What Not To Do Next

- Do not start with BVH4. The current audit found correctness/contract issues
  that BVH4 will not solve.
- Do not move KCC StepDown/StepMove policy directly into narrowphase.
- Do not claim PhysX compatibility because function names say `PhysXLike`.
- Do not use `t == 0` as initial-overlap proof; use `startPenetrating`.
- Do not treat overlap contacts as a solved MTD manifold.

## 6. Minimal Probe Set

The next test/probe session should cover:

1. Empty BVH sweep returns no hit without stack growth.
2. Empty BVH overlap returns zero contacts without stack growth.
3. 10k cube broad sweep reports max stack usage below capacity.
4. Closest raw hit rejected by filter is still observable in debug probe.
5. Capsule-box sweep near face diagonal reports stable normal/source identity.
6. Capsule seam overlap with >32 contacts has deterministic retained contact set.
7. Solid `queryMask` either filters correctly or is documented unsupported.

## 7. Portfolio / Public Repo Caution

SceneQuery should be described as:

> static BVH-backed capsule sweep/overlap prototype with deterministic closest-hit
> ordering and ongoing contract audits for initial overlap, filtering, and
> source/proxy normal semantics.

Avoid:

> PhysX-compatible SceneQuery implementation.

Avoid:

> production-ready KCC collision backend.
