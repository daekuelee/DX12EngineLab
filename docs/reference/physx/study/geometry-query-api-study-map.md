# PhysX Study Map: geometry-query-api

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx`
- Related contract cards:
  - `docs/reference/physx/contracts/geometry-query-api.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to read PhysX `PxGeometryQuery` as an API boundary. The goal is not yet to understand primitive TOI, normal, or closest-feature math; it is to know what the public API accepts, what it returns, what it rejects, and where it dispatches next.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`

## Reading Order

1. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:63` - public `PxGeometryQuery` class and declared APIs.
2. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:67` - sweep API parameters, supported swept geometries, target combinations, result validity, and inflation.
3. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:102` - overlap API and unsupported pair list.
4. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:122` - raycast API and hit-count return.
5. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:146` - computePenetration MTD output meaning.
6. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:180` - pointDistance squared-distance and closestPoint semantics.
7. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:196` - getWorldBounds inflation input.
8. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:100` - implementation wrapper for sweep validation and dispatch.
9. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:192` - overlap wrapper into `Gu::overlap`.
10. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:200` - raycast validation and dispatch.
11. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:221` - pointDistance implementation switch.
12. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:313` - getWorldBounds wrapper into `Gu::computeBounds`.
13. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:328` - computePenetration validation, pair-order normalization, and MTD direction flip.
14. `.ref/PhysX_4.0/physx/source/geomutils/include/GuRaycastTests.h:52`, `.ref/PhysX_4.0/physx/source/geomutils/src/GuOverlapTests.h:74`, `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.h:78`, `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.h:48` - raw helper function pointer signatures.

## Call Path

```text
PxGeometryQuery::sweep
  -> validate poses, direction finite, distance finite/range
  -> choose gGeomSweepFuncs map from geom0 type
  -> dispatch to capsule/box/convex sweep helper

PxGeometryQuery::overlap
  -> Gu::overlap
  -> validate poses
  -> normalize geometry type order
  -> dispatch through gGeomOverlapMethodTable

PxGeometryQuery::raycast
  -> validate origin, direction, pose, maxDist, unit direction
  -> dispatch through gRaycastMap[geom.getType()]

PxGeometryQuery::computePenetration
  -> validate poses
  -> normalize geometry type order
  -> dispatch through gGeomMTDMethodTable
  -> negate MTD when operands were reversed

PxGeometryQuery::pointDistance
  -> validate pose
  -> switch on sphere/capsule/box/convex
  -> return squared distance, 0 inside, -1 unsupported

PxGeometryQuery::getWorldBounds
  -> validate pose
  -> Gu::computeBounds(bounds, geom, pose, 0.0f, NULL, inflation)
```

## Data / Result Flow

- Inputs: every API uses public `PxGeometry` and `PxTransform` boundaries; sweep/raycast also take direction and distance-like inputs.
- Intermediate state: `GuGeometryQuery.cpp` validates the API-level inputs and forwards to `gGeomSweepFuncs`, `gGeomOverlapMethodTable`, `gRaycastMap`, `gGeomMTDMethodTable`, point-distance helpers, or `Gu::computeBounds`.
- Outputs: sweep writes `PxSweepHit` only on true, overlap returns bool, raycast returns hit count, computePenetration writes MTD direction/depth, pointDistance returns squared distance, getWorldBounds returns `PxBounds3`.
- Failure or rejection path: invalid poses/directions/distances fail before dispatch; unsupported pointDistance returns `-1.0`; unsupported overlap/MTD pairs use NotSupported callbacks; invalid getWorldBounds pose returns empty bounds.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `PxGeometryQuery` | `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:63` | Public GeometryQuery API boundary. |
| `PxGeometryQuery::sweep` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:100` | Validates sweep inputs and dispatches by swept geometry type. |
| `PxGeometryQuery::overlap` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:192` | Thin wrapper into `Gu::overlap`. |
| `Gu::overlap` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuOverlapTests.h:92` | Validates overlap poses and normalizes pair order. |
| `PxGeometryQuery::raycast` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:200` | Enforces unit direction and dispatches by target geometry type. |
| `PxGeometryQuery::computePenetration` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:328` | MTD API wrapper with pair-order normalization and direction flip. |
| `PxGeometryQuery::pointDistance` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:221` | Defines public point-distance support boundary. |
| `PxGeometryQuery::getWorldBounds` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:313` | Public wrapper around world AABB calculation. |
| `gGeomSweepFuncs` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp:543` | Sweep helper dispatch table bundle. |
| `gGeomOverlapMethodTable` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuOverlapTests.cpp:599` | Overlap pair dispatch table. |
| `gRaycastMap` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuRaycastTests.cpp:544` | Raycast target geometry dispatch table. |
| `gGeomMTDMethodTable` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp:1379` | MTD pair dispatch table. |
| `Gu::computeBounds` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuBounds.cpp:210` | World bounds helper used by getWorldBounds. |

## Contracts Produced

- `docs/reference/physx/contracts/geometry-query-api.md`

## Terms To Remember

- `API facade`: `PxGeometryQuery` does validation and dispatch, but primitive-specific math lives in geomutils helpers.
- `pair-order normalization`: overlap and MTD choose a canonical geometry order before table dispatch.
- `MTD flip`: computePenetration negates the returned MTD when it dispatches with operands reversed.
- `heightfield registration`: raw tables initially use unregistered heightfield callbacks for raycast/sweep/overlap until registration swaps them.
- `strictly positive closestPoint`: pointDistance closestPoint is documented as valid only when returned distance is greater than zero.

## EngineLab Comparison Questions

- Which local layer validates query inputs before narrowphase math?
- Does local overlap/MTD normalize pair order, and if so does it preserve result direction semantics?
- Does local raycast reject non-unit directions or silently normalize them?
- Does local sweep distinguish finite direction checks from normalized direction checks?
- Does local pointDistance return squared distance, return `0` inside, and expose unsupported geometry distinctly?
- Does local getWorldBounds treat inflation as an API input and keep contactOffset separate?

## Next Reading Path

- `distance-closest-point`: pointDistance internals for sphere/capsule/box/convex and raw closest-feature helpers.
- `intersection-raw`: ray/AABB, ray/triangle, primitive overlap helpers.
- `sweep-toi-hit-normal`: sweep hit distance, normal direction, initial overlap, and MTD flags.
- `mesh-sweeps`: mesh candidate traversal and hit selection after GeometryQuery dispatch.
