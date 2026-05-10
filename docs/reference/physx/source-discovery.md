# PhysX Source Discovery

Updated: 2026-05-08

## Purpose

This file is a reusable discovery log for checked-out PhysX source.
It is not a behavior contract by itself.
Use it to avoid repeating source-location work before mining contract cards.

## Evidence Policy

- `context.txt`, `*.context.md`, generated summaries, and prior notes are search hints only.
- Behavioral claims require raw source `file:line` evidence.
- Contract conclusions belong under `docs/reference/physx/contracts/`.
- A contract card is not final design authority until reviewed and accepted by a local context or contract document.

## Source Roots Found

- `.ref/PhysX_4.0/physx`
- `.ref/PhysX_4.1/physx`
- `.ref/PhysX_3.4/PhysX_3.4`
- `.ref/PhysX34/PhysX_3.4`
- `.ref/paste`

## Initial Located Files

SceneQuery / GeometryQuery:

- `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h`
- `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h`
- `.ref/PhysX_4.0/physx/source/scenequery/include/SqSceneQueryManager.h`
- `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp`
- `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp`
- `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.cpp`

CCT:

- `.ref/PhysX_4.0/physx/include/characterkinematic/PxController.h`
- `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp`
- `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctSweptCapsule.cpp`

## Discovery Entries

### 2026-05-08 - Initial PhysX roots

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`, `.ref/PhysX_4.1/physx`, `.ref/PhysX_3.4/PhysX_3.4`
- Search terms: `PxGeometryQuery.h`, `PxQueryFiltering.h`, `SqSceneQueryManager.h`, `GuSweepCapsuleTriangle.cpp`, `GuSweepsMesh.cpp`, `GuSweepMTD.cpp`, `PxController.h`, `CctCharacterController.cpp`, `CctSweptCapsule.cpp`
- Located files: see `Initial Located Files`
- Missing files: none recorded for this initial search
- Notes: this entry records source availability only; no behavior contract has been accepted.

### 2026-05-08 - Geometry math foundation

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`, `.ref/PhysX_4.0/pxshared`
- Searched roots:
  - `.ref/PhysX_4.0/physx`
  - `.ref/PhysX_4.0/pxshared`
- Search terms: `PxVec3.h`, `PxTransform.h`, `PxQuat.h`, `PxMat33.h`, `PxBounds3.h`, `PxMath.h`, `PxGeometry.h`, `PxBoxGeometry.h`, `PxSphereGeometry.h`, `PxCapsuleGeometry.h`, `PxTriangle.h`, `PxGeometryQuery.h`, `GuGeometryQuery.cpp`, `GuBox.h`, `GuSegment.h`, `GuCapsule.h`, `GuSphere.h`, `GuDistancePointTriangle.cpp`, `GuIntersectionRayBox.cpp`, `GuSweepTests.cpp`, `GuSweepTriangleUtils.cpp`, `Vec3V`, `FloatV`, `V3LoadU`, `PsVecMath`, `aos`
- Located files:
  - `.ref/PhysX_4.0/pxshared/include/foundation/PxVec3.h`
  - `.ref/PhysX_4.0/pxshared/include/foundation/PxTransform.h`
  - `.ref/PhysX_4.0/pxshared/include/foundation/PxQuat.h`
  - `.ref/PhysX_4.0/pxshared/include/foundation/PxMat33.h`
  - `.ref/PhysX_4.0/pxshared/include/foundation/PxBounds3.h`
  - `.ref/PhysX_4.0/pxshared/include/foundation/PxMath.h`
  - `.ref/PhysX_4.0/physx/include/geometry/PxGeometry.h`
  - `.ref/PhysX_4.0/physx/include/geometry/PxBoxGeometry.h`
  - `.ref/PhysX_4.0/physx/include/geometry/PxSphereGeometry.h`
  - `.ref/PhysX_4.0/physx/include/geometry/PxCapsuleGeometry.h`
  - `.ref/PhysX_4.0/physx/include/geometry/PxTriangle.h`
  - `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/include/GuBox.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/include/GuSegment.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuCapsule.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSphere.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangle.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayBox.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepTriangleUtils.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/gjk/GuVecTriangle.h`
