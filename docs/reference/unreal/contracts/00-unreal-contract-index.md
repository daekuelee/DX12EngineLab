# Unreal Movement Policy Contract Index

Updated: 2026-05-10

## Purpose

This directory is the reusable storage location for Unreal raw-source-backed
movement-policy and transform-integration contracts used by DX12EngineLab work.

Do not put EngineLab audit conclusions in this directory.
Do not copy Unreal code.
Extract behavior contracts only.

## Evidence Rule

Every accepted contract in this directory must cite raw Unreal source with
`file:line` evidence.

Allowed evidence:

1. Checked-out raw Unreal source under `.ref/UnrealEngine/.ref/UnrealEngine`.
2. Local trimmed reference files under `UnrealFork/` only when they can be
   traced back to the same raw source region.

Not enough by itself:

- `UnrealFork/*.context.md`
- `UnrealFork/*.map.md`
- `gpt.txt`
- prior chat summaries
- memory of Unreal behavior

## Current Source Discovery

Reusable source discovery is tracked in:

- `docs/reference/unreal/source-discovery.md`

Known raw source anchors:

- `PhysWalking`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5554`
- `MoveAlongFloor`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5457`
- `PhysFalling`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:4804`
- `ComputeFloorDist`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6949`
- `FindFloor`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7100`
- `IsWithinEdgeTolerance`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6941`
- `ShouldComputePerchResult`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7352`
- `ComputePerchResult`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7380`
- `StepUp`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7450`
- `SafeMoveUpdatedComponent`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/MovementComponent.cpp:558`
- `ResolvePenetrationImpl`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/MovementComponent.cpp:624`
- `CalcNewComponentToWorld_GeneralCase`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:715`
- `SetWorldTransform`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:1981`
- `SetWorldLocationAndRotation(FQuat)`: `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:2026`

These anchors are discovery entries, not accepted behavior contracts yet.

## Contract Files To Mine

Mine these files before claiming Unreal parity in KCC audits or source comments:

- `character-movement-walking.md`
  - `PhysWalking`
  - `MoveAlongFloor`
  - `ComputeGroundMovementDelta`
  - walking slide / step decision
- `character-movement-falling.md`
  - `PhysFalling`
  - falling collision response
  - landing transition
  - remaining-time continuation
- `floor-find-perch-edge.md`
  - `FindFloor`
  - `ComputeFloorDist`
  - edge tolerance
  - perch result
  - floor distance bands
- `reactive-stepup.md`
  - `CanStepUp`
  - `StepUp`
  - up / forward / down transaction
  - rollback and final floor validation
- `safemove-penetration.md`
  - `SafeMoveUpdatedComponent`
  - `ResolvePenetrationImpl`
  - start penetration
  - movement retry after depenetration
- `transform-component-pose.md`
  - `CalcNewComponentToWorld_GeneralCase`
  - `UpdateComponentToWorldWithParent`
  - `SetWorldTransform`
  - `SetWorldLocationAndRotation`
  - `MovementComponent` `FQuat` overloads
  - collision transform space conversion

## Contract Card Template

```md
### Contract: <name>

- Reference engine: Unreal
- Reference root: `.ref/UnrealEngine/.ref/UnrealEngine`
- Trust level: raw-source-verified
- Review status: pending | accepted | rejected | superseded
- Source:
  - `path/to/file.cpp:line`
- Inputs:
- Outputs:
- Invariant:
- Rejection/filtering rules:
- Ordering/tie-break rules:
- Edge cases:
- Why this matters:
- EngineLab audit question:
```

## Use From EngineLab Audits

EngineLab KCC audits should cite these contract files after they exist.
Until then, audits may cite raw Unreal source directly, but must mark the
comparison as pending contract review.

EngineLab audit conclusions belong under:

- `docs/audits/kcc/`

Session routing belongs under:

- `docs/agent-context/cct-refactor.md`
