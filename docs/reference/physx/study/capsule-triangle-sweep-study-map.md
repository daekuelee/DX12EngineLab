# PhysX Study Map: capsule-triangle-sweep

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx`
- Related contract cards:
  - `docs/reference/physx/contracts/capsule-triangle-sweep.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to study the primitive capsule-vs-triangle sweep path. The important shape of the algorithm is: test initial overlap against the real capsule, reduce degenerate or axis-colinear capsules to the sphere-triangle path, otherwise extrude each source triangle by the capsule segment and sweep a sphere against those generated triangles.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`

## Reading Order

1. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.h:46` - public helper contract for capsule vs triangle arrays.
2. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:70` - main implementation entry.
3. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:81` - mesh-both-sides, backface, any-hit, and initial-overlap flags.
4. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:90` - degenerate and colinear branch decision.
5. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:142` - initial overlap in the sphere-path branch.
6. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:216` - initial overlap in the extruded-mesh branch.
7. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:233` - source triangle extrusion into cap and side triangles.
8. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:278` - sphere-vs-extruded-triangle sweep and candidate keep.
9. `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:300` - final hit writeback and source-triangle contact recomputation.

## Call Path

```text
sweepCapsuleTriangles_Precise(...)
  -> reject empty triangle array
  -> derive meshBothSides / backface culling / anyHit / initial-overlap settings
  -> if capsule degenerates to sphere: sweepSphereTriangles(...)
  -> else if axis and sweep direction are colinear: copied sphere path with real capsule initial-overlap test
  -> else:
       for each source triangle
         cull backfaces and optional cullBox misses
         test initial overlap with intersectCapsuleTriangle
         extrude triangle by capsule segment
         sweep sphere against each generated triangle
         keep best candidate by distance and alignment
       recompute position on original source triangle
```

## Data / Result Flow

- `hit.distance` is the best `curT` impact distance.
- `hit.faceIndex` refers to the original source triangle index, not the generated extruded-triangle index.
- `triNormalOut` stores the selected source triangle normal or `-unitDir` for initial overlap.
- `hit.normal` is generated from sphere-triangle impact data and then flipped only for the mesh-both-sides back-face convention.
- `hit.position` is finally recomputed from moved capsule segment vs original triangle, so it is not simply the position from the generated extruded triangle.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `sweepCapsuleTriangles_Precise` | `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:70` | Main capsule-triangle sweep algorithm. |
| `intersectCapsuleTriangle` | `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:142` | Initial-overlap predicate before sweep TOI. |
| `sweepSphereVSTri` | `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:156` | Sphere path used for degenerate/colinear capsule cases. |
| `computeSphereTriImpactData` | `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepTriangleUtils.cpp:45` | Computes hit point and normal from sphere center at impact. |
| `shouldFlipNormal` | `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepTriangleUtils.h:251` | Implements mesh-both-sides back-face normal flip. |
| `computeAlignmentValue` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuInternal.h:89` | Scores triangle normal alignment against sweep direction. |
| `keepTriangle` | `.ref/PhysX_4.0/physx/source/geomutils/src/GuInternal.h:111` | Candidate tie-break by distance and alignment. |
| `distanceSegmentTriangleSquared` | `.ref/PhysX_4.0/physx/source/geomutils/src/sweep/GuSweepCapsuleTriangle.cpp:325` | Recomputes final position on original source triangle. |

## Contracts Produced

- `docs/reference/physx/contracts/capsule-triangle-sweep.md`

## Terms To Remember

- `source triangle`: the triangle in the mesh or triangle array.
- `extruded triangle`: generated cap/side triangle swept by a sphere.
- `curT`: current best impact distance.
- `meshBothSides`: query flag that can allow back-face hits and may flip normals.
- `anyHit`: stops after the first kept candidate, not necessarily global closest.
- `cachedIndex`: tries a prior best triangle first to shrink the query early.

## EngineLab Comparison Questions

- Is local capsule-triangle sweep using real capsule initial-overlap testing before TOI?
- Does the reported face index point to the source triangle rather than a generated helper primitive?
- Does local normal orientation oppose sweep direction except for mesh-both-sides back-face rules?
- Does the final hit position come from capsule segment vs source triangle closest points?
- Is tie-breaking stable around near-equal distances and more-opposing normals?

## Next Reading Path

- `sweep-toi-hit-normal`: common public sweep and initial-overlap result semantics.
- `mesh-sweeps-ordering`: how triangle arrays are gathered and how callbacks use primitive helpers.
- `initial-overlap-mtd`: how PhysX changes initial-overlap output when MTD is requested.