- Missing files:
  - No raw definition file for internal `Ps::aos::Vec3V` / `FloatV` was located under `.ref/PhysX_4.0/physx` or `.ref/PhysX_4.0/pxshared` using `PsVecMath`, `VecMath`, `aos`, `AoS`, `V3Load`, `Vec3V`, `FloatV`, and `BoolV` file-name searches. Raw call sites using these symbols were located.
- Notes:
  - Contract card: `docs/reference/physx/contracts/geometry-math-foundation.md`
  - Study map: `docs/reference/physx/study/geometry-math-foundation-study-map.md`
  - This entry records source availability only; behavior conclusions remain pending until the contract is reviewed.

### 2026-05-08 - GeometryQuery public API

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`
- Searched roots:
  - `.ref/PhysX_4.0/physx/include/geometry`
  - `.ref/PhysX_4.0/physx/source/geomutils`
- Search terms: `PxGeometryQuery`, `sweep`, `overlap`, `raycast`, `computePenetration`, `pointDistance`, `getWorldBounds`, `gGeomSweepFuncs`, `gRaycastMap`, `gGeomOverlapMethodTable`, `gGeomMTDMethodTable`, `computeBounds`, `GeomSweepFuncs`, `RaycastFunc`, `GeomOverlapTable`, `GeomMTDFunc`
- Located files:
  - `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/include/GuRaycastTests.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuRaycastTests.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuOverlapTests.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuOverlapTests.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepSharedTests.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepSharedTests.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuBounds.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuBounds.cpp`
- Missing files:
  - None for this API-boundary pass.
- Notes:
  - Contract card: `docs/reference/physx/contracts/geometry-query-api.md`
  - Study map: `docs/reference/physx/study/geometry-query-api-study-map.md`
  - This pass intentionally stopped at the public API/dispatch boundary. Primitive-specific TOI, hit normal, closest-feature, and mesh ordering behavior should be mined in separate cards.

### 2026-05-08 - Distance / closest point

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`
- Searched roots:
  - `.ref/PhysX_4.0/physx/include/geometry`
  - `.ref/PhysX_4.0/physx/source/geomutils`
- Search terms: `pointDistance`, `distancePointSegmentSquared`, `distancePointBoxSquared`, `distancePointTriangleSquared`, `pointConvexDistance`, `closestPoint`, `sqrDistance`, `sqDistance`
- Located files:
  - `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointSegment.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointBox.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointBox.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangle.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangle.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangleSIMD.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/include/GuSegment.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuCapsule.h`
- Missing files:
  - None for this distance pass.
- Notes:
  - Contract card: `docs/reference/physx/contracts/distance-closest-point.md`
  - Study map: `docs/reference/physx/study/distance-closest-point-study-map.md`
  - This pass intentionally separates point-distance semantics from sweep TOI and MTD penetration semantics.

### 2026-05-08 - Raw intersection

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`
- Searched roots:
  - `.ref/PhysX_4.0/physx/include/geometry`
  - `.ref/PhysX_4.0/physx/source/geomutils`
- Search terms: `GuIntersection`, `GuRaycastTests`, `GuOverlapTests`, `raycast_box`, `raycast_sphere`, `raycast_capsule`, `intersectRayAABB`, `intersectRayTriangle`, `intersectRaySphere`, `intersectRayCapsule`, `intersectSphereBox`, `intersectCapsuleTriangle`, `GeomOverlapCallback`
- Located files:
  - `.ref/PhysX_4.0/physx/source/geomutils/include/GuRaycastTests.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuRaycastTests.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuOverlapTests.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuOverlapTests.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayBox.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayBox.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayTriangle.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRaySphere.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRaySphere.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayCapsule.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayCapsule.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionSphereBox.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionSphereBox.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionCapsuleTriangle.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionCapsuleTriangle.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionBoxBox.cpp`
- Missing files:
  - None for this raw intersection pass.
- Notes:
  - Contract card: `docs/reference/physx/contracts/intersection-raw.md`
  - Study map: `docs/reference/physx/study/intersection-raw-study-map.md`
  - Mesh traversal and ordering were intentionally left for a mesh-specific pass.

### 2026-05-08 - Sweep TOI / hit normal

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`
- Searched roots:
  - `.ref/PhysX_4.0/physx/include/geometry`
  - `.ref/PhysX_4.0/physx/source/geomutils`
