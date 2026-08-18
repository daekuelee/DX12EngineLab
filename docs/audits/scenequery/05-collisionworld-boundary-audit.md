# SceneQuery Invariant Audit: CollisionWorld Boundary

Updated: 2026-05-10

## 1. Scope

This audit covers the boundary between world colliders and SceneQuery:

- `Engine/Collision/CollisionWorldLegacy.h`
- `Engine/Collision/CollisionWorldLegacy.cpp`
- `Engine/Collision/SceneQuery/SqBVH.h`
- `Engine/Collision/SceneQuery/SqQueryLegacy.h`

Reference contracts:

- `docs/reference/physx/contracts/scenequery-pipeline.md`
- `docs/reference/physx/contracts/query-filtering.md`

## 2. Query Result Contract

| Boundary item | Current behavior | Evidence | Ambiguity | Required invariant |
|---|---|---|---|---|
| Solid BVH contents | `BuildStatic` puts non-trigger AABBs and triangles into one static BVH. | `CollisionWorldLegacy.cpp:13-35` | OBB storage exists in `StaticBVH`, but current `CollisionWorldLegacy` does not populate OBBs. | World boundary must document supported solid primitive set. |
| Trigger handling | Triggers are excluded from BVH and linearly scanned for ID overlap. | `CollisionWorldLegacy.cpp:20-29`, `CollisionWorldLegacy.cpp:73-96` | `OverlapCapsule` returns trigger IDs only; solid contact overlap uses separate API. | ID overlap and contact overlap are different APIs. |
| `queryMask` for sweep | Header says `mask & queryMask` participates; implementation ignores parameter for sweep. | `CollisionWorldLegacy.h:77-85`, `CollisionWorldLegacy.cpp:53-62` | Solid sub-layer filtering is documented but not implemented. | Either implement mask filtering or narrow the documented contract. |
| `queryMask` for contact overlap | Header accepts `queryMask`; implementation ignores parameter. | `CollisionWorldLegacy.h:94-99`, `CollisionWorldLegacy.cpp:99-106` | Same as sweep. | Same as sweep. |
| Result remap | BVH-local primitive index is remapped to `m_descs` index by primitive type. | `CollisionWorldLegacy.cpp:63-69`, `CollisionWorldLegacy.cpp:107-113` | Remap assumes only AABB or Tri in current BVH. | Remap must be extended if OBBs are added. |
| Scratch ownership | One mutable `QueryScratch` is shared by const query methods. | `CollisionWorldLegacy.h:115`, `CollisionWorldLegacy.cpp:61`, `CollisionWorldLegacy.cpp:105` | Not thread-safe by design. | Single-threaded contract must stay explicit. |

## 3. Hit Normal and TOI Semantics

`CollisionWorldLegacy` does not change hit normal or TOI. It only delegates to
SceneQuery and remaps `index`. Evidence: `CollisionWorldLegacy.cpp:53-70`.

Therefore any normal/TOI bug must be fixed in SceneQuery or KCC consumption, not
in remap code, unless the wrong collider id is reported.

## 4. Broadphase / Narrowphase Boundary

PhysX SceneQuery separates static and dynamic pruners and query flags choose
which are traversed. Reference: `scenequery-pipeline.md:31-33`.

Current `CollisionWorldLegacy` has only one static BVH for solids and a trigger
linear scan. Evidence: `CollisionWorldLegacy.cpp:13-35`,
`CollisionWorldLegacy.cpp:73-96`.

This is acceptable for the current engine if documented as a static-solid query
boundary. It should not be described as a full PhysX-style query system.

## 5. Policy Leakage

The boundary still exposes `SweepFilter` directly and documents it as a
Bullet-equivalent callback filtering mechanism. Evidence:
`CollisionWorldLegacy.h:77-85`.

This is a compatibility layer smell: `CollisionWorld` should own object/world
filtering and remap, while KCC should own movement policy. Passing a
stage-specific normal predicate through this boundary makes responsibility
unclear.

## 6. Determinism Risks

### Issue: `queryMask` documented participation is not implemented for solids

- Severity: P1
- EngineLab evidence: Header says only colliders whose `mask & queryMask != 0`
  participate (`CollisionWorldLegacy.h:77-79`). Sweep ignores `queryMask` in the
  parameter list and directly queries the solid BVH (`CollisionWorldLegacy.cpp:53-62`).
  Contact overlap also ignores `queryMask` (`CollisionWorldLegacy.cpp:99-106`).
- Reference evidence: PhysX query filter data can reject shapes before callback
  filtering (`query-filtering.md:29-33`).
- Current behavior: All solids included in the BVH participate regardless of
  solid submask.
- Why it matters: Future object/layer filtering cannot be trusted at the public
  boundary, and tests may pass accidentally because only `Q_Solid` exists.
- Minimal test: Build two solid colliders with disjoint masks and sweep with a
  query mask matching only one. Expected: only matching collider can hit.

### Issue: Result remap is primitive-type-specific and incomplete for OBB

- Severity: P2
- EngineLab evidence: `StaticBVH` can store AABB, OBB, and Tri arrays
  (`SqBVH.h:47-56`), but `CollisionWorldLegacy::BuildStatic` only passes AABBs
  and triangles, with OBB pointer/count as `nullptr, 0`
  (`CollisionWorldLegacy.cpp:32-35`). Remap treats non-Tri as AABB
  (`CollisionWorldLegacy.cpp:63-69`, `CollisionWorldLegacy.cpp:107-113`).
- Reference evidence: PhysX GeometryQuery dispatch separates geometry type
  support and pair dispatch tables (`geometry-query-api.md:56-60`).
- Current behavior: OBB support exists in SceneQuery but not in world boundary
  storage/remap.
- Why it matters: Adding OBB colliders later will report wrong indices unless
  remap expands.
- Minimal test: None until OBB colliders are exposed through `ColliderDesc`.
  Document as extension blocker.

### Issue: Single mutable scratch makes const query not thread-safe

- Severity: P2
- EngineLab evidence: `m_scratch` is mutable and shared by const sweep/overlap
  methods (`CollisionWorldLegacy.h:115`, `CollisionWorldLegacy.cpp:61`,
  `CollisionWorldLegacy.cpp:105`).
- Reference evidence: PhysX SceneQuery manager/pruner architecture is outside
  this local thread-safety contract; no direct equivalence claimed.
- Current behavior: Const methods mutate shared scratch.
- Why it matters: Future async debug probes or parallel character queries can
  corrupt traversal.
- Minimal test: Not needed for current single-threaded app; document as
  single-thread-only invariant.

## 7. Minimal Fix Direction

Do not implement from this audit alone.

Smallest next steps:

1. Decide whether `queryMask` is required now. If yes, add solid mask filtering
   before/after BVH candidate remap with tests. If no, update comments.
2. Keep `CollisionWorldLegacy` documented as single-threaded.
3. Do not expose OBB colliders through world descriptors until remap arrays and
   tests exist.
4. Keep KCC stage filtering out of `CollisionWorld` long term; use a future
   query/collector adapter instead.
