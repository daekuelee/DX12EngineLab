# StepMove Query Boundary Plan

Updated: 2026-05-09

## 0. Verdict

`StepMove` must not consume raw `SceneQuery` hits directly.

Current implementation direction:

```text
StepMove
  -> QueryStepMoveHit
       current: closest raw sweep + stage filter + rejectInitialOverlap resweep
       future: filteredClosestHit or multi-hit collector
  -> StepMoveHitView response
```

This is not a final SceneQuery design. It is the adapter boundary that keeps
`StepMove` stable while the raw kernel remains closest-only.

## 1. StepMove Contract

```text
StepMove is a lateral movement-response stage.
StepMove asks for a stage-usable lateral blocker.
StepMove may call SlideAlongNormal only for PositiveLateralBlocker.
```

`StepMove` does not own:

- support / ramp interpretation
- ground state
- penetration recovery
- step-up legality
- raw hit ordering policy

## 2. Semantic View

`QueryStepMoveHit` converts raw `sq::Hit` into a `StepMoveHitView`.

| Kind | Meaning | StepMove response |
|---|---|---|
| `ClearPath` | no usable lateral blocker | move to target |
| `PositiveLateralBlocker` | positive TOI hit with a valid lateral response normal | safe advance, then slide |
| `NeedsRecovery` | near-zero / initial-overlap / skin contact still blocks the query | do not slide; report/stuck; recovery or MTD owns this |
| `UnsupportedForStepMove` | hit exists, but StepMove cannot convert it to a lateral movement response | do not slide; report/stuck |

Important distinction:

```text
near-zero is not a special StepMove patch.
near-zero is one raw-hit state that QueryStepMoveHit classifies before response.
```

## 3. Current Closest-Only Adapter

The current adapter uses existing raw-kernel knobs:

- `SweepFilter.refDir = -Normalize(remaining)`
- `SweepFilter.minDot = approach epsilon`
- `SweepFilter.filterInitialOverlap = true`
- first sweep with `rejectInitialOverlap=false`
- if classified as near-zero, resweep with `rejectInitialOverlap=true`
- classify the resweep result through the same `StepMoveHitView`

The verified caveat is:

```text
rejectInitialOverlap removes explicit initial-overlap candidates.
It does not guarantee that every ordinary TOI t==0 candidate disappears.
```

Therefore repeated near-zero after resweep remains `NeedsRecovery`.

## 4. Non-Goals

Session 1 does not change:

- `SceneQuery`
- `BetterHit`
- `SqNarrowphaseLegacy`
- `Recover`
- `StepDown`
- `StepUp`
- ramp movement
- MTD-like recovery
- multi-hit collection

## 5. Regression Checks

Use the manual KCC traces:

| Case | Expected Session 1 behavior |
|---|---|
| `062651` | positive raw upward normal must not create `dxIntent.y > 0` through `StepMove` |
| `072035` | t~0 contact should not slide on raw `-moveDir` normal |
| `072418` | escape/tangent case should use reject-initial resweep before declaring stuck |
| `072459` | diagonal wall contact should progress or report unsupported without raw t~0 slide |

Pass criteria:

```text
SlideAlongNormal is called only for PositiveLateralBlocker.
Near-zero contacts do not push position in StepMove.
Near-zero contacts do not slide on raw normals.
```

## 6. Session 1.1 Final Classification Trace

Session 1.1 adds observation-only final query classification.
It does not change `StepMove` behavior.

Trace terminology:

```text
stepMoveFirst* = the first raw SceneQuery hit observed by QueryStepMoveHit
stepMoveLast*  = the final StepMoveHitView consumed by StepMove response
```

This distinction is required because the closest-only adapter may resweep with
`rejectInitialOverlap=true` after an initial near-zero hit. A trace that only
records the first raw hit cannot tell whether the final result was:

- `ClearPath`
- `PositiveLateralBlocker`
- `NeedsRecovery`
- `UnsupportedForStepMove`

The required trace fields are:

- final query kind
- reject reason
- resweep-used flag
- final hit TOI / normal / lateral normal / index
- final approach dot
- final near-zero / walkable / lateral-normal flags
- per-tick counts for clear path, positive blocker, needs recovery, and
  unsupported StepMove query results

## 7. Session 2A SceneQuery Hit Semantics

Session 2A moves the raw-kernel meaning split into `sq::Hit`.
It does not change `StepMove` policy yet.

New raw facts:

```text
sq::Hit::startPenetrating
sq::Hit::penetrationDepth
```

Required semantics:

```text
skin/contactOffset is a movement pullback margin.
skin/contactOffset is not a penetration flag.
t == 0 does not imply startPenetrating.
startPenetrating means the query shape was initially overlapping the primitive.
penetrationDepth is only meaningful when startPenetrating is true.
rejectInitialOverlap rejects explicit startPenetrating candidates, not every
ordinary positive-movement hit whose TOI is near zero.
```

Trace additions:

```text
stepMoveFirstStartPen / stepMoveFirstPenDepth
stepMoveLastStartPen  / stepMoveLastPenDepth
```

Follow-up for Session 2B:

```text
QueryStepMoveHit should classify recovery ownership from startPenetrating,
not from near-zero TOI alone.
Near-zero positive TOI must remain available for lateral movement policy when
it is approaching and has a usable lateral blocker normal.
```

## 8. Session 2B StepMove Skin-Band Classification

Session 2B applies the Session 2A raw-hit split at the StepMove boundary.

Required semantics:

```text
raw.startPenetrating == true  -> NeedsRecovery / StartPenetrating.
rawNearZero                   -> trace/debug flag only.
rawNearZero != recovery ownership.
skin/contactOffset            -> movement pullback margin only.
```

StepMove classification:

```text
no hit                         -> ClearPath
startPenetrating               -> NeedsRecovery
non-penetrating lateral blocker -> PositiveLateralBlocker
non-lateral/support/tangent hit -> ClearPath
```

The closest-only adapter may still perform a reject-initial resweep when the
first raw hit is `startPenetrating`. The resweep result is then classified with
the same rules; if it returns no movement blocker, escape movement is allowed.
