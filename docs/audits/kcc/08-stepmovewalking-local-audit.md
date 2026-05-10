# StepMoveWalking Local Audit

Updated: 2026-05-10

## 0. Verdict

`StepMove` can be narrowed toward `StepMoveWalking`, but only as a small local
boundary fix in this session.

The current confirmed bug is:

```text
walkable/support-like raw hit -> PositiveLateralBlocker -> SlideAlongNormal
```

This conflicts with the local `StepMove` contract and with the raw-source-backed
Unreal movement-policy contract in
`docs/reference/unreal/contracts/character-movement-walking-floor-step.md`.

This audit does not claim the whole upward-pop / wall-climb problem is fixed.
`StepDown`, `Recover`, floor/perch, MTD-like recovery, and Walking/Falling mode
dispatch remain separate work.

## 1. Reference Level

Reference level:

- Unreal comparison: raw-source-backed movement-policy comparison.
- EngineLab behavior: current source and latest KCC trace.
- PhysX comparison: not used in this audit.

Unreal is used only as movement-policy reference. It is not used here as a
low-level geometry oracle.

## 2. Unreal Contract Summary

Relevant contract card:

- `docs/reference/unreal/contracts/character-movement-walking-floor-step.md`

Raw Unreal source establishes these policy boundaries:

- `PhysWalking` owns walking mode movement and floor refresh:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5554`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5645`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5667`
- `MoveAlongFloor` requires a walkable current floor and converts movement
  through floor/ramp policy before barrier handling:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5457`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5459`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5467`
- A walkable ramp/floor hit is processed as ground movement, not immediately as
  wall slide:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5394`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5399`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5486`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5490`
- Non-walkable blocking hit owns step-up or slide fallback:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5498`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5504`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:5508`
- Floor support is richer than normal.y:
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6949`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:6966`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7040`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7161`
  - `.ref/UnrealEngine/.ref/UnrealEngine/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp:7174`

The immediate EngineLab implication is narrow:

```text
StepMoveWalking should not convert a walkable/support-like hit into a
PositiveLateralBlocker.
```

Full Unreal-like `FindFloor`, perch, `MoveAlongFloor`, and transaction
`StepUp` are not part of this local patch.

## 3. Current EngineLab StepMove Contract

The file header says the controller does not compute horizontal input. The
caller passes `walkMove`, already resolved and dt-baked:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:6`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:7`
- `Engine/Collision/CctTypes.h:66`
- `Engine/Collision/CctTypes.h:70`

The current pipeline still calls all stages linearly:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:237`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:241`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:245`

`StepMove` itself declares the intended boundary:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:406`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:407`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:418`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:420`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:421`

The relevant local contract is:

```text
StepMove is a lateral movement-response stage.
Support, ramp, stair, and recovery normals belong to other stages.
```

## 4. What StepMove Reads And Writes

| Item | Current source | Meaning |
|---|---|---|
| `walkMove` | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:426` | input lateral displacement |
| `walkLen` early-out | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:428` | no sweep for zero movement |
| `m_originalDirection` | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:434` | anti-oscillation reference |
| `m_targetPosition` | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:435` | desired lateral target |
| `m_currentPosition` | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:555` and `Engine/Collision/KinematicCharacterControllerLegacy.cpp:573` | movement result |
| `m_targetPosition` rewrite | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:1067` to `Engine/Collision/KinematicCharacterControllerLegacy.cpp:1070` | slide projection |
| `m_debug.stepMoveLast*` | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:150` to `Engine/Collision/KinematicCharacterControllerLegacy.cpp:168` | final consumed query debug |

No `onGround`, `verticalVelocity`, or `groundNormal` should be owned by
`StepMove` in this local patch.

## 5. classifyStepMoveRawHit Audit

Current classification path:

