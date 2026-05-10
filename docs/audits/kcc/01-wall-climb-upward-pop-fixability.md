# KCC Wall-Climb / Upward-Pop Fixability Notes

Updated: 2026-05-08

## Purpose

This document records the current working explanation for seam / corner / wall upward-pop bugs in `KinematicCharacterControllerLegacy`.

It is not a patch plan by itself. It is a diagnostic anchor for later passes:

1. Instrumentation / repro.
2. One-day mitigation patch.
3. Longer SceneQuery / KCC contract refactor.

Do not claim the root cause is proven until runtime telemetry or a deterministic probe confirms which stage creates the first positive Y displacement.

## Active Code Path

`WorldState` passes a lateral-only displacement into KCC:

- `Engine/WorldState.cpp:158-160` builds `CctInput.walkMove = {walkVelX * fixedDt, 0.0f, walkVelZ * fixedDt}`.
- `Engine/WorldState.cpp:163-165` calls `m_cct->Tick(cctInput, fixedDt)`.

The active KCC tick order is:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:89-124`
  runs `PreStep`, `IntegrateVertical`, pre-sweep `Recover`, `StepUp`,
  `StepMove`, `StepDown`, post-sweep `Recover`, then `Writeback`.

Important implication:

- Any Y displacement during lateral wall movement is created inside KCC, not by `WorldState`'s `walkMove`.

## Example 1: Edge / Corner / Proxy Normal Enters StepMove Near-Zero Push

### What happens in motion terms

Expected wall-slide behavior:

```text
Input: move laterally into or along a cube wall.
Expected response: block or slide in XZ, do not move upward.
```

This expectation is valid only when the response normal used for wall motion is lateral:

```cpp
wallNormal = {-1, 0, 0};
pos += wallNormal * margin; // pos.y unchanged
```

But a contact near an edge or corner can produce a feature/closest-pair normal:

```text
capsule point = (4.8, 3.2, 0)
edge point    = (5.0, 3.0, 0)
delta         = (-0.2, +0.2, 0)
normal        = normalize(delta) = (-0.707, +0.707, 0)
```

This normal is not necessarily mathematically wrong. It is wrong only if KCC treats it as a lateral wall-response normal without policy conversion.

### Where the current code compensates

The normal `StepMove` hit path explicitly removes the up component for non-walkable contacts:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:354-365`
  computes `nUp = Dot(hit.normal, m_config.up)`, removes the up component when `nUp < m_maxSlopeCos`, and sends the lateralized normal into `SlideAlongNormal`.

This is good. It says:

```text
Raw geometric normal may contain vertical noise or edge/corner contribution.
For wall slide response, use a lateral movement-response normal.
```

### Where the current code does not compensate

The near-zero / skin-dominated path bypasses that lateralization:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:331-337`
  applies `m_currentPosition = m_currentPosition + hit.normal * m_config.addedMargin`.

If the raw normal has positive Y:

```text
hit.normal = (-0.80, +0.20, -0.56)
addedMargin = 0.02
Y push = +0.004
```

The retry budget can make the small push visible:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:293` defines `maxZeroHitPushes = 8`.
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:335-342` increments `zeroHitPushes`, pushes, and retries.

### Why such normals are possible in this codebase

SceneQuery does not promise that a sweep normal is always a source cube face normal.

Evidence:

- `Engine/Collision/SceneQuery/SqNarrowphaseLegacy.h:211-224`
  edge candidates use `ct - q` from sphere center to closest edge point.
- `Engine/Collision/SceneQuery/SqNarrowphaseLegacy.h:229-239`
  vertex candidates use `ct - tri.pN`.
- `Engine/Collision/SceneQuery/SqNarrowphaseLegacy.h:488-496`
  box surfaces are decomposed into 12 triangles.
- `Engine/Collision/SceneQuery/SqNarrowphaseLegacy.h:585-599`
  each triangle is extruded and swept through proxy faces.

So the safe statement is:

```text
Runtime has not yet proven that hit.normal.y is the observed bug source.
But the API can return feature/proxy/closest-pair normals, and StepMove near-zero response consumes the raw normal as a pose correction.
```

### Minimal probe

In the near-zero branch, record:

- tick id
- current position before/after
- `hit.t`
- `hit.normal`
- `hit.featureId`
- `hit.type`
- `hit.index`
- `addedMargin`
- `Dot(hit.normal, up)`
- `zeroHitPushes`

If `posY` increases in this branch while `hit.normal.y > 0`, this example becomes a confirmed cause.

## Example 2: Initial-Overlap Hit Bypasses StepMove Approach Filter

### What StepMove is trying to do

`StepMove` builds an approach filter:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:303-311`
  sets `refDir = NormalizeSafe(remaining * -1.0f, m_config.up)`,
  `minDot = 1e-4f`, and `active = true`.

Motion meaning:

```text
Only contacts opposing the current movement direction should block this StepMove iteration.
Tangential, side, or retreating contacts should not become the chosen movement hit.
```

Example:

