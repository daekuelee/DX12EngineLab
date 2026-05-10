# StepDown Split Local Audit

Updated: 2026-05-10

## 0. Verdict

`StepDown` is already moving in the right semantic direction, but it is still a
single compatibility function that mixes two different jobs:

```text
Walking support maintenance  -> FindGroundWalking
Falling landing / continue   -> FindFloorForLanding
```

This split can be planned in parallel with the current `StepMove` implementation
because the audit writes docs only. The implementation should still be done in a
separate session because the current working tree already has production KCC
changes.

## 1. Snapshot / Parallel Safety

This audit is based on the current working tree snapshot. At audit time,
`git status --short` reported production KCC files already modified:

```text
 M Engine/Collision/CctTypes.h
 M Engine/Collision/KinematicCharacterControllerLegacy.cpp
 M Engine/WorldState.cpp
```

That likely comes from the parallel implementation lane. This document does not
edit those files.

Parallel-safe reason:

- This audit only writes `docs/reference/` and `docs/audits/`.
- It does not change `StepDown`, `Recover`, `SceneQuery`, or movement behavior.

Risk:

- Line numbers and exact branches must be rechecked by the implementation
  session if the parallel lane changes `KinematicCharacterControllerLegacy.cpp`
  again.

## 2. Reference Level

Reference docs:

- `docs/reference/unreal/contracts/character-movement-walking-floor-step.md`
- `docs/reference/unreal/contracts/floor-find-perch-edge.md`

Unreal is used as movement-policy reference only. This audit does not copy
Unreal behavior and does not claim full Unreal-style floor quality.

Key Unreal-backed principles:

- Floor result stores blocking/walkable/source/distance/hit data, not just a
  normal:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:563`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:568`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:573`
- Floor candidates use direction and edge tolerance:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6960`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6966`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7020`
- Perch/edge is a reduced-radius validation, not normal-only support:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h:2145`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h:2154`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7380`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7399`
- Falling landing is not identical to Walking support maintenance:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7270`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7305`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7317`

## 3. Current StepDown Shape

Current source declares the intended mode split directly:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:604`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:608`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:610`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:614`

Current `StepDown` owns:

- pose mutation: `m_currentPosition`
- mode mutation: `SetModeWalking`, `SetModeFalling`
- support state: `groundNormal`, `onGround` mirror through mode setters
- debug: `floorSemantic`, `floorSource`, `floorRejectReason`

Evidence:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:618`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:632`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:699`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:706`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:708`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:862`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:867`

The code now has useful debug enums:

- `Engine/Collision/CctTypes.h:121`
- `Engine/Collision/CctTypes.h:129`
- `Engine/Collision/CctTypes.h:137`
- `Engine/WorldState.cpp:85`
- `Engine/WorldState.cpp:103`
- `Engine/WorldState.cpp:120`

## 4. Current Responsibilities Inside One Function

| Responsibility | Current source | Intended owner |
|---|---|---|
| Record floor decision | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:632` | common helper |
| Evaluate sweep hit as walkable floor | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:644` | common helper |
| Choose Walking/Falling semantics | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:673` | dispatcher |
| Walking short support probe | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:734` | `FindGroundWalking` |
| Falling actual drop distance | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:727` | `FindFloorForLanding` |
| Walkable-only downward filter | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:750` | common query helper |
| Primary downward sweep | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:768` | both, with different semantic |
| Initial-overlap retry | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:780` | both, but should be stricter for Falling |
| Extended stepHeight support | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:790` | Walking-only |
| Overlap support fallback | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:821` | currently shared, should be reviewed |
| Latch across one-frame miss | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:834` | Walking-only |
| Walking miss -> Falling | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:862` | `FindGroundWalking` result apply |
| Falling miss -> full drop | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:867` | `FindFloorForLanding` result apply |

## 5. Main Structural Problem

The current code has `CctFloorSemantic`, but the control flow still makes one
function decide both:

```text
How far should I search?
What kind of floor am I looking for?
Which fallbacks are legal?
What mutation should happen if I miss?
```

This is why the current function is hard to reason about.

Current partial separation:

- `walkingAtEntry` selects `WalkingMaintainFloor` or `FallingLand`.
- walking uses `supportProbeDist`.
- falling uses `m_currentStepOffset + downVelocityDt`.
- extended step-height search is guarded by `walkingAtEntry`.
- latch search is guarded by `walkingAtEntry`.

Evidence:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:673`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:734`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:736`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:793`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:836`

Remaining leak:

- `OverlapSupport` fallback is outside the `walkingAtEntry` branch.
- Because `makeSupportDecision` uses `snapSemantic`, a Falling tick can accept
  overlap support as `FallingLand`.

Evidence:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:681`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:684`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:821`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:826`

That may be useful as a compatibility landing fallback, but it is not the same
as Unreal-style valid landing. It has no lower-hemisphere, edge tolerance,
perch, or floor distance validation.

## 6. Proposed Semantic Split

### 6.1 Common Result

First implementation should keep a local result type equivalent to current
`FloorDecision`.

