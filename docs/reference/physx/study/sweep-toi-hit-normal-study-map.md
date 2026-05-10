# PhysX Study Map: sweep-toi-hit-normal

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx`
- Related contract cards:
  - `docs/reference/physx/contracts/sweep-toi-hit-normal.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to study how PhysX turns sweep time-of-impact into `PxSweepHit` fields. The key distinction is normal sweep hit vs initial overlap: normal hits report a linear travel distance and geometric hit normal, while non-MTD initial overlaps report `distance = 0` and `normal = -unitDir`.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`

## Reading Order

1. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:67` - public sweep signature, supported geometry combinations, and input/output contract.
2. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:100` - validation and geom0 dispatch.
3. `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.h:78` - sweep function pointer tables.
4. `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp:543` - concrete dispatch table contents.
5. `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp:45` - representative capsule-vs-box GJK sweep result writeback.
6. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepTriangleUtils.h:284` - shared initial-overlap result helper.
7. `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.h:71` - shared MTD finalization helper.
8. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepConvexTri.h:40` - convex-vs-triangle TOI and closest-distance update.
9. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:188` - mesh sweep finalization split for initial overlap vs normal hit.

## Call Path

```text
PxGeometryQuery::sweep(unitDir, distance, geom0, pose0, geom1, pose1, hit, flags, inflation)
  -> validate poses, finite unitDir, finite distance, initial-overlap assumptions
  -> switch geom0
  -> sphere: convert to zero half-height capsule
  -> capsule: build world capsule
  -> box: build Gu::Box
  -> convex: use convex sweep table
  -> primitive or mesh helper writes PxSweepHit
```

## Data / Result Flow

- `unitDir` is the travel direction for the swept shape.
- `distance` / `maxDist` is a linear sweep range, not a squared distance.
- Primitive GJK helpers compute normalized `toi`, then store `toi * distance` as `hit.distance` for normal hits.
- Normal hits generally write `POSITION` and `NORMAL`; mesh helpers also write `FACE_INDEX`.
- Initial overlap without MTD writes `distance = 0.0f` and `normal = -unitDir`.
- Initial overlap with `eMTD` can write depenetration distance, normal, and position through MTD helpers.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `PxGeometryQuery::sweep` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:100` | Public validation and dispatch boundary. |
| `GeomSweepFuncs` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.h:98` | Table bundle for capsule, box, precise, and convex sweeps. |
| `gGeomSweepFuncs` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp:543` | Concrete geometry-pair dispatch table. |
| `sweepCapsule_BoxGeom` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp:45` | Compact example of GJK TOI, initial overlap, MTD, and result writeback. |
| `setInitialOverlapResults` | `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepTriangleUtils.h:285` | Shared non-MTD initial-overlap convention. |
| `setupSweepHitForMTD` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.h:71` | Shared MTD result finalization. |
| `sweepConvexVsTriangle` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepConvexTri.h:40` | Triangle candidate TOI, backface culling, face index, and nearest-distance update. |
| `SweepCapsuleMeshHitCallback::finalizeHit` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:188` | Mesh finalization path for MTD vs normal hits. |

## Contracts Produced

- `docs/reference/physx/contracts/sweep-toi-hit-normal.md`

## Terms To Remember

- `toi`: normalized sweep time before multiplying by full sweep distance.
- `linear distance`: `PxSweepHit.distance` stores travel distance along `unitDir`.
- `initial overlap`: hit begins overlapped, so non-MTD result uses `distance = 0`.
- `eMTD`: requests depenetration-style result for initial overlap.
- `-unitDir normal`: PhysX's non-MTD initial-overlap normal convention.
- `precise sweep`: flag that selects alternate capsule/box dispatch tables.

## EngineLab Comparison Questions

- Does the local sweep API store linear travel distance rather than squared distance?
- Are initial overlaps detected before normal TOI writeback?
- Does non-MTD initial overlap always use `normal = -unitDir`?
- Is MTD treated as a distinct depenetration branch?
- Are primitive geometry sweeps separated from scene-query filtering and hit ordering?

## Next Reading Path

- `capsule-triangle-sweep`: primitive capsule-vs-triangle edge/face behavior.
- `initial-overlap-mtd`: MTD result direction and depth semantics.
- `mesh-sweeps-ordering`: triangle mesh candidate traversal and nearest-hit update.
