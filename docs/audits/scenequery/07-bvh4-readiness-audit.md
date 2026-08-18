# SceneQuery Invariant Audit: BVH4 Readiness

Updated: 2026-05-10

## 1. Scope

This audit checks whether the current EngineLab binary BVH/query layer is ready
to accept a future PhysX-style BV4/BVH4 backend.

Audited files:

- `Engine/Collision/SceneQuery/SqBVH.h`
- `Engine/Collision/SceneQuery/SqQueryLegacy.h`
- `Engine/Collision/SceneQuery/SqMetrics.h`
- `docs/audits/scenequery/02-bvh-broadphase-audit.md`
- `docs/reference/physx/contracts/bv4-layout-traversal.md`

Out of scope:

- Production source changes.
- SIMD implementation.
- `ex.cpp` comparison.
- KCC walking/falling/recovery policy.

## 2. Current Binary BVH Contract

| Area | Current behavior | Evidence | BVH4 implication |
|---|---|---|---|
| Layout | `BVHNode` is binary: `left`, `right`, `primStart`, `primCount`. | `Engine/Collision/SceneQuery/SqBVH.h:43-47` | BVH4 should be a separate backend storage type, not a mutation of `BVHNode`. |
| Primitive storage | `StaticBVH` owns `nodes`, `primIdx`, and flattened `PrimRef` records while borrowing source geometry arrays. | `Engine/Collision/SceneQuery/SqBVH.h:51-60` | A future BVH4 can share `PrimRef` and geometry arrays if its node stream is separate. |
| Empty BVH | Empty BVH has a degenerate root and `IsEmptyBVH` treats empty `nodes` or empty `prims` as empty. | `Engine/Collision/SceneQuery/SqBVH.h:21-22`, `Engine/Collision/SceneQuery/SqBVH.h:63-66`, `Engine/Collision/SceneQuery/SqBVH.h:188-193` | Previous empty-root risk is now guarded at query entry; BVH4 must preserve the same early no-candidate behavior. |
| Build determinism | Build uses median split and `std::stable_sort` by centroid, type, index. | `Engine/Collision/SceneQuery/SqBVH.h:11-15`, `Engine/Collision/SceneQuery/SqBVH.h:125-135` | BVH4 builder must state its own deterministic ordering, not inherit this accidentally. |
| Sweep traversal | Query resets scratch, checks empty BVH, tests root swept AABB, then DFS traverses nodes. | `Engine/Collision/SceneQuery/SqQueryLegacy.h:291-307`, `Engine/Collision/SceneQuery/SqQueryLegacy.h:309-371` | Traversal logic is still embedded in the public sweep query; backend abstraction is not present yet. |
| Closest collector | Primitive hits are accepted through broadphase time window, narrowphase dispatch, optional filter, and `BetterHit`. | `Engine/Collision/SceneQuery/SqQueryLegacy.h:158-235` | This can become the backend-independent collector boundary if extracted cleanly. |
| Overlap traversal | Overlap query uses DFS, top-k contact insertion, final sort, and overflow fallback. | `Engine/Collision/SceneQuery/SqQueryLegacy.h:405-443`, `Engine/Collision/SceneQuery/SqQueryLegacy.h:512-591` | Contact collection is deterministic after sort, but same-depth top-k eviction still depends on traversal order. |
| Metrics | Metrics already include `QueryBackend::BVH4`, traversal counters, primitive counters, result fields, overflow, and fallback. | `Engine/Collision/SceneQuery/SqMetrics.h:21-55` | Metrics are mostly ready for binary-vs-BVH4 comparison; elapsed time is not present. |

## 3. Existing Audit Delta

`02-bvh-broadphase-audit.md` is now partly stale against the current working
tree:

