# SceneQuery Invariant Audit: Result Contract

Updated: 2026-05-10

## 1. Scope

This audit covers the current working-tree result types and public closest-sweep
path:

- `Engine/Collision/SceneQuery/SqTypes.h`
- `Engine/Collision/SceneQuery/SqQueryLegacy.h`
- `Engine/Collision/SceneQuery/SqNarrowphaseLegacy.h`

Reference contracts:

- `docs/reference/physx/contracts/sweep-toi-hit-normal.md`
- `docs/reference/physx/contracts/initial-overlap-mtd.md`
- `docs/reference/physx/contracts/geometry-query-api.md`

## 2. Query Result Contract

| Field | Meaning in current code | Evidence | Ambiguity | Required invariant |
|---|---|---|---|---|
| `Hit.hit` | Whether a closest sweep candidate survived broadphase, narrowphase, filter, time-window, and `BetterHit`. | `SqTypes.h:113-124`, `SqQueryLegacy.h:193-205` | None for closest sweep. There is no all-hits/touch-buffer result. | `hit == false` means every other field is default/ignored. |
| `Hit.t` | Normalized sweep fraction in `[0,1]`; initial overlap is reported as `t == 0`. | `SqTypes.h:115`, `SqQueryLegacy.h:191-198`, `SqNarrowphaseLegacy.h:185-193`, `SqNarrowphaseLegacy.h:411-417`, `SqNarrowphaseLegacy.h:617-638` | `t == 0` is not enough to distinguish initial overlap from a very early impact. | Callers must use `startPenetrating`, not `t == 0`, for penetration semantics. |
| `Hit.normal` | Hit normal returned by primitive sweep, normalized and flipped to oppose motion in several paths. | `SqTypes.h:118`, `SqNarrowphaseLegacy.h:271-272`, `SqNarrowphaseLegacy.h:437`, `SqNarrowphaseLegacy.h:491` | For capsule-box, it can be a proxy/extruded-triangle normal rather than a source box face normal. | State whether normal is source-surface or proxy normal per primitive path. |
| `Hit.featureId` | Packed feature information from primitive/proxy sweep. | `SqTypes.h:119`, `SqQueryLegacy.h:193-204`, `SqNarrowphaseLegacy.h:481-482`, `SqNarrowphaseLegacy.h:679-680` | Packing differs by source: tri sweep packs `(prismFace << 8) | sphereTriFeature`; box sweep packs `(triId << 16) | (prismFace << 8) | sphereTriFeature`. | Feature id must declare source/proxy ownership before KCC or debug code interprets it. |
| `Hit.startPenetrating` | Explicit initial-overlap flag. | `SqTypes.h:120-123`, `SqQueryLegacy.h:172-204`, `SqNarrowphaseLegacy.h:63-80` | Good direction, but public docs/audit index were stale before this report. | This is the only initial-overlap boolean. |
| `Hit.penetrationDepth` | Depth computed for local initial-overlap cases. | `SqTypes.h:123`, `SqNarrowphaseLegacy.h:190-192`, `SqNarrowphaseLegacy.h:414-416`, `SqNarrowphaseLegacy.h:635-637` | It is attached to sweep hits, while PhysX separates public `computePenetration` MTD from normal sweep hit semantics. | Treat this as local diagnostic/recovery input, not equivalent to public MTD unless separately audited. |
| `OverlapContact.normal` | Push-out normal for overlap recovery, pointing away from primitive. | `SqTypes.h:126-131`, `SqNarrowphaseLegacy.h:732-740`, `SqNarrowphaseLegacy.h:752-770`, `SqNarrowphaseLegacy.h:827-836` | Near-zero fallback normals may be geometric support normals, not averaged multi-contact MTD. | Overlap normal must be classified separately from sweep normal. |
| `OverlapContact.depth` | Positive overlap depth. | `SqTypes.h:128`, `SqNarrowphaseLegacy.h:754`, `SqNarrowphaseLegacy.h:770`, `SqNarrowphaseLegacy.h:825` | AABB inside case uses min-axis depth plus radius; multiple-contact composition is not a SceneQuery result. | Recovery must not treat one contact as a complete manifold solution. |

## 3. Hit Normal and TOI Semantics

Current local sweep uses normalized `t` rather than PhysX's public linear
`distance` field. That is acceptable only if every caller understands `t` is a
fraction of `SweepCapsuleInput::delta`. PhysX public sweep takes `unitDir` plus
linear `maxDist`, then writes linear `hit.distance`; the local equivalent is
`position = start + delta * t`. Reference: `sweep-toi-hit-normal.md:19-29`.