| Step | Source | Current behavior |
|---|---|---|
| no hit | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:459` | `ClearPath` |
| near-zero flag | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:464` | debug/classification fact |
| walkable flag | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:466` | computed but not used as blocker rejection |
| start penetrating | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:467` | `NeedsRecovery / StartPenetrating` |
| build lateral normal | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:473` | removes up component |
| no lateral normal | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:476` | `ClearPath / NoLateralNormal` |
| not approaching | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:482` to `Engine/Collision/KinematicCharacterControllerLegacy.cpp:487` | `ClearPath / NotApproaching` |
| otherwise | `Engine/Collision/KinematicCharacterControllerLegacy.cpp:490` | `PositiveLateralBlocker` |

The defect is precise:

```text
rawWalkable is calculated but does not prevent PositiveLateralBlocker.
```

That means any walkable/support-like raw normal with a lateral component can be
converted into a lateral wall response.

## 6. queryStepMoveHit Resweep Meaning

`queryStepMoveHit` currently adapts a closest-only raw sweep:

- first sweep:
  - `Engine/Collision/KinematicCharacterControllerLegacy.cpp:510`
- first classification:
  - `Engine/Collision/KinematicCharacterControllerLegacy.cpp:511`
- if first raw hit is not start-penetrating, return it:
  - `Engine/Collision/KinematicCharacterControllerLegacy.cpp:515`
- if first raw hit is start-penetrating, resweep with reject-initial:
  - `Engine/Collision/KinematicCharacterControllerLegacy.cpp:519`
  - `Engine/Collision/KinematicCharacterControllerLegacy.cpp:522`
  - `Engine/Collision/KinematicCharacterControllerLegacy.cpp:524`

This resweep only separates explicit initial overlap from normal movement hits.
It does not filter walkable/support-like hits. Therefore the promoted result can
still be `rawWalkable == true`, and the current classifier can still mark it as
`PositiveLateralBlocker`.

## 7. SlideAlongNormal Call Condition

`SlideAlongNormal` is currently reached only after `ClearPath`,
`NeedsRecovery`, and `UnsupportedForStepMove` have already returned/broken:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:554`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:559`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:565`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:574`

`SlideAlongNormal` itself removes remaining movement into the supplied normal:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:1063`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:1067`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:1068`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:1070`

So the problem is not that `SlideAlongNormal` exists. The problem is that
walkable/support-like hits can be classified as `PositiveLateralBlocker` and
therefore become slide input.

## 8. Trace Evidence

Latest trace used:

- `captures/debug/kcc_trace_last.txt`

The trace header reports:

- `captures/debug/kcc_trace_last.txt:1`
  - `culprit=PostRecover`

So the full upward-pop chain is not proven to be only `StepMove`.

However, the local `StepMove` boundary violation is directly visible:

| Trace | Evidence | Meaning |
|---|---|---|
| `captures/debug/kcc_trace_last.txt:103` | `modeStepMove=Walking`, `stepMoveLastKind=PositiveLateralBlocker`, `stepMoveLastWalkable=1`, `stepMoveLastN=(-0.509231,0.693803,0.509236)` | Walking `StepMove` consumes walkable/support-like hit as lateral blocker |
| `captures/debug/kcc_trace_last.txt:109` | `stepMoveLastKind=PositiveLateralBlocker`, `stepMoveLastWalkable=1`, `stepMoveLastNearZero=1`, `forwardIters=9` | near-zero support-like hit causes repeated blocker iterations |
| `captures/debug/kcc_trace_last.txt:110` | `stepMoveLastKind=PositiveLateralBlocker`, `stepMoveLastWalkable=1`, `forwardIters=10`, `stuck=1` | repeated support-like blocker ends in stuck |

This is not only a Falling-mode dispatch issue. These lines are all in Walking:

```text
modeBegin=Walking modeStepMove=Walking modeFinal=Walking
```

## 9. User Report Connection

Current `report.txt` describes:

- cube top: `W` should escape, but character gets more stuck.
- cube front: expected escape, but character remains caught near floor/head.
- floor triangle/gap bugs appear to be back.

Relevant source:

- `report.txt:4`
- `report.txt:7`
- `report.txt:8`
- `report.txt:14`
- `report.txt:17`
- `report.txt:18`
- `report.txt:24`

The local interpretation:

```text
If support-like floor/edge hits become PositiveLateralBlocker, StepMove can
consume the movement budget as wall response instead of letting the character
escape along the intended walking path.
```

This explains the cube-top stuck symptom better than a pure `Recover` or pure
`StepDown` explanation, but it does not eliminate those later bugs.

## 10. Mismatch Table

| Topic | Unreal-backed policy | Current EngineLab behavior | Risk |
|---|---|---|---|
| Walking floor movement | Walking movement starts from walkable floor and uses floor/ramp policy before barrier handling | `StepMove` receives caller-provided lateral `walkMove` and can consume support-like hits as blockers | Medium |
| Walkable hit | Walkable ramp/floor hit is processed as ground movement, not immediate wall slide | `rawWalkable` is recorded but does not reject `PositiveLateralBlocker` | High |
| Non-walkable blocker | Barrier hit owns step-up or slide fallback | `PositiveLateralBlocker` can be assigned to walkable/support-like hit | High |
| Floor validity | Floor support uses distance, edge tolerance, perch, and walkability | Current local patch cannot validate support location/perch | Medium |
| Step-up | Reactive transaction after blocker | Not part of this patch | Medium |

## 11. Recommended Small Patch

Patch only the `StepMove` boundary:

```text
raw.startPenetrating -> NeedsRecovery / StartPenetrating
rawWalkable          -> ClearPath / WalkableSupport
non-walkable + valid lateral normal + approaching -> PositiveLateralBlocker
```

Recommended write set:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp`
- `Engine/Collision/CctTypes.h`
- `Engine/WorldState.cpp`