- Search terms: `PxGeometryQuery::sweep`, `GeomSweepFuncs`, `gGeomSweepFuncs`, `sweepCapsule_BoxGeom`, `gjkRaycastPenetration`, `hit.distance`, `hit.normal`, `toi`, `setInitialOverlapResults`, `setupSweepHitForMTD`, `initialOverlap`, `SweepCapsuleMeshHitCallback`
- Located files:
  - `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepTriangleUtils.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepConvexTri.h`
- Missing files:
  - None for this sweep TOI / hit normal pass.
- Notes:
  - Contract card: `docs/reference/physx/contracts/sweep-toi-hit-normal.md`
  - Study map: `docs/reference/physx/study/sweep-toi-hit-normal-study-map.md`
  - This pass intentionally separates primitive sweep writeback from full SceneQuery filtering and mesh traversal ordering.

### 2026-05-08 - Capsule-triangle sweep

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`
- Searched roots:
  - `.ref/PhysX_4.0/physx/source/geomutils/src/sweep`
  - `.ref/PhysX_4.0/physx/source/geomutils/src`
- Search terms: `sweepCapsuleTriangles_Precise`, `intersectCapsuleTriangle`, `sweepSphereVSTri`, `setInitialOverlapResults`, `computeSphereTriImpactData`, `shouldFlipNormal`, `computeAlignmentValue`, `keepTriangle`, `distanceSegmentTriangleSquared`, `getInitIndex`
- Located files:
  - `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepSphereTriangle.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepSphereTriangle.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepTriangleUtils.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepTriangleUtils.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionCapsuleTriangle.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionCapsuleTriangle.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistanceSegmentTriangle.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistanceSegmentTriangle.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistanceSegmentTriangleSIMD.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionTriangleBox.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuInternal.h`
- Missing files:
  - None for this capsule-triangle sweep pass.
- Notes:
  - Contract card: `docs/reference/physx/contracts/capsule-triangle-sweep.md`
  - Study map: `docs/reference/physx/study/capsule-triangle-sweep-study-map.md`
  - This pass covers the primitive helper only. Mesh candidate traversal and SceneQuery filtering are separate topics.

### 2026-05-08 - Initial overlap / MTD

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`
- Searched roots:
  - `.ref/PhysX_4.0/physx/include/geometry`
  - `.ref/PhysX_4.0/physx/source/geomutils/src`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh`
- Search terms: `computePenetration`, `GeomMTDFunc`, `gGeomMTDMethodTable`, `computeMTD`, `validateDepth`, `manualNormalize`, `eMTD`, `setupSweepHitForMTD`, `computeCapsule_TriangleMeshMTD`, `computeSphere_SphereMTD`, `SweepCapsuleMeshHitCallback::finalizeHit`
- Located files:
  - `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepMTD.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp`
- Missing files:
  - None for this initial-overlap / MTD pass.
- Notes:
  - Contract card: `docs/reference/physx/contracts/initial-overlap-mtd.md`
  - Study map: `docs/reference/physx/study/initial-overlap-mtd-study-map.md`
  - This pass separates public `computePenetration` depth semantics from sweep `eMTD` `PxSweepHit` semantics.

### 2026-05-08 - Mesh sweeps / ordering

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`
- Searched roots:
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh`
  - `.ref/PhysX_4.0/physx/source/geomutils/src`
- Search terms: `sweepTriangleMesh`, `sweepCapsule_MeshGeom`, `sweepBox_MeshGeom`, `sweepConvex_MeshGeom`, `SweepShapeMeshHitCallback`, `processHit`, `finalizeHit`, `mInitialOverlap`, `mBestDist`, `shrunkMaxT`, `eMESH_ANY`, `gMidphaseCapsuleSweepTable`, `gMidphaseBoxSweepTable`, `gMidphaseConvexSweepTable`, `sweepCapsule_MeshGeom_RTREE`, `sweepCapsule_MeshGeom_BV4`
- Located files:
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepMesh.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuSweepsMesh.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseRTree.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp`
- Missing files:
  - None for this mesh sweep ordering pass.
