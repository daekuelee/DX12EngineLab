# PhysX Study Map: distance-closest-point

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx`
- Related contract cards:
  - `docs/reference/physx/contracts/distance-closest-point.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to study PhysX distance as a separation query. The main rule is that public `pointDistance` returns squared separation distance, collapses inside/touching to zero, and only makes closestPoint meaningful for strictly positive results.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`

## Reading Order

1. `.ref/PhysX_4.0/physx/include/geometry/PxGeometryQuery.h:180` - public pointDistance contract and return semantics.
2. `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:221` - public wrapper and geometry switch.
3. `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointSegment.h:41` - point-to-segment parameter clamp and squared residual.
4. `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointBox.cpp:34` - point-to-box local clamp and squared outside deltas.
5. `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangle.h:103` - scalar point-to-triangle closest point wrapper.
6. `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangle.cpp:218` - SIMD point-to-triangle closest-feature regions.
7. `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp:504` - point-to-convex distance through GJK.

## Call Path

```text
PxGeometryQuery::pointDistance(point, geom, pose, closestPoint)
  -> validate pose
  -> switch geom type
  -> sphere: center distance minus radius
  -> capsule: point-segment distance, then radius shell
  -> box: distancePointBoxSquared and local closest param
  -> convex: pointConvexDistance via GJK
  -> unsupported: -1.0f
```

## Data / Result Flow

- Inputs: point, public geometry, geometry pose, optional closestPoint pointer.
- Intermediate state: sphere uses center delta; capsule builds `Gu::Capsule`; box builds `Gu::Box`; convex builds a point-as-capsule GJK query.
- Outputs: squared distance, `0.0` inside/touching, `-1.0` unsupported, optional closestPoint only for separation.
- Failure or rejection path: invalid pose is rejected before geometry dispatch; unsupported geometry falls through to `-1.0f`.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `PxGeometryQuery::pointDistance` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuGeometryQuery.cpp:221` | Public distance boundary and geometry switch. |
| `distancePointSegmentSquaredInternal` | `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointSegment.h:41` | Segment parameter clamp and squared residual. |
| `distancePointBoxSquared` | `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointBox.cpp:34` | Box local-space clamp and closest parameter. |
| `distancePointTriangleSquared` | `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangle.h:103` | Scalar triangle closest point wrapper. |
| SIMD `distancePointTriangleSquared` | `.ref/PhysX_4.0/physx/source/geomutils/src/distance/GuDistancePointTriangle.cpp:218` | Vertex/edge/face closest-feature regions. |
| `pointConvexDistance` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuMTD.cpp:504` | Point-convex distance through GJK. |
| `Gu::Segment::getPointAt` | `.ref/PhysX_4.0/physx/source/geomutils/include/GuSegment.h:168` | Converts segment parameter into closest point. |

## Contracts Produced

- `docs/reference/physx/contracts/distance-closest-point.md`

## Terms To Remember

- `squared distance`: public pointDistance returns squared separation distance.
- `closed inside`: sphere/capsule use `<=` checks and return zero for touching.
- `closestPoint validity`: only meaningful when returned distance is strictly positive.
- `segment param`: clamped `t` in `[0,1]` used to reconstruct closest point.
- `closest feature`: triangle helper chooses vertex, edge, or face closest region.

## EngineLab Comparison Questions

- Does the local distance API return squared distance or linear distance?
- Does inside/touching collapse to zero without closestPoint writes?
- Does unsupported geometry return a sentinel distinct from valid zero distance?
- Does capsule distance reduce to segment distance plus radius consistently?
- Does box distance preserve local clamp then transform closest point to world?

## Next Reading Path

- `intersection-raw`: bool overlap/intersection helpers, where distance may be used only as a predicate.
- `sweep-toi-hit-normal`: sweep TOI and hit normal are not distance API semantics.
- `initial-overlap-mtd`: penetration depth and depenetration direction are separate from pointDistance.