```text
remaining = (0, 0, +1)
refDir    = (0, 0, -1)

front wall normal = (0, 0, -1), Dot = 1.0    => accept
side wall normal  = (-1, 0, 0), Dot = 0.0    => reject
retreat normal    = (0, 0, +1), Dot = -1.0   => reject
```

### The bypass

`PassNarrowfilter` bypasses filtering for initial overlap by default:

- `Engine/Collision/SceneQuery/SqNarrowphaseLegacy.h:69-75`
  returns true when `!rejectInitialOverlap && t <= 0.0f && !filter->filterInitialOverlap`.

`StepMove` calls sweep with `rejectInitialOverlap = false`:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:313`
  calls `SweepClosest(m_currentPosition, remaining, approachFilter, false)`.

`SweepFilter::filterInitialOverlap` defaults to false:

- `Engine/Collision/SceneQuery/SqTypes.h:154-158`.

Therefore:

```text
In StepMove, a t == 0 initial-overlap hit can pass even when the approach filter would reject its normal.
```

### Why this matters

If the character is already touching, inside skin, or slightly overlapped at a wall/seam:

```text
1. StepMove asks for an approach-filtered sweep.
2. SceneQuery finds a t == 0 overlap.
3. Because filterInitialOverlap is false, the hit bypasses approach filtering.
4. StepMove sees hit.t near zero.
5. The near-zero branch applies raw hit.normal * addedMargin.
```

Relevant near-zero response:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:331-337`.

This can cause:

- side-contact jitter while sliding along a wall
- repeated zero-hit push loops
- upward motion if the accepted raw normal has positive Y
- starvation of a later, better movement hit

### Why the bypass may have existed

Bypassing initial overlap is not always wrong. Recovery-style queries may need to see overlaps even when a directional movement filter would reject them.

But in `StepMove`, the stage contract is different:

```text
StepMove is a lateral sweep/slide stage.
Penetration recovery already exists before and after StepMove.
So StepMove should not silently turn every initial overlap into movement response.
```

Pre/post recovery evidence:

- pre-sweep `Recover`: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:97-103`
- post-sweep `Recover`: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:115-121`

### Minimal probe

Record for each StepMove hit:

- `hit.t`
- whether `t <= 0`
- `Dot(hit.normal, approachFilter.refDir)`
- `approachFilter.filterInitialOverlap`
- whether the hit entered the near-zero branch

If many `t == 0` hits have `Dot(hit.normal, refDir) < minDot` but still enter near-zero response, this example is confirmed.

### One-day mitigation candidate

Set:

```cpp
approachFilter.filterInitialOverlap = true;
```

This is not a complete KCC fix. It only changes StepMove's policy so initial-overlap hits must obey the same approach normal predicate as positive-TOI hits.

## Example 3: StepUp Premature Lift Before a Real Step Candidate Exists

### What happens in motion terms

`WorldState` gives KCC lateral-only movement, but grounded ticks still get gravity:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:174-182`
  always subtracts gravity and writes `verticalOffset = verticalVelocity * dt`.

At 60 Hz with gravity around 30:

```text
verticalVelocity starts at 0 on grounded tick.
verticalVelocity -= 30 * 0.016 ~= -0.48.
```

`StepUp` then uses this gate:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:211-216`
  sets `stairLift = stepHeight` when `verticalVelocity < 0.0f && hasLateralIntent`.

So a normal grounded walking tick can become:

```text
on ground
press W near wall
gravity makes verticalVelocity negative
lateralIntent is true
StepUp applies stepHeight lift
```

`stepHeight` comes from:

- `Engine/WorldState.cpp:52-57` copies `m_config.maxStepHeight` into `cctCfg.stepHeight`.
- `Engine/WorldTypes.h:320` sets `maxStepHeight = 0.3f`.

### Why this is risky at walls and seams

The current `StepUp` happens before `StepMove`:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:108-110`
  calls `StepUp`, then `StepMove`, then `StepDown`.

This means the controller lifts before it has proven:

- a lateral blocking hit exists
- the hit is low enough to be a stair
- the top is walkable
- there is enough headroom
- the final step-down finds legal ground

The code sweeps upward for ceilings:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:226-235`
  uses a ceiling filter and calls `SweepClosest` upward.

On miss, it applies the full lift:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:255-258`.

The intended compensation is later `StepDown`:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:431-458`
  drops by `m_currentStepOffset + downVelocityDt` and applies a walkable ground hit.

But seam/corner cases can interfere:

```text
1. StepUp applies +0.3.
2. StepMove hits wall/seam/corner at the lifted position.
3. StepDown tries primary sweep, no-reject retry, extended sweep, overlap support, and latch.
4. If the legal ground is missed or safeT/backoff does not fully recover the lift, leftover Y becomes visible.
```

StepDown complexity evidence:

