# KCC StepMove Lateral Response Semantics

Updated: 2026-05-09

## 0. Verdict

`StepMove` is a lateral movement-response stage.

It may consume raw sweep normals only as input to build a lateral response
normal. It must not directly use raw sweep normals as slide normals because raw
normals can represent face, edge, corner, support, proxy, or initial-overlap
facts.

## 1. Normal Ownership

| Normal | Meaning | Owner |
|---|---|---|
| raw sweep normal | geometric fact from `SceneQuery` / narrowphase | `SceneQuery` |
| lateral response normal | `rawNormal` with `up` component removed and normalized | `StepMove` |
| support / walkable normal | accepted ground/support normal | `StepDown`, `HasWalkableSupport`, future ramp stage |
| recovery / MTD normal | pose-correction direction for penetration | `Recover`, future MTD-like stage |

## 2. Current Source Contract

`WorldState` passes a lateral displacement into the controller, and the KCC
pipeline describes `StepMove` as lateral-only:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:17`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:332`

The source anchor for this contract is:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:37`

`StepMove` now builds its movement response through
`BuildLateralStepMoveResponseNormal`:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:76`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:389`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:477`

`SlideAlongNormal` still removes the component of remaining motion that goes
into the supplied normal:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:833`

Therefore the normal supplied to `SlideAlongNormal` must already be the correct
movement-response normal for the current stage.

## 3. Why Raw Normal Is Not StepMove Response

In cube edge, corner, proxy triangle, skin, or initial-overlap cases, a raw
sweep normal can contain a positive `up` component. That is not automatically a
bug in `SceneQuery`.

It becomes a KCC bug when `StepMove` treats that normal as a wall-slide normal.
If `SlideAlongNormal` receives a normal with `up` in it, projecting the
remaining lateral movement can create `dxIntent.y > 0`.

That was the important pattern in the `062651` trace:

```text
StepMove received a positive hit with an upward raw normal.
StepMove used that normal as the slide plane.
The lateral movement phase produced upward displacement.
```

This session fixes only that stage ownership error.

## 4. Non-Goals

This document does not claim that all seam / wall-climb cases are fixed.

Out of scope for this session:

- ramp movement
- reactive `TryStepUpOverBlock`
- `StepDown` redesign
- MTD-like recovery
- multi-hit / touch-buffer collection
- `SceneQuery` collector/filter refactor

The expected next sessions are:

```text
1. Walking ramp / slope movement using verified support normal
2. Reactive step-up transaction after positive non-walkable lateral blocking hit
3. MTD-like recovery for t~0 / initial-overlap / skin contacts
```

## 5. Regression Checks

Use the existing manual traces/probes:

| Case | Expected result after this session |
|---|---|
| `062651` | `StepMove` should not create positive `dxIntent.y` from a raw upward normal |
| `062952` | may still fail; if no progress is made, `stuck` should be reported honestly |
| `062443` | may still fail due recovery / support / t~0 behavior |
| flat wall slide | lateral slide should still work |
| ramp | may need the next ramp session; do not use this session to judge final ramp behavior |
