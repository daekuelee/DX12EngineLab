# Unreal Source Discovery

Updated: 2026-05-10

## Purpose

This file is a reusable discovery log for checked-out Unreal source used by DX12EngineLab movement-policy comparison.
It is not a behavior contract by itself.

## Evidence Policy

- Unreal is the movement-policy integration reference, not the low-level SceneQuery geometry oracle.
- Behavioral claims require raw source `file:line` evidence.
- Contract conclusions belong under `docs/reference/unreal/contracts/`.
- Context files and generated notes are search hints only.

## Source Roots Found

- `.ref/UnrealEngine/.ref/UnrealEngine`

## Initial Located Files

Character movement:

- `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h`
- `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp`

## Discovery Entries

### 2026-05-10 - CharacterMovement walking / floor / step contract mining

- Engine: Unreal
- Version/root: `.ref/UnrealEngine/.ref/UnrealEngine`
- Search terms: `MoveAlongFloor`, `ComputeGroundMovementDelta`, `PhysWalking`, `FindFloor`, `ComputeFloorDist`, `StepUp`, `IsWalkable`, `FFindFloorResult`
- Located files:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h`
- Missing files: none for this audit scope
- Notes: raw-source-backed contract card written to `docs/reference/unreal/contracts/character-movement-walking-floor-step.md`. This discovery entry is movement-policy context only; it does not audit EngineLab production code.

### 2026-05-10 - Floor find / perch / edge contract mining

- Engine: Unreal
- Version/root: `.ref/UnrealEngine/.ref/UnrealEngine`
- Search terms: `FindFloor`, `ComputeFloorDist`, `FFindFloorResult`, `IsWithinEdgeTolerance`, `ShouldComputePerchResult`, `ComputePerchResult`, `GetValidPerchRadius`, `ShouldCheckForValidLandingSpot`
- Located files:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h`
- Missing files: none for this audit scope
- Notes: raw-source-backed contract card written to `docs/reference/unreal/contracts/floor-find-perch-edge.md`. This card is required context for StepDown split audits that discuss floor quality, perch, edge tolerance, or falling landing.

### 2026-05-08 - Initial Unreal movement-policy roots

- Engine: Unreal
- Version/root: `.ref/UnrealEngine/.ref/UnrealEngine`
- Search terms: `CharacterMovementComponent.h`, `CharacterMovementComponent.cpp`, `MovementMode.h`
- Located files: see `Initial Located Files`
- Missing files: `MovementMode.h`
- Notes: this entry records source availability only; no behavior contract has been accepted.