- primary sweep: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:468-475`
- no-reject retry: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:479-483`
- extended step-height sweep: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:486-505`
- overlap support fallback: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:508-522`
- ground latch: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:525-538`

### Minimal probe

Record per tick:

- `verticalVelocity` after `IntegrateVertical`
- `hasLateralIntent`
- `stairLift`
- `m_currentStepOffset`
- StepUp upward hit status
- position before StepUp, after StepUp, after StepDown
- `stepDownHit`, `stepDownHitTOI`, `stepDownWalkable`, `fullDrop`

If `m_currentStepOffset > 0` on wall-contact ticks before any real step candidate is identified, this example is confirmed as a contributor.

### One-day patch caution

Do not patch `StepUp` first unless telemetry proves it is the first positive-Y source.

Reason:

- A stricter StepUp gate can break stair behavior.
- A correct StepUp fix likely needs a transactional `try lateral hit -> step up -> move -> step down -> validate floor` policy, which is more than a one-day safe patch.

## Cross-Example Relationship

These examples are related but not identical:

| Example | First suspicious stage | Main failure shape | One-day patchability |
|---|---|---|---|
| Example 1 | `StepMove` near-zero branch | raw feature/proxy normal becomes pose push | Medium-High |
| Example 2 | `StepMove` sweep filtering | initial overlap bypasses approach policy | High |
| Example 3 | `StepUp` | lift happens before real step candidate is known | Medium-Low |

If upward pop is caused by Example 1 or 2, a scoped `StepMove` mitigation may reduce the symptom without touching stairs.

If upward pop is caused by Example 3, the correct fix is probably a StepUp policy redesign, not a quick SceneQuery change.

If upward pop is caused by `Recover`, all three examples are incomplete and a recovery-specific probe is required:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:567-607`
  accumulates overlap normals and applies pose-only push.

## Parallelization Assessment

### Safe to parallelize

These can run in parallel because they read different evidence or produce non-overlapping reports:

- StepMove telemetry design for Examples 1 and 2.
- StepUp telemetry design for Example 3.
- Manual repro checklist writing.
- README / portfolio wording cleanup that labels KCC as WIP.
- PhysX reference mining for query filtering and initial overlap, as a separate lane under `docs/reference/physx/`.

### Do not parallelize as independent code patches

These should not be patched independently at the same time:

- Example 1 and Example 2 patches both touch `KinematicCharacterControllerLegacy::StepMove`.
- Example 3 patch touches movement order / step policy and can invalidate StepMove observations.
- SceneQuery collector/filter refactor can change the hit stream that all KCC probes depend on.
- Recover upward clamp can hide StepMove or StepUp evidence by changing post-correction pose.

Recommended sequencing:

```text
1. Instrument StepMove and StepUp in one coherent probe.
2. If StepMove near-zero is confirmed, patch Examples 1 and 2 together in one StepMove-only change.
3. Re-test StepUp evidence after StepMove mitigation.
4. Only then decide whether StepUp policy needs a separate patch.
```

## Three-Pass Plan

### Pass 1: Instrumentation / Repro

Goal:

- Identify the first stage in a tick that creates positive Y displacement during wall/seam input.

Required observations:

- before/after position for `StepUp`, `StepMove`, `StepDown`, and post `Recover`
- StepMove near-zero hit data
- StepMove approach filter dot result
- StepUp lift gate data
- recover push vector Y contribution

Acceptance:

- A wall/seam repro can classify the first positive-Y stage as one of:
  - `StepMoveNearZero`
  - `StepUpPrematureLift`
  - `RecoverPush`
  - `StepDownIncompleteRecovery`
  - `Unknown`

### Pass 2: One-Day Mitigation

Default target if telemetry confirms StepMove:

- File: `Engine/Collision/KinematicCharacterControllerLegacy.cpp`
- Function: `KinematicCharacterControllerLegacy::StepMove`
- Patch:
  - set `approachFilter.filterInitialOverlap = true`
  - lateralize non-walkable near-zero push normals before applying `addedMargin`

Constraints:

- No public API changes.
- No SceneQuery changes.
- No `StepUp`, `StepDown`, or `Recover` behavior changes in the same patch.

Acceptance:

- Wall/seam lateral input no longer produces upward pop through `StepMove`.
- No obvious stair regression in manual stair grid.
- If bug persists, telemetry points to `StepUp` or `Recover` instead of StepMove.

### Pass 3: Contract Refactor

Goal:

- Stop overloading a single `Hit.normal` meaning across raw geometry, movement response, support, and recovery.

Future contract work:

- Define whether `Hit.normal` is source-surface, feature/proxy, or raw geometric normal.
- Add a KCC-side movement-response normal policy per stage.
- Define initial overlap semantics separately from positive-TOI sweep hits.
- Decide whether StepUp must become transactional.
- Mine PhysX / Unreal reference contracts before using their names as authority.

## Current Fixability Verdict

Current one-day verdict:

```text
B: StepMove-only mitigation is plausible, but complete KCC collision fix is not.
```

Reason:

- Examples 1 and 2 point to a narrow, revertible `StepMove` patch.
- Example 3 and `Recover` remain plausible contributors.
- Therefore the only honest claim after a one-day patch is mitigation, not complete correction.
