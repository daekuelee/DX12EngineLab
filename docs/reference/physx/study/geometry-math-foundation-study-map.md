# PhysX Study Map: geometry math foundation

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx`, `.ref/PhysX_4.0/pxshared`
- Related contract cards:
  - `docs/reference/physx/contracts/geometry-math-foundation.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to read the PhysX math layer that supports intersection, raw distance, and sweep code. It separates public data contracts (`PxVec3`, `PxTransform`, `PxGeometry`) from internal geometry/query helpers (`Gu::Box`, `Gu::Capsule`, `Vec3V` call sites) so later audits can compare the right layer.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`
- `.ref/PhysX_4.0/pxshared`

## Reading Order

1. `.ref/PhysX_4.0/pxshared/include/foundation/PxVec3.h:45` - public vector storage, construction, finite/unit checks, dot/cross, squared length, and normalization behavior.
2. `.ref/PhysX_4.0/pxshared/include/foundation/PxTransform.h:45` - public pose as quaternion plus translation, with transform/inverse transform and validity checks.
3. `.ref/PhysX_4.0/pxshared/include/foundation/PxQuat.h:132` - unit/sane quaternion checks and unit-quaternion rotate/rotateInv assumptions.
4. `.ref/PhysX_4.0/physx/include/geometry/PxGeometry.h:70` - public geometry is shape data without placement.
5. `.ref/PhysX_4.0/physx/include/geometry/PxBoxGeometry.h:45`, `.ref/PhysX_4.0/physx/include/geometry/PxSphereGeometry.h:46`, `.ref/PhysX_4.0/physx/include/geometry/PxCapsuleGeometry.h:55`, `.ref/PhysX_4.0/physx/include/geometry/PxTriangle.h:48` - primitive public geometry structs.
6. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:67` - public query entry points and result meanings.
7. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:100` - query implementation validation and dispatch.
8. `.ref/PhysX_4.0/physx/source/geomutils/include/GuBox.h:52`, `.ref/PhysX_4.0/physx/source/geomutils/include/GuSegment.h:45`, `.ref/PhysX_4.0/physx/source/geomutils/src/GuCapsule.h:47`, `.ref/PhysX_4.0/physx/source/geomutils/src/GuSphere.h:42` - internal primitive representations.
9. `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangle.cpp:218`, `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayBox.cpp:426`, `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp:45`, `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepTriangleUtils.cpp:70` - examples of raw distance, intersection, and sweep math using internal vector forms.

## Call Path

```text
PxGeometryQuery public API
  -> GuGeometryQuery.cpp validation
  -> dispatch by PxGeometryType
  -> public primitive data converted to Gu primitives or Ps::aos call-site data
  -> distance / intersection / sweep helper
```

Important raw entries:

```text
PxGeometryQuery::sweep
  -> validates pose0, pose1, finite direction, finite distance
  -> dispatches sphere/capsule/box through gGeomSweepFuncs

PxGeometryQuery::raycast
  -> validates finite origin/direction, pose, maxDist, and unit direction
  -> dispatches through gRaycastMap

PxGeometryQuery::pointDistance
  -> validates pose
  -> dispatches sphere/capsule/box/convex
  -> returns squared distance, 0 inside, -1 unsupported