Minimum fields:

```text
semantic
source
rejectReason
accepted
hit
delta
dist
safeT
```

Do not add full Unreal `floorDist`, `lineDist`, `perch`, or support point in the
first split. The split is about ownership, not full floor quality.

### 6.2 `FindGroundWalking`

Meaning:

```text
Maintain existing Walking support.
If support is not found, report miss so caller can switch to Falling.
Do not apply actual falling displacement.
Do not consume Falling landing semantics.
```

Allowed branches:

- support probe using `supportProbeDist`
- primary walkable sweep
- initial-overlap retry as Walking snap/latch
- extended step-height support fallback
- overlap support fallback as compatibility
- one-frame latch fallback

Not allowed:

- full falling drop
- `FallingLand`
- `FallingContinue`
- applying vertical velocity distance as floor reach

Current code that belongs here:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:734`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:768`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:780`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:790`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:821`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:834`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:862`

### 6.3 `FindFloorForLanding`

Meaning:

```text
Validate whether this Falling tick lands on walkable floor.
If no valid landing, return miss so caller applies actual downward movement and
stays Falling.
```

Allowed branches:

- actual downward distance from vertical velocity
- primary walkable sweep
- initial-overlap retry only as landing compatibility
- optional overlap support fallback only if explicitly labelled as
  compatibility, not Unreal-equivalent landing

Not allowed:

- extended walking step-height search
- walking latch
- speculative snap to keep grounded

Current code that belongs here:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:727`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:736`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:768`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:780`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:867`

Current ambiguous branch:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:821`

`OverlapSupport` during Falling should be kept only if the implementation names
it as `FallingLand` compatibility and tests it. It should not be sold as
Unreal-style `IsValidLandingSpot`.

## 7. Next Implementation Plan

Recommended implementation order:

1. Keep `StepDown(float dt)` as the only class method for now.
2. Inside `StepDown`, keep common local helpers:
   - `recordFloorDecision`
   - `evaluateSweepFloor`
   - `applyAcceptedFloor`
3. Introduce two local lambdas first:
   - `findGroundWalking() -> FloorDecision`
   - `findFloorForLanding() -> FloorDecision`
4. `StepDown` becomes a dispatcher:
   - ascending: record skip, `SetModeFalling`, return
   - if `walkingAtEntry`: call `findGroundWalking`
   - else: call `findFloorForLanding`
   - accepted: call `applyAcceptedFloor`
   - walking miss: record miss, `SetModeFalling`
   - falling miss: apply `downDelta`, `SetModeFalling`, `fullDrop = true`
5. After tests pass, optionally promote lambdas to private methods in a later
   cleanup session.

Why local lambdas first:

- Avoids changing `KinematicCharacterControllerLegacy.h`.
- Avoids new public/private API churn while the parallel implementation lane is
  already dirty.
- Keeps rollback small.

## 8. Out Of Scope For This Split

Do not add in this implementation:

- Unreal-style reduced-radius perch.
- `IsWithinEdgeTolerance` equivalent.
- `floorDist` / `lineDist` band maintenance.
- `IsValidLandingSpot` equivalent.
- MTD-like recovery.
- SceneQuery multi-hit.
- time-budget continuation after Walking/Falling transition.

These require later contracts/tests.

## 9. Acceptance Criteria

Trace-level criteria:

- Walking support miss records `floorSemantic=WalkingSnapOrLatch` and then
  switches to Falling without applying full falling drop in the same branch.
- Falling miss records `floorSemantic=FallingContinue` and applies actual
  downward movement.
- `WalkingMaintainFloor` and `FallingLand` are no longer produced by the same
  interleaved block of code.
- `OverlapSupport` during Falling is either absent or clearly labelled as
  compatibility in the code and trace.

Manual cases:

- flat ground walk remains stable.
- walking off an edge transitions to Falling without teleporting down by
  `stepHeight`.
- falling onto flat floor lands.
- jump into wall / fake floating case is observed, but not claimed fixed.
- cube top / seam cases are observed, but perch/edge correctness is out of
  scope.

## 10. Next Implementation Prompt

```text
Implement the StepDown split from
docs/audits/kcc/09-stepdown-split-local-audit.md.

Modify only KCC production files needed for StepDown structure.
Do not modify SceneQuery, Recover, StepMove, or StepUp behavior.

Goal:
Keep StepDown(float dt) as the class method, but split its internal control flow
into local helpers:
- findGroundWalking() -> FloorDecision
- findFloorForLanding() -> FloorDecision

Walking support maintenance may use supportProbeDist, extended stepHeight
support, overlap support compatibility, and latch.
Falling landing may use actual downward velocity distance and landing sweep.
It must not use Walking latch or extended stepHeight support.

Do not add Unreal perch, edge tolerance, floorDist, time-budget continuation, or
MTD-like recovery in this session.

Acceptance:
Trace must clearly separate WalkingMaintainFloor / WalkingSnapOrLatch from
FallingLand / FallingContinue, and the code should make illegal fallback sharing
hard to reintroduce.
```