Recommended enum addition:

```text
CctStepMoveRejectReason::WalkableSupport
```

Why `ClearPath` instead of `UnsupportedForStepMove`:

- The reported cube-top case expects lateral escape.
- `UnsupportedForStepMove` currently sets `m_debug.stuck = true` and breaks.
- `ClearPath / WalkableSupport` better means “this is not a lateral blocker for
  StepMove”.

## 12. Non-Goals

Do not do these in the small patch:

- Do not edit `StepDown`.
- Do not edit `Recover`.
- Do not edit `SceneQuery`.
- Do not change `walkMove` input contract.
- Do not introduce `SimulateWalking` / `SimulateFalling`.
- Do not add Unreal-style full `FindFloor`, perch, or transaction `StepUp`.

## 13. Acceptance Criteria

Trace condition that must disappear:

```text
stepMoveLastWalkable=1 && stepMoveLastKind=PositiveLateralBlocker
```

Expected replacement:

```text
stepMoveLastWalkable=1
stepMoveLastKind=ClearPath
stepMoveLastReason=WalkableSupport
```

Manual checks:

- cube top escape with `W`
- cube front stuck case
- flat wall slide still works
- `1.4` wall climb case observed but not claimed fully fixed
- `2.6` upward movement case observed but not claimed fully fixed

## 14. Next Implementation Prompt

```text
Implement the StepMoveWalking small patch from
docs/audits/kcc/08-stepmovewalking-local-audit.md.

Modify only:
- Engine/Collision/KinematicCharacterControllerLegacy.cpp
- Engine/Collision/CctTypes.h
- Engine/WorldState.cpp

Goal:
classifyStepMoveRawHit must classify rawWalkable hits as ClearPath with
WalkableSupport reason before building a lateral response normal.
Only non-walkable + valid lateral normal + approaching hits may become
PositiveLateralBlocker.

Do not modify StepDown, Recover, SceneQuery, walkMove input, or mode dispatch.

After patch, verify that latest KCC trace no longer contains:
stepMoveLastWalkable=1 && stepMoveLastKind=PositiveLateralBlocker.
```