```

## Data / Result Flow

- Inputs: `PxVec3` values, a `PxGeometry` subtype, and a `PxTransform` pose. Public query APIs require pose-explicit shape queries.
- Intermediate state: public primitives are converted into internal forms such as `Gu::Box`, `Gu::Capsule`, `Gu::Sphere`, `TriangleV`, `BoxV`, and call-site `Vec3V`/`FloatV` values.
- Outputs: raycast returns hit count, sweep returns a boolean plus `PxSweepHit`, point-distance returns squared distance and optional closest point.
- Failure or rejection path: invalid poses and invalid ray/sweep parameters are rejected before dispatch; unsupported point-distance geometry returns `-1.0`.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `PxVec3` | `.ref/PhysX_4.0/pxshared/include/foundation/PxVec3.h:45` | Public 3D vector storage and scalar math basis. |
| `PxVec3::isNormalized` | `.ref/PhysX_4.0/pxshared/include/foundation/PxVec3.h:164` | Defines finite/unit tolerance used by API validation concepts. |
| `PxTransform` | `.ref/PhysX_4.0/pxshared/include/foundation/PxTransform.h:45` | Pose carrier: quaternion plus translation. |
| `PxTransform::isValid` | `.ref/PhysX_4.0/pxshared/include/foundation/PxTransform.h:148` | Query implementations validate poses through this. |
| `PxGeometry` | `.ref/PhysX_4.0/physx/include/geometry/PxGeometry.h:70` | Public shape identity without placement. |
| `PxBoxGeometry::isValid` | `.ref/PhysX_4.0/physx/include/geometry/PxBoxGeometry.h:92` | Positive finite half extents. |
| `PxSphereGeometry::isValid` | `.ref/PhysX_4.0/physx/include/geometry/PxSphereGeometry.h:76` | Positive finite radius. |
| `PxCapsuleGeometry::isValid` | `.ref/PhysX_4.0/physx/include/geometry/PxCapsuleGeometry.h:93` | Positive finite radius and half height. |
| `PxGeometryQuery::sweep` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:100` | Public sweep validation and dispatch entry. |
| `PxGeometryQuery::raycast` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:200` | Unit direction enforcement before ray dispatch. |
| `PxGeometryQuery::pointDistance` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:221` | Squared-distance semantics and supported geometry boundary. |
| `Gu::Box` | `.ref/PhysX_4.0/physx/source/geomutils/include/GuBox.h:52` | Internal oriented box representation with world/local transforms. |
| `Gu::Segment` | `.ref/PhysX_4.0/physx/source/geomutils/include/GuSegment.h:45` | Segment basis for capsule math. |
| `Gu::Capsule` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuCapsule.h:47` | Internal capsule as segment plus radius. |
| `Gu::distancePointTriangleSquared` | `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangle.cpp:218` | Raw closest-feature distance foundation for triangle math. |
| `Gu::intersectRayAABB2` | `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayBox.cpp:426` | SIMD-style ray/AABB slab intersection foundation. |
| `sweepCapsule_BoxGeom` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuSweepTests.cpp:45` | Example sweep path converting public inputs to internal vector forms. |

## Contracts Produced

- `docs/reference/physx/contracts/geometry-math-foundation.md`

## Terms To Remember

- `pose-explicit`: geometry shape data and world placement are separate.
- `valid pose`: finite translation/quaternion plus unit quaternion.
- `sane quaternion`: looser quaternion magnitude check than `isUnit`.
- `squared distance`: distance APIs often return squared distance to avoid square root except when needed for closest point projection.
- `Vec3V`: internal PhysX vector form observed in raw call sites; definition files were not located in this checkout.
- `Gu primitive`: internal geomutils primitive such as `Gu::Box`, `Gu::Capsule`, or `Gu::Sphere`.

## EngineLab Comparison Questions

- Does the local query API keep geometry shape and world pose as separate concerns?
- Which local layer validates finite/unit direction and valid transforms before primitive math?
- Are point-distance results consistently squared distance, with `0` for inside and an explicit unsupported-geometry path?
- Are public primitive validity rules and internal helper validity rules kept separate?
- Does the local sweep path have a documented equivalent for the PhysX sphere-as-zero-height-capsule internal dispatch edge case?
- Are SIMD/internal math helpers treated as implementation detail rather than public query contract?

## Next Reading Path

- `geometry-query-api`: public `PxGeometryQuery` inputs/outputs and validation boundaries.
- `distance-closest-point`: `GuDistancePointTriangle.cpp`, segment distance, capsule/box distance helpers.
- `intersection-raw`: `GuIntersectionRayBox.cpp`, ray/triangle and primitive overlap helpers.
- `sweep-toi-hit-normal`: `GuSweepTests.cpp`, `sweep/GuSweepCapsuleTriangle.cpp`, mesh sweep helpers.