- Notes:
  - Contract card: `docs/reference/physx/contracts/mesh-sweeps-ordering.md`
  - Study map: `docs/reference/physx/study/mesh-sweeps-ordering-study-map.md`
  - This pass covers mesh candidate traversal and callback result selection only. SceneQuery filter/block/touch semantics are separate.

### 2026-05-08 - Query filtering

- Engine: PhysX
- Version/root:
  - `.ref/PhysX_4.0/physx` public headers
  - `.ref/PhysX_3.4/PhysX_3.4` SceneQuery implementation
- Searched roots:
  - `.ref/PhysX_4.0/physx/include`
  - `.ref/PhysX_4.0/physx/source/scenequery`
  - `.ref/PhysX_4.1/physx`
  - `.ref/PhysX_3.4/PhysX_3.4`
  - `.ref/PhysX34/PhysX_3.4`
- Search terms: `PxQueryFiltering.h`, `PxQueryReport.h`, `PxScene.h`, `NpSceneQueries.cpp`, `NpQueryShared.h`, `PxQueryHitType`, `PxQueryFilterData`, `PxQueryFilterCallback`, `preFilter`, `postFilter`, `eANY_HIT`, `eNO_BLOCK`, `eBLOCK`, `eTOUCH`, `applyFilterEquation`, `MultiQueryCallback`
- Located files:
  - `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h`
  - `.ref/PhysX_4.0/physx/include/PxQueryReport.h`
  - `.ref/PhysX_4.0/physx/include/PxScene.h`
  - `.ref/PhysX_4.1/physx/include/PxQueryFiltering.h`
  - `.ref/PhysX_4.1/physx/include/PxQueryReport.h`
  - `.ref/PhysX_4.1/physx/include/PxScene.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpQueryShared.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Include/PxQueryFiltering.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Include/PxQueryReport.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Include/PxScene.h`
- Missing files:
  - `.ref/PhysX_4.0/physx` and `.ref/PhysX_4.1/physx` checkouts do not include `Source/PhysX/src/NpSceneQueries.cpp` or an equivalent implementation file in the located source tree; public headers are present. Implementation-order evidence for this pass therefore uses the checked-out PhysX 3.4 raw source.
- Notes:
  - Contract card: `docs/reference/physx/contracts/query-filtering.md`
  - Study map: `docs/reference/physx/study/query-filtering-study-map.md`
  - This pass covers SceneQuery filtering semantics only. Geometry primitive behavior remains in geometry cards.

### 2026-05-08 - SceneQuery pipeline

- Engine: PhysX
- Version/root:
  - `.ref/PhysX_4.0/physx` public headers
  - `.ref/PhysX_3.4/PhysX_3.4` SceneQuery implementation
- Searched roots:
  - `.ref/PhysX_4.0/physx/include`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery`
- Search terms: `PxScene::raycast`, `PxScene::sweep`, `PxScene::overlap`, `NpSceneQueries::raycast`, `NpSceneQueries::multiQuery`, `MultiQueryInput`, `MultiQueryCallback`, `GeomQueryAny`, `mSQManager`, `PrunerCallback`, `Pruner::raycast`, `Pruner::overlap`, `Pruner::sweep`, `shrunkDistance`
- Located files:
  - `.ref/PhysX_4.0/physx/include/PxScene.h`
  - `.ref/PhysX_4.0/physx/include/PxQueryReport.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/include/SqSceneQueryManager.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/src/SqSceneQueryManager.cpp`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/include/SqPruner.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/src/SqAABBPruner.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/src/SqBucketPruner.h`
- Missing files:
  - `.ref/PhysX_4.0/physx` and `.ref/PhysX_4.1/physx` checked-out trees do not include `Source/PhysX/src/NpSceneQueries.cpp`; implementation-order evidence for this pass uses the checked-out PhysX 3.4 raw source.
