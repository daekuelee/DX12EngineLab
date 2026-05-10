# Unreal CharacterMovement Walking / Floor / Step Contract

Updated: 2026-05-10

## Scope

This document records raw-source-backed Unreal movement-policy contracts for
DX12EngineLab KCC work.

Reference level:

- Reference engine: Unreal
- Reference version/root: `.ref/UnrealEngine/.ref/UnrealEngine`
- Trust level: raw-source-verified
- Review status: pending

This is a movement-policy reference. It is not a low-level geometry oracle, and
it is not a license to copy Unreal code.

## Located Source

- `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp`
- `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h`

## Contract: Walking Mode Owns Floor Movement

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5554`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5587`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5600`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5608`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5632`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5645`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5667`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5675`
- Inputs:
  - Current movement mode, velocity, acceleration, floor state, and remaining tick time.
- Outputs:
  - Ground movement, possible mode transition, and refreshed floor state.
- Invariant:
  - Walking movement is mode-owned. Ground movement is not just an arbitrary
    lateral sweep; it is tied to current floor state and floor refresh.
- Edge cases:
  - If movement mode changes during walking movement, Unreal refunds unused time
    and re-enters physics through the new mode.
- Rejection/filtering rules:
  - Walking mode maintains horizontal ground velocity before floor movement.
- Why this matters:
  - A local KCC `StepMoveWalking` stage should not silently own Falling,
    landing, floor search, and recovery semantics at the same time.
- Possible EngineLab audit question:
  - Does `StepMove` run as a walking-only response stage, or does it still run
    in Falling / landing paths?

## Contract: MoveAlongFloor Starts From A Walkable Floor

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5457`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5459`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5464`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5467`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5468`
- Inputs:
  - Movement velocity, delta time, and the cached current floor.
- Outputs:
  - Movement along floor, with optional step-down result.
- Invariant:
  - If the current floor is not walkable, floor movement does not proceed.
  - The movement delta is first converted into ground/ramp movement using the
    current floor before the sweep.
- Edge cases:
  - Floor movement can hit another ramp or a barrier after the initial movement.
- Rejection/filtering rules:
  - A walkable floor is a precondition for `MoveAlongFloor`.
- Why this matters:
  - A local walking move should classify support-like / walkable hits as floor
    policy, not as a generic wall-blocking slide by default.
- Possible EngineLab audit question:
  - Does `StepMove` treat a walkable/support-like raw hit as a floor/ramp fact,
    or can it become `PositiveLateralBlocker`?

## Contract: Walkable Ramp Hit Is Not Immediately Wall Slide

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5394`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5399`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5401`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5415`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5482`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5486`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5490`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5492`
- Inputs:
  - Desired movement delta and a hit that may represent a floor/ramp.
- Outputs:
  - Ground movement delta projected along the floor/ramp when the hit is
    walkable.
- Invariant:
  - Walkable ramp/floor hits are interpreted through ground movement before
    barrier slide.
- Edge cases:
  - A second movement after ramp projection can still produce a blocking hit.
- Rejection/filtering rules:
  - The hit must be walkable and not a line-trace floor result for ramp
    projection.
- Why this matters:
  - If a KCC converts every hit with a lateral component into a wall normal, it
    will incorrectly block or deflect on floor/support geometry.
- Possible EngineLab audit question:
  - Does `StepMove` reject `rawWalkable` before building a lateral response
    normal?

## Contract: Non-Walkable Blocking Hit Owns Step Or Slide

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5498`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5500`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5504`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5507`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5508`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5526`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5529`
- Inputs:
  - A blocking hit after floor/ramp movement.
- Outputs:
  - Attempted `StepUp`, or fallback slide when step-up is not possible.
- Invariant:
  - Barrier handling is reactive to a blocking hit. It is not driven solely by
    lateral input or by a support normal.
- Edge cases:
  - Step-up can fail and fall back to slide.
- Rejection/filtering rules:
  - `CanStepUp` gates step attempt.
- Why this matters:
  - A local KCC should not treat walkable/support hits as the same category as
    non-walkable lateral blockers.
- Possible EngineLab audit question:
  - Is `PositiveLateralBlocker` reserved for non-walkable movement blockers?

## Contract: Walkability Is A Floor Policy

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6883`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6885`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6891`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6898`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6908`
- Inputs:
  - Blocking hit and walkable floor threshold.
- Outputs:
  - Boolean walkability classification.
- Invariant:
  - Walkability is about whether the surface may be used as floor, not whether
    a hit should be treated as a lateral blocker.
- Edge cases:
  - Non-hit / start-penetrating cases are not walkable floor.
  - Vertical surfaces are rejected.
- Rejection/filtering rules:
  - Surface-specific slope override can modify the threshold.
- Why this matters:
  - A local `rawWalkable` flag must not be only a debug flag if the movement
    stage needs to distinguish floor/support from wall.
- Possible EngineLab audit question:
  - Is `rawWalkable` used to prevent floor-like hit normals from feeding wall
    slide?

## Contract: Floor Search Uses Distance, Edge Tolerance, And Perch

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6949`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6960`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6966`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6971`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7004`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7020`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7040`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7044`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7100`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7161`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7174`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7194`
- Inputs:
  - Capsule location, floor sweep/line distances, optional downward sweep.
- Outputs:
  - Floor result with blocking, walkable, distance, and perch interpretation.
- Invariant:
  - Floor support is not reducible to normal.y alone.
  - Floor search validates downward direction, edge tolerance, floor distance,
    walkability, and perch.
- Edge cases:
  - Edge-adjacent hits may force a smaller capsule retry.
  - No valid perch invalidates the floor.
- Rejection/filtering rules:
  - A supplied downward sweep is accepted only when it is downward/vertical and
    within edge tolerance.
- Why this matters:
  - A small `StepMove` patch can reject walkable support hits from blocker
    response, but full floor correctness belongs to a later floor/perch audit.
- Possible EngineLab audit question:
  - Does `StepDown` distinguish support location, edge/perch, floor distance,
    and landing policy?

## Contract: StepUp Is A Transaction

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7450`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7454`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7485`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7511`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7517`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7528`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7573`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7586`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7595`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7617`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7636`
- Inputs:
  - Gravity direction, remaining movement delta, and the blocking hit that
    triggered step-up.
- Outputs:
  - Committed step-up movement or full revert.
- Invariant:
  - Step-up is a scoped transaction: up, forward, down, validate, then commit
    or revert.
- Edge cases:
  - Start penetration, too-high result, unwalkable upward result, edge tolerance
    failure, and failed floor validation reject the transaction.
- Rejection/filtering rules:
  - `CanStepUp`, max step height, edge tolerance, walkability, and final floor
    validation all gate success.
- Why this matters:
  - Step-up should be a later reactive transaction after a real blocker, not a
    catch-all fix for support-like normals consumed by `StepMove`.
- Possible EngineLab audit question:
  - Is local step-up driven by a real non-walkable lateral blocker, and does it
    commit/revert as one transaction?

## EngineLab Follow-Up Questions

- Can `StepMove` be narrowed to `StepMoveWalking` and consume only non-walkable
  lateral blockers?
- Does `rawWalkable` currently prevent `PositiveLateralBlocker`, or is it only
  debug data?
- Can a small local patch reject walkable/support-like hits before
  `SlideAlongNormal` without changing `StepDown`, `Recover`, or `SceneQuery`?
- Which later session should own floor distance, perch, and landing/falling
  split?