| Previous finding | Current status | Evidence | Follow-up |
|---|---|---|---|
| Empty BVH root can be traversed as internal node. | Resolved in current query entry points. | Sweep returns before root traversal at `Engine/Collision/SceneQuery/SqQueryLegacy.h:293-294`; overlap returns before root traversal at `Engine/Collision/SceneQuery/SqQueryLegacy.h:525`. | Keep an empty-BVH regression probe. |
| Fixed traversal stack has no overflow guard. | Resolved structurally; measurement still needed. | `PushQueryTask` sets `overflowed` instead of writing past capacity at `Engine/Collision/SceneQuery/SqQueryLegacy.h:73-87`; sweep fallback at `Engine/Collision/SceneQuery/SqQueryLegacy.h:374-379`; overlap fallback at `Engine/Collision/SceneQuery/SqQueryLegacy.h:581-586`. | Add max-stack and fallback probes before BVH4. |
| Overlap top-k tie handling depends on traversal topology. | Still open. | Top-k evicts by strict greater depth only at `Engine/Collision/SceneQuery/SqQueryLegacy.h:432-441`, while final sort happens after candidate loss at `Engine/Collision/SceneQuery/SqQueryLegacy.h:589`. | Required before claiming backend-independent contact determinism. |
| BVH4 is not immediate fix lane. | Still true for production behavior, but now metrics/backend hooks exist. | `QueryBackend::BVH4` exists at `Engine/Collision/SceneQuery/SqMetrics.h:21-24`; current query resets as `BinaryBVH` at `Engine/Collision/SceneQuery/SqQueryLegacy.h:291` and `Engine/Collision/SceneQuery/SqQueryLegacy.h:518`. | Next step is readiness/probe work, not SIMD. |

## 4. PhysX BV4 Comparison

PhysX BV4 contract to borrow:

- BV4 is a four-slot node stream and candidate accelerator, not the owner of final collision policy. Reference: `docs/reference/physx/contracts/bv4-layout-traversal.md`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h:215`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:36`.
- Child traversal order is an optimization; exact primitive hit callbacks and result finalization own correctness. Reference: `docs/reference/physx/contracts/bv4-layout-traversal.md`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h:33`.
- SIMD belongs first around child AABB rejection. Reference: `docs/reference/physx/contracts/bv4-layout-traversal.md`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:35`.
- Mesh sweep closest behavior shrinks traversal/result distance through callbacks, not by assuming backend visit order is final hit order. Reference: `docs/reference/physx/contracts/mesh-sweeps-ordering.md`.

PhysX BV4 contract not to copy directly:

- PhysX BV4 is mesh-midphase-oriented. EngineLab currently has a top-level static binary BVH over `Aabb`, `Obb`, and `Tri` `PrimRef`s. Evidence: `Engine/Collision/SceneQuery/SqBVH.h:51-60`, `Engine/Collision/SceneQuery/SqBVH.h:165-180`.
- KCC concepts such as walkable, floor, perch, step-up, step-down, and recovery must remain outside BVH traversal. Local ownership rule: `Engine/Collision/SceneQuery/AGENTS.md`.

## 5. Readiness Table

| Item | Current state | Needed for BVH4 | Blocker | Verdict |
|---|---|---|---|---|
| Layout | Binary `BVHNode` and shared `PrimRef` arrays. | New scalar BVH4 node stream that shares primitives. | Must not mutate binary layout in-place. | GO with separate backend. |
| Traversal | DFS traversal is embedded in `SweepCapsuleClosestHit_Fast` / `OverlapCapsuleContacts_Fast`. | Backend traversal should call shared collectors. | Collector boundary is implicit, not an interface. | CONDITIONAL GO. |
| Closest collector | `ConsiderSweepCapsulePrim` contains primitive AABB prune, narrowphase dispatch, query filter, and `BetterHit`. | Reuse or extract as backend-independent candidate consumer. | It still mixes filter policy with primitive consideration. | CONDITIONAL GO. |
| Overlap collector | Top-k and final sort exist. | Backend-independent stable contact collection. | Same-depth overflow eviction depends on traversal order. | NO-GO for contact equivalence claims. |
| Metrics | Backend enum and cost counters exist. | Binary vs BVH4 comparison on same query set. | No elapsed-time counter; no baseline probes documented here. | GO for structural metrics, PARTIAL for performance story. |
| SIMD | No local SIMD BVH traversal. | Scalar BVH4 correctness before SIMD/SoA. | Direct SIMD first would hide correctness bugs. | NO-GO until scalar backend passes equivalence probes. |