Initial overlap is now explicitly represented by `startPenetrating`; this is
the correct direction. PhysX non-MTD initial overlap convention reports
`distance = 0` and `normal = -unitDir`, while MTD is a separate path that may
write a depenetration result. Reference: `sweep-toi-hit-normal.md:27-31`,
`initial-overlap-mtd.md:23-33`.

Current code preserves the non-MTD initial overlap normal policy through
`InitialOverlapNormal(delta, fallback)`, which prefers `-delta` when possible.
Evidence: `SqNarrowphaseLegacy.h:83-90`.

The remaining ambiguity is that local sweep `Hit` now carries both
`startPenetrating` and `penetrationDepth`. That is useful for KCC recovery
experiments, but it is not equivalent to PhysX public `computePenetration`
because PhysX public MTD returns a direction/depth for `geom0`, while sweep MTD
writes a sweep-hit-shaped result with different distance conventions. Reference:
`initial-overlap-mtd.md:23-33`, `initial-overlap-mtd.md:43-45`.

## 4. Broadphase / Narrowphase Boundary

`Hit` is selected in `SweepCapsuleClosestHit_Fast` after BVH pruning and
primitive narrowphase. Evidence: `SqQueryLegacy.h:143-148`,
`SqQueryLegacy.h:157-180`, `SqQueryLegacy.h:191-205`.

The query result contract is currently closest-block-only. There is no local
touch buffer, all-hits collector, any-hit mode, or block/touch classification.
PhysX has block/touch/any-hit semantics at the SceneQuery boundary. Reference:
`query-filtering.md:25-33`, `scenequery-pipeline.md:25-33`.

## 5. Policy Leakage

`SweepFilter` comments still document movement-stage usage: `StepUp`,
`StepMove`, and `StepDown`. Evidence: `SqTypes.h:147-163`. That makes the
result contract look SceneQuery-owned while actually encoding KCC policy.

## 6. Determinism Risks

### Issue: `featureId` is both a tie-break key and an encoded proxy feature

- Severity: P1
- EngineLab evidence: `BetterHit` uses `featureId` after feature class, type,
  and primitive index (`SqQueryLegacy.h:59-72`); capsule-triangle and capsule-box
  pack proxy ids differently (`SqNarrowphaseLegacy.h:481-482`,
  `SqNarrowphaseLegacy.h:679-680`).
- Reference evidence: PhysX capsule-triangle finalization recomputes hit data
  against the original source triangle after using extruded triangles internally
  (`capsule-triangle-sweep.md:27-33`).
- Current behavior: Equal-time hits may be ordered by generated proxy feature
  ids, not source primitive feature ids.
- Why it matters: KCC seam/corner behavior can change when proxy feature order
  changes even if the source object is the same.
- Minimal test: Two equivalent box-face diagonal configurations should produce
  the same chosen source face and normal when swept by the same capsule path.

### Issue: `penetrationDepth` exists on sweep `Hit` without a separate MTD contract

- Severity: P1
- EngineLab evidence: `Hit` stores `penetrationDepth` (`SqTypes.h:123`), and
  initial-overlap paths fill it (`SqNarrowphaseLegacy.h:190-192`,
  `SqNarrowphaseLegacy.h:414-416`, `SqNarrowphaseLegacy.h:635-637`).
- Reference evidence: PhysX separates public `computePenetration` direction/depth
  from sweep hit reporting and sweep MTD conventions (`initial-overlap-mtd.md:23-33`).
- Current behavior: A sweep result can now look like an MTD result, but only for
  local initial-overlap cases.
- Why it matters: Recovery can accidentally consume sweep overlap depth as if it
  were a full manifold or public MTD result.
- Minimal test: Initial overlap sweep and explicit overlap query against the same
  AABB should document whether their normal/depth are expected to match.

## 7. Minimal Fix Direction

Do not patch production code from this audit alone.

Smallest next steps:

1. Add deterministic tests/probes for `startPenetrating` versus `t == 0`.
2. Write a short contract saying local `Hit.t` is normalized fraction, not
   PhysX linear distance.
3. Decide whether `penetrationDepth` is temporary KCC telemetry or a stable
   SceneQuery field.
4. Clarify `featureId` ownership: source primitive feature, proxy feature, or
   explicitly opaque debug/tie-break key.
