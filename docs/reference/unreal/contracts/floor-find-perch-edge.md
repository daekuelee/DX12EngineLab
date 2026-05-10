# Unreal Floor Find / Perch / Edge Contract

Updated: 2026-05-10

## Scope

This document records raw-source-backed Unreal floor finding policy for
DX12EngineLab KCC `StepDown` split work.

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

## Contract: Floor Result Stores More Than A Normal

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:563`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:565`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:566`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:568`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:573`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:590`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:591`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:592`
- Inputs:
  - Blocking sweep or line hit.
- Outputs:
  - Floor result containing blocking state, walkable state, sweep/line source,
    floor distance, line distance, and hit data.
- Invariant:
  - Floor support is not only `normal.y >= threshold`; it also carries distance
    and source semantics.
- Edge cases:
  - Line-trace floor can override parts of the sweep hit while preserving sweep
    distance context.
- Why this matters:
  - A local `StepDown` split needs a `CctFloorResult`-like semantic object
    before it can cleanly separate Walking support from Falling landing.
- Possible EngineLab audit question:
  - Does local `FloorDecision` carry enough data to be promoted into a stable
    floor result?

## Contract: Downward Floor Sweep Validates Direction And Edge Tolerance

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6941`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6944`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6949`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6960`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6961`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6962`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6966`
- Inputs:
  - Capsule location, test impact point, capsule radius, optional downward sweep.
- Outputs:
  - Accepted or rejected floor hit.
- Invariant:
  - A floor candidate from a supplied sweep must be downward, vertical, and
    within edge tolerance before it is trusted as floor.
- Edge cases:
  - Edge/cusp hits are not accepted merely because the normal is walkable.
- Why this matters:
  - A local `FindGroundWalking` cannot be equivalent to Unreal `FindFloor` if it
    only checks `IsWalkable(normal)`.
- Possible EngineLab audit question:
  - Does local `StepDown` know where the support point is relative to the
    capsule radius?

## Contract: Floor Sweep Uses Shrink / Retry For Edge Ambiguity

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7004`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7007`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7013`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7016`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7020`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7022`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7026`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7034`
- Inputs:
  - Capsule location, sweep distance, sweep radius.
- Outputs:
  - Floor hit after regular or reduced-radius sweep.
- Invariant:
  - Edge-adjacent hits may require a smaller-radius retry before being used as
    floor.
- Edge cases:
  - Start penetration and outside-edge-tolerance cases go through alternate
    sweep handling.
- Why this matters:
  - A local `StepDown` split should not claim full edge/perch quality until it
    has a reduced-radius/perch policy.
- Possible EngineLab audit question:
  - Should current `OverlapSupport` remain a compatibility fallback until a
    reduced-radius floor probe exists?

## Contract: Floor Distance Has A Valid Range

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:89`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:90`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7040`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7041`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7115`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7117`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7178`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7179`
- Inputs:
  - Floor hit time, trace distance, shrink height, movement mode.
- Outputs:
  - Floor distance and floor adjustment target.
- Invariant:
  - Floor support is maintained within a small distance band, not by snapping to
    every walkable hit without distance semantics.
- Edge cases:
  - Negative floor distances are allowed within limits to pull out of shallow
    penetration.
- Why this matters:
  - Local `FindGroundWalking` should eventually carry `floorDist` separately
    from `safeT`, but a first split can keep existing `safeT` behavior.
- Possible EngineLab audit question:
  - Does current `FloorDecision` distinguish floor distance from movement
    fraction?

## Contract: Perch Is A Reduced-Radius Validation

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h:2145`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h:2154`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7161`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7166`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7174`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7341`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7352`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7367`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7380`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7397`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7399`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7403`
- Inputs:
  - Previous floor hit, reduced test radius, max floor distance.
- Outputs:
  - Accepted or rejected perch floor result.
- Invariant:
  - Perch answers whether the capsule can stand when only edge support is
    available; it is not the same as normal walkability.
- Edge cases:
  - If the reduced-radius test cannot find a walkable floor within max distance,
    floor support is invalid.
- Why this matters:
  - Local seam/floor-edge issues cannot be fully solved by the StepDown split
    alone.
- Possible EngineLab audit question:
  - Which local bugs must remain out of scope until a reduced-radius/perch probe
    exists?

## Contract: Landing Needs A Separate Valid-Landing Check

- Reference engine: Unreal
- Trust level: raw-source-verified
- Review status: pending
- Source:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7270`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7273`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7282`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7289`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7297`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7305`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7308`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7317`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7320`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7324`
- Inputs:
  - Falling hit, capsule location, movement delta.
- Outputs:
  - Valid landing or continue falling.
- Invariant:
  - Falling landing is not identical to Walking support maintenance.
- Edge cases:
  - Edge hits may need a separate floor check before being accepted as landing.
  - Penetrating hits near vertical/overhanging walls are rejected as floor pop.
- Why this matters:
  - Local `FindFloorForLanding` should not reuse every Walking support fallback
    blindly.
- Possible EngineLab audit question:
  - Should `OverlapSupport` be legal during Falling, and if so under what
    stricter landing semantics?

## EngineLab Follow-Up Questions

- Can local `StepDown` split into `FindGroundWalking` and
  `FindFloorForLanding` without adding full perch?
- Which existing fallbacks are Walking-only?
- Which existing fallbacks can be reused for Falling landing?
- What minimal result type must be produced before mutating pose/mode?