- Notes:
  - Contract card: `docs/reference/physx/contracts/scenequery-pipeline.md`
  - Study map: `docs/reference/physx/study/scenequery-pipeline-study-map.md`
  - This pass covers the high-level SceneQuery path. Filter policy and primitive geometry behavior are in separate cards.

### 2026-05-08 - CCT query / recovery mechanics

- Engine: PhysX
- Version/root:
  - `.ref/PhysX_4.0/physx` CCT public headers
  - `.ref/PhysX_3.4/PhysX_3.4` CharacterKinematic implementation
- Searched roots:
  - `.ref/PhysX_4.0/physx/include/characterkinematic`
  - `.ref/PhysX_3.4/PhysX_3.4/Include/characterkinematic`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src`
- Search terms: `PxController::move`, `PxControllerFilters`, `setOverlapRecoveryModule`, `contactOffset`, `mOverlapRecovery`, `mPreciseSweeps`, `getSweepHitFlags`, `moveCharacter`, `doSweepTest`, `updateTouchedGeoms`, `findTouchedGeometry`, `scene->overlap`, `CollideGeoms`, `gSweepMap`, `computeMTD`, `computeTemporalBox`, `ControllerFilter`, `mCCTFilterCallback`
- Located files:
  - `.ref/PhysX_4.0/physx/include/characterkinematic/PxController.h`
  - `.ref/PhysX_4.0/physx/include/characterkinematic/PxControllerManager.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Include/characterkinematic/PxController.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Include/characterkinematic/PxControllerManager.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterControllerCallbacks.cpp`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterControllerManager.h`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterControllerManager.cpp`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctController.cpp`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctSweptCapsule.cpp`
  - `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctSweptVolume.cpp`
- Missing files:
  - `.ref/PhysX_4.0/physx` checked-out tree includes CCT public headers, but this pass did not locate a 4.0 CharacterKinematic implementation equivalent to the checked 3.4 `CctCharacterController.cpp`; implementation evidence uses checked-out PhysX 3.4 raw source.
- Notes:
  - Contract card: `docs/reference/physx/contracts/cct-query-recovery-mechanics.md`
  - Study map: `docs/reference/physx/study/cct-query-recovery-mechanics-study-map.md`
  - This pass covers CCT candidate overlap, exact local sweeps, pass decomposition, contact offset, CCT filtering, and overlap recovery. Unreal movement policy and EngineLab source were intentionally not inspected.

### 2026-05-10 - BV4 algorithm / optimization deep dive

- Engine: PhysX
- Version/root: `.ref/PhysX_4.0/physx`
- Searched roots:
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh`
  - `.ref/PhysX_4.0/physx/source/geomutils/src`
- Search terms: `BV4Tree`, `BVDataPacked`, `BVDataSwizzled`, `GU_BV4_USE_SLABS`, `processStreamOrdered`, `processStreamNoOrder`, `BV4_ProcessNodeOrdered`, `BV4_SegmentAABBOverlap`, `PNS`, `mEarlyExit`, `LeafFunction_CapsuleSweepClosest`, `setupRayData`, `ShrinkOBB`, `gMidphaseCapsuleSweepTable`, `sweepCapsule_MeshGeom_BV4`, `PX_SIMD_DISABLED`
- Located files:
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Common.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4Build.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Internal.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_AABBAABBSweepTest.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_Slabs_KajiyaOrdered.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_ProcessStreamNoOrder_SegmentAABB.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_BoxSweep_Params.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweepAA.cpp`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuBV4_CapsuleSweep_Internal.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseInterface.h`
  - `.ref/PhysX_4.0/physx/source/geomutils/src/mesh/GuMidphaseBV4.cpp`
- Missing files:
  - None for this BV4 algorithm / optimization pass.
- Notes:
  - Existing contract card: `docs/reference/physx/contracts/bv4-layout-traversal.md`
  - Existing study map: `docs/reference/physx/study/bv4-layout-traversal-study-map.md`
  - Deep-dive study map: `docs/reference/physx/study/bv4-algorithm-optimization-deep-dive.md`
  - This pass focuses on BV4 build/traversal/optimization structure for later DX12EngineLab BVH4 improvement. It did not inspect or modify EngineLab production source.
