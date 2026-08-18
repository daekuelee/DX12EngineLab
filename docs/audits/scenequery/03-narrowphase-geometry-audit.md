# SceneQuery Invariant Audit: Narrowphase Geometry

Updated: 2026-05-10

## 1. Scope

This audit covers primitive-level sweep and overlap behavior:

- `Engine/Collision/SceneQuery/SqNarrowphaseLegacy.h`
- `Engine/Collision/SceneQuery/SqDistance.h`
- `Engine/Collision/SceneQuery/SqPrimitiveTests.h`

Reference contracts:

- `docs/reference/physx/contracts/capsule-triangle-sweep.md`
- `docs/reference/physx/contracts/sweep-toi-hit-normal.md`
- `docs/reference/physx/contracts/initial-overlap-mtd.md`
- `docs/reference/physx/contracts/distance-closest-point.md`

## 2. Query Result Contract

| Field | Meaning in current code | Evidence | Ambiguity | Required invariant |
|---|---|---|---|---|
| Sphere-triangle sweep normal | Face/edge/vertex hit normal, normalized and flipped against motion. | `SqNarrowphaseLegacy.h:131-140`, `SqNarrowphaseLegacy.h:200-260`, `SqNarrowphaseLegacy.h:263-274` | Initial-overlap fallback may use `-delta`, not closest-pair normal. | Distinguish impact normal from initial-overlap normal. |
| Capsule-triangle sweep normal | Chosen from degenerate sphere path, colinear front-sphere path, or extruded prism-face sphere sweep. | `SqNarrowphaseLegacy.h:411-438`, `SqNarrowphaseLegacy.h:441-465`, `SqNarrowphaseLegacy.h:468-492` | Non-initial normal may be proxy prism normal, not reconstructed source-triangle normal. | Declare proxy/source normal ownership. |
| Capsule-box sweep normal | Box surface is triangulated into 12 triangles, then each face triangle is extruded and sphere-swept. | `SqNarrowphaseLegacy.h:543-555`, `SqNarrowphaseLegacy.h:617-638`, `SqNarrowphaseLegacy.h:665-690`, `SqNarrowphaseLegacy.h:695-709` | Box diagonal triangulation can leak into feature id and tie-break behavior. | Source box face identity should be reconstructable or feature id must remain opaque. |
| Overlap normal/depth | Closest-pair direction when reliable; fallback to min-axis or triangle normal near zero. | `SqNarrowphaseLegacy.h:741-780`, `SqNarrowphaseLegacy.h:812-840` | Not a multi-contact MTD solver. | KCC recovery must treat contacts as individual facts, not a solved manifold. |

## 3. Hit Normal and TOI Semantics

PhysX capsule-triangle sweep also uses extrusion internally, but its contract
does not leave final hit data as a purely proxy-triangle result. It recomputes
hit position against the original source triangle after choosing the candidate.
Reference: `capsule-triangle-sweep.md:27-33`, `capsule-triangle-sweep.md:43-46`.

Current EngineLab capsule-triangle sweep builds 7 extruded faces and packs
`(prismFace << 8) | sphereTriFeature` into `featureId`. Evidence:
`SqNarrowphaseLegacy.h:281-314`, `SqNarrowphaseLegacy.h:468-482`.

Current capsule-box sweep triangulates the box into 12 surface triangles and
then extrudes each triangle. Evidence: `SqNarrowphaseLegacy.h:499-509`,
`SqNarrowphaseLegacy.h:543-555`, `SqNarrowphaseLegacy.h:665-680`.

That implementation can be a valid approximation lane, but it needs a clear
contract: normals and features may describe generated proxy geometry unless a
later reconstruction step maps them back to the source primitive.

## 4. Broadphase / Narrowphase Boundary

Primitive narrowphase currently receives `SweepFilter` and `rejectInitialOverlap`
from the query path. Evidence: `SqQueryLegacy.h:77-105`,
`SqNarrowphaseLegacy.h:63-80`, `SqNarrowphaseLegacy.h:328-338`,
`SqNarrowphaseLegacy.h:555-563`.

That mixes raw geometry generation with query/caller policy. PhysX raw geometry
dispatch is separate from SceneQuery filtering and block/touch classification.
Reference: `geometry-query-api.md:56-60`, `query-filtering.md:29-33`,
`query-filtering.md:42-45`.

## 5. Policy Leakage

