# PhysX Study Map: intersection-raw

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx`
- Related contract cards:
  - `docs/reference/physx/contracts/intersection-raw.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to read PhysX raw intersection helpers as primitive geometry facts. These files answer "does this ray or primitive hit?" and do not decide query filtering, mesh ordering, or KCC movement policy.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`

## Reading Order

1. `.ref/PhysX_4.0/physx/source/geomutils/src/GuRaycastTests.cpp:46` - box/sphere/capsule public raycast helpers.
2. `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayBox.cpp:371` - ray/AABB near/far interval helper.
3. `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRaySphere.cpp:38` - ray/sphere inside and max-distance behavior.
4. `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayCapsule.h:44` - ray/capsule inside and closest-hit selection.
5. `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayTriangle.h:74` - ray/triangle culling and barycentric checks.
6. `.ref/PhysX_4.0/physx/source/geomutils/src/GuOverlapTests.cpp:216` - primitive overlap callbacks and closed equality checks.
7. `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionSphereBox.cpp:36` - sphere/box clamp overlap.
8. `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionCapsuleTriangle.cpp:36` - capsule/triangle overlap axes.

## Call Path

```text
GeometryQuery::raycast
  -> gRaycastMap[geom type]
  -> raycast_box / raycast_sphere / raycast_capsule / raycast_triangleMesh
  -> raw intersectRay* helper or midphase

GeometryQuery::overlap
  -> Gu::overlap pair dispatch
  -> GeomOverlapCallback_* pair helper
  -> raw distance/intersection predicate
```

## Data / Result Flow

- Inputs: ray origin/direction/max distance or primitive geometry/poses.
- Intermediate state: rays are transformed into local primitive space where needed; overlap callbacks often convert public geometry into internal primitive helpers.
- Outputs: primitive raycast returns hit count and hit fields; overlap predicates return bool.
- Failure or rejection path: raw helpers return false/zero for misses, out-of-range ray distance, outside slabs, barycentric rejection, or separating axes.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `raycast_box` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuRaycastTests.cpp:46` | Local-space ray/AABB and single-hit result writes. |
| `raycast_sphere` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuRaycastTests.cpp:111` | Ray/sphere helper and inside normal convention. |
| `raycast_capsule` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuRaycastTests.cpp:158` | Ray/capsule helper and distance rejection. |
| `intersectRayAABB2` | `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayBox.cpp:371` | Near/far interval raw intersection. |
| `intersectRayTriangle` | `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayTriangle.h:74` | Triangle culling and barycentric gates. |
| `intersectRaySphere` | `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRaySphere.cpp:89` | Surface-offset ray/sphere wrapper. |
| `intersectRayCapsule` | `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionRayCapsule.h:44` | Capsule inside and closest root selection. |
| `GeomOverlapCallback_SphereSphere` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuOverlapTests.cpp:216` | Closed equality overlap predicate. |
| `intersectSphereBox` | `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionSphereBox.cpp:36` | Sphere center clamp to OBB. |
| `intersectCapsuleTriangle` | `.ref/PhysX_4.0/physx/source/geomutils/src/intersection/GuIntersectionCapsuleTriangle.cpp:36` | Capsule/triangle separating-axis test. |

## Contracts Produced

- `docs/reference/physx/contracts/intersection-raw.md`

## Terms To Remember

- `raw predicate`: bool geometry fact, not filtered query result.
- `closed overlap`: equality counts as overlap in common primitive predicates.
- `initial ray overlap`: starts inside sphere/capsule/box produce distance zero.
- `single primitive raycast`: box/sphere/capsule write at most one hit.
- `midphase`: mesh raycast is delegated; ordering is not local primitive logic.

## EngineLab Comparison Questions

- Does the primitive layer return raw bool/hit-count facts without filters?
- Does equality count as overlap?
- Do raycasts starting inside primitives return distance zero and `-rayDir` normal where PhysX does?
- Are mesh traversal and sorting kept outside primitive intersection helpers?

## Next Reading Path

- `sweep-toi-hit-normal`: moving primitive TOI and hit normal production.
- `mesh-sweeps-ordering`: mesh traversal and deterministic hit selection.
- `query-filtering-scenequery`: filter/block/touch result acceptance after raw primitive facts.