## 6. Issues

### Issue 1: BVH4 backend boundary is not explicit

- Severity: P1
- EngineLab evidence: Sweep traversal, primitive consideration, filter application, and closest-hit update live in one query path (`Engine/Collision/SceneQuery/SqQueryLegacy.h:158-235`, `Engine/Collision/SceneQuery/SqQueryLegacy.h:279-384`).
- Reference evidence: PhysX BV4 separates stream traversal, node overlap, leaf callback, and final hit handling (`docs/reference/physx/contracts/bv4-layout-traversal.md`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:36`).
- Current behavior: A future BVH4 would either duplicate `SweepCapsuleClosestHit_Fast` logic or need to extract a collector boundary first.
- Why it matters: Duplicating hit policy in a BVH4 implementation risks binary/BVH4 result divergence.
- Minimal test/probe: Binary traversal and a future scalar BVH4 traversal must call the same primitive collector and produce identical `Hit` for a deterministic fixture with equal-time hits.

### Issue 2: Overlap top-k cannot yet support backend-independent equivalence

- Severity: P1
- EngineLab evidence: When contact storage is full, `InsertOverlapContactTopK` evicts only if the new contact is strictly deeper (`Engine/Collision/SceneQuery/SqQueryLegacy.h:432-441`). Final deterministic sort happens only after this lossy decision (`Engine/Collision/SceneQuery/SqQueryLegacy.h:589`).
- Reference evidence: BV4 traversal order should not be the only hit-ordering rule (`docs/reference/physx/contracts/bv4-layout-traversal.md`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h:220`).
- Current behavior: Same-depth contacts beyond capacity are retained according to traversal order.
- Why it matters: A binary BVH and BVH4 can visit candidates in different orders while both are otherwise correct.
- Minimal test/probe: Construct more than 32 equal-depth capsule overlap contacts and assert the retained set is independent of primitive insertion order and backend traversal order.

### Issue 3: Metrics are structurally useful but incomplete for a performance story

- Severity: P2
- EngineLab evidence: Metrics track node pops, AABB tests/rejects, primitive tests/rejects, narrowphase calls, raw/accepted hits, max stack, overflow, fallback, and result (`Engine/Collision/SceneQuery/SqMetrics.h:26-55`). They do not track elapsed query time.
- Reference evidence: PhysX BV4 uses SIMD-shaped child AABB rejection, so a BVH4 performance claim needs both work-count metrics and timing context (`docs/reference/physx/contracts/bv4-layout-traversal.md`, `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h:35`).
- Current behavior: Current metrics can compare algorithmic work but not wall-clock performance.
- Why it matters: Portfolio wording can claim measured traversal work reduction only after probes exist; it cannot claim runtime speedup without timing.
- Minimal test/probe: Add a deterministic query batch and record per-backend aggregate counters first; add timing only after correctness equivalence is stable.

## 7. Recommended Next Sessions

1. Session 3A: metrics baseline/probe patch.
   - Add deterministic fixtures that run current binary BVH and linear fallback.
   - Assert identical `Hit` / contact results.
   - Record metrics for node visits, primitive tests, narrowphase calls, max stack, overflow, fallback.

2. Session 3B: scalar BVH4 design and skeleton.
   - Add separate BVH4 node storage.
   - Do not add SIMD.
   - Reuse the same primitive collector path as binary traversal.

3. Session 4: scalar BVH4 equivalence tests.
   - Binary BVH vs scalar BVH4 result equivalence.
   - Equal-time sweep tie-break fixture.
   - Overlap contact stability fixture.

4. Session 5: SIMD/SoA traversal.
   - Only after scalar BVH4 has equivalence tests.
   - SIMD scope limited to four-child AABB rejection.

## 8. Final Verdict

Do not implement SIMD BVH4 next.

The current code is closer than the old audit suggests because empty BVH and
stack overflow handling now exist. However, BVH4 should still start with
metrics and scalar backend equivalence. The blocker is not "we cannot build a
4-child tree"; the blocker is that backend-independent collector/result
semantics are not explicit enough to safely claim binary/BVH4 equivalence.