Narrowphase applies normal-based filtering through `PassNarrowfilter` before the
query collector has a chance to see the raw candidate. Evidence:
`SqNarrowphaseLegacy.h:63-80`, `SqNarrowphaseLegacy.h:136-138`,
`SqNarrowphaseLegacy.h:363-365`, `SqNarrowphaseLegacy.h:583-586`.

This is the wrong layer for future all-hits/touch-buffer work because rejected
raw hits become invisible to diagnostics.

## 6. Determinism Risks

### Issue: Capsule-box sweep uses triangle decomposition as observable hit identity

- Severity: P1
- EngineLab evidence: `BuildBoxSurfaceTris12` defines 12 triangles with fixed
  diagonals (`SqNarrowphaseLegacy.h:499-509`), and box sweep packs `triId` into
  the returned feature id (`SqNarrowphaseLegacy.h:553-555`,
  `SqNarrowphaseLegacy.h:679-680`).
- Reference evidence: PhysX box/mesh sweep finalizers preserve face index and
  normal conventions at the final hit boundary (`mesh-sweeps-ordering.md:28-29`,
  `mesh-sweeps-ordering.md:36-41`).
- Current behavior: A box face diagonal can become part of hit identity and
  tie-break behavior.
- Why it matters: KCC seam/corner behavior may change around a box face diagonal
  even when the source collision object is a cube face.
- Minimal test: Sweep a capsule into both triangles of the same source box face
  near the diagonal and verify the selected normal/feature does not create
  artificial seam differences.

### Issue: Proxy prism normals can be consumed as source surface normals

- Severity: P1
- EngineLab evidence: Capsule-triangle path selects from generated prism faces
  (`SqNarrowphaseLegacy.h:468-482`), then returns `outN = bestN` with only a
  final motion-opposition flip (`SqNarrowphaseLegacy.h:485-492`).
- Reference evidence: PhysX uses extrusion internally but recomputes final hit
  position on the source triangle (`capsule-triangle-sweep.md:27-33`).
- Current behavior: KCC may see a normal created by the extrusion/proxy geometry.
- Why it matters: Wall/floor/seam classification that assumes a source surface
  normal can misclassify edges or generated side faces.
- Minimal test: Sweep capsule along a triangle edge and record whether the
  returned normal classifies as wall, floor, or proxy side face.

### Issue: Near-zero overlap normals are intentionally heuristic

- Severity: P1
- EngineLab evidence: Capsule-AABB overlap uses closest-pair normal when
  `dist2 > 1e-8`, otherwise min-penetration axis (`SqNarrowphaseLegacy.h:749-770`).
  Capsule-triangle overlap uses closest-pair normal when reliable, otherwise
  oriented triangle normal (`SqNarrowphaseLegacy.h:821-836`).
- Reference evidence: PhysX public `computePenetration` has explicit MTD
  direction/depth semantics, including singular fallback handling
  (`initial-overlap-mtd.md:23-33`).
- Current behavior: Local overlap contacts are useful raw facts but are not a
  full MTD manifold solver.
- Why it matters: Summing several heuristic normals in KCC recovery can push
  upward or sideways depending on local contact mix.
- Minimal test: Capsule overlapping two adjacent AABBs at a seam should report
  stable contact normals and depths independent of collider order.

### Issue: Primitive input validation is minimal

- Severity: P2
- EngineLab evidence: `NormalizeSafe` has NaN-safe fallback (`SqMath.h:56-63`),
  but sweep entry points mostly assume finite positions/radii/delta once called.
- Reference evidence: PhysX GeometryQuery validates poses, directions, distance,
  and supported geometry combinations at the API boundary
  (`geometry-query-api.md:68-74`).
- Current behavior: Invalid inputs are partly tolerated by fallback math rather
  than rejected at a defined API boundary.
- Why it matters: Debug probes can hide bad inputs by producing fallback normals.
- Minimal test: Finite-input assertions at the CollisionWorld/SceneQuery entry
  boundary, not inside every primitive helper.

## 7. Minimal Fix Direction

Do not implement from this audit alone.

Smallest next steps:

1. Add deterministic probes for capsule-box face diagonal sweeps.
2. Add probes that compare sweep initial-overlap normal with overlap contact
   normal for the same primitive pair.
3. Add a source/proxy feature contract before KCC consumes `featureId`.
4. Do not rewrite narrowphase to GJK/EPA until existing proxy-normal behavior is
   measured and fenced by tests.
