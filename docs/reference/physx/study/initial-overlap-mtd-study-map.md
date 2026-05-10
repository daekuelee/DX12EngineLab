# PhysX Study Map: initial-overlap-mtd

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx`
- Related contract cards:
  - `docs/reference/physx/contracts/initial-overlap-mtd.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to study the two PhysX MTD surfaces that look similar but should be kept separate: public `PxGeometryQuery::computePenetration` returns `(direction, positive depth)`, while sweep `eMTD` produces a `PxSweepHit` for an initial-overlap sweep.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`

## Reading Order

1. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:146` - public `computePenetration` contract.
2. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:328` - public wrapper validation, table dispatch, and direction negation on swapped arguments.
3. `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.h:39` - common MTD callback signature and depenetration-vector note.
4. `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp:63` - MTD direction normalization and singular fallback.
5. `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp:76` - public MTD depth clamp to positive or zero.
6. `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp:1379` - supported and unsupported geometry-pair dispatch table.
7. `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.h:71` - sweep initial-overlap MTD finalization.
8. `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.cpp:210` - capsule-vs-triangle-mesh iterative MTD.
9. `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp:72` - primitive sweep `eMTD` branch example.
10. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:194` - mesh sweep initial-overlap finalization.

## Call Path

```text
PxGeometryQuery::computePenetration(direction, depth, geom0, pose0, geom1, pose1)
  -> validate poses
  -> choose gGeomMTDMethodTable[minType][maxType]
  -> if arguments were swapped, negate direction
  -> return positive-or-zero depth on overlap

sweep(..., hitFlags = eMTD)
  -> primitive or mesh sweep detects initial overlap
  -> call pair-specific sweep MTD helper when available
  -> setupSweepHitForMTD(hit, hasContacts, unitDir)
  -> return PxSweepHit-style initial-overlap result
```

## Data / Result Flow

- Public `computePenetration`: `direction * depth` is applied to the first geometry to leave the second.
- Public `depth`: clamped to positive or zero.
- Public `direction`: normalized, with `(1,0,0)` fallback for singular zero normal.
- Sweep `eMTD`: stored in `PxSweepHit`; no-contact and zero-distance cases use `normal = -unitDir`.
- Mesh sweep MTD: may iterate, accumulate translation, and store a non-positive `hit.distance` representing penetration-style correction length.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `PxGeometryQuery::computePenetration` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:328` | Public MTD API boundary. |
| `GeomMTDFunc` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.h:56` | Pair-specific public MTD callback signature. |
| `gGeomMTDMethodTable` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp:1379` | Supported geometry-pair table. |
| `manualNormalize` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp:63` | Direction normalization and singular fallback. |
| `validateDepth` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp:76` | Public depth positivity guard. |
| `setupSweepHitForMTD` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.h:71` | Sweep initial-overlap result finalization. |
| `computeCapsule_TriangleMeshMTD` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.cpp:210` | Iterative mesh MTD for capsule sweeps. |
| `sweepCapsule_BoxGeom` MTD branch | `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp:72` | Compact primitive sweep MTD example. |

## Contracts Produced

- `docs/reference/physx/contracts/initial-overlap-mtd.md`

## Terms To Remember

- `MTD`: minimum translational distance.
- `direction * depth`: public `computePenetration` depenetration vector for `geom0`.
- `positive depth`: public API depth convention.
- `sweep eMTD`: hit-flag-driven initial-overlap result path.
- `non-positive hit.distance`: sweep MTD helper convention for penetration-style `PxSweepHit` distance.
- `zero-distance initial overlap`: uses `normal = -unitDir`.

## EngineLab Comparison Questions

- Does local public penetration keep `direction` relative to the first input shape?
- Is public depth always positive or zero?
- Does local sweep MTD use a separate `PxSweepHit` convention instead of reusing public penetration outputs directly?
- Are no-contact MTD and exact-touching initial overlaps normalized to `normal = -unitDir`?
- Are unsupported geometry pairs rejected rather than inferred?

## Next Reading Path

- `sweep-toi-hit-normal`: ordinary sweep hit and initial-overlap split.
- `mesh-sweeps-ordering`: how mesh sweep detects initial overlap before MTD finalization.
- `geometry-query-api`: public API-level supported combinations.
