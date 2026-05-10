# PhysX Study Map: mesh-sweeps-ordering

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx`
- Related contract cards:
  - `docs/reference/physx/contracts/mesh-sweeps-ordering.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to study mesh sweep result selection. The core idea is that the midphase enumerates candidate triangles, but callback objects decide whether a candidate becomes the current result, shrink remaining traversal distance, and finalize normal/position/face-index semantics.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`

## Reading Order

1. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:269` - RTree/BV4 sweep dispatch tables.
2. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:377` - capsule/box/convex mesh sweep midphase wrappers.
3. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:220` - GeometryQuery capsule-to-mesh entry into midphase.
4. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:431` - GeometryQuery box-to-mesh entry into midphase.
5. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:549` - GeometryQuery convex-to-mesh setup and callback.
6. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepMesh.h:52` - callback state fields.
7. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:124` - capsule callback candidate processing.
8. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:268` - box callback candidate processing.
9. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:483` - convex callback candidate processing.
10. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:188` - capsule finalization.
11. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:368` - box finalization.
12. `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:514` - convex finalization.

## Call Path

```text
GeometryQuery sweep dispatch
  -> sweepCapsule_MeshGeom / sweepBox_MeshGeom / sweepConvex_MeshGeom
  -> Midphase::sweep*VsMesh
  -> RTree or BV4 backend
  -> Sweep*MeshHitCallback::processHit(candidate triangle)
  -> shrink remaining traversal distance when a better hit is kept
  -> stop for initial overlap or eMESH_ANY
  -> Sweep*MeshHitCallback::finalizeHit(...)
```

## Data / Result Flow

- Candidate triangles arrive in mesh-local coordinates.
- Negative determinant scale can flip candidate vertex order.
- Primitive helpers produce per-triangle sweep hits.
- Callback state stores current status, initial-overlap status, best distance, face index, and hit fields.
- `shrunkMaxT` lets traversal avoid farther candidates after a closer hit.
- Finalization handles MTD, normal orientation, world-space position, and result flags.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `gMidphase*SweepTable` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h:269` | Backend dispatch by triangle mesh midphase. |
| `SweepShapeMeshHitCallback` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepMesh.h:53` | Shared callback state for all shape-vs-mesh sweeps. |
| `SweepCapsuleMeshHitCallback::processHit` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:124` | Per-triangle capsule candidate selection. |
| `SweepCapsuleMeshHitCallback::finalizeHit` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:188` | Capsule mesh result finalization. |
| `SweepBoxMeshHitCallback::processHit` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:268` | Precise/GJK box candidate selection. |
| `SweepBoxMeshHitCallback::finalizeHit` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:368` | Box mesh result finalization. |
| `SweepConvexMeshHitCallback::processHit` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:483` | Convex candidate update via convex-vs-triangle helper. |
| `SweepConvexMeshHitCallback::finalizeHit` | `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp:514` | Convex mesh result finalization. |

## Contracts Produced

- `docs/reference/physx/contracts/mesh-sweeps-ordering.md`

## Terms To Remember

- `midphase`: mesh acceleration structure traversal that emits candidate triangles.
- `callback`: per-candidate primitive testing and result state owner.
- `shrunkMaxT`: reduced traversal distance after a closer hit.
- `mInitialOverlap`: callback state that forces MTD or initial-overlap finalization.
- `eMESH_ANY`: early-out after a kept candidate.
- `faceIndex`: original mesh triangle id carried into `PxSweepHit`.

## EngineLab Comparison Questions

- Does local mesh sweep have a callback-owned closest state, or does traversal order directly decide the hit?
- Is candidate traversal shrunk after closer hits?
- Are initial overlaps terminal and finalized separately?
- Does any-hit explicitly trade closest ordering for early-out?
- Are mesh backend differences isolated below a stable result-selection contract?

## Next Reading Path

- `query-filtering`: where scene-query block/touch and pre/post filters sit above geometry hits.
- `scenequery-pipeline`: how high-level SceneQuery calls reach geometry and mesh midphase.
- `capsule-triangle-sweep`: primitive capsule-triangle candidate behavior.
