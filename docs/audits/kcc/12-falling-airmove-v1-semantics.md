# FallingAirMove V1 Semantics

## Purpose

F1 removes the legacy Falling path that split air movement into:

```text
StepUp -> StepMove -> StepDown
```

That split made Falling borrow Walking lateral movement and Walking/Falling
floor maintenance policy. The target F1 contract is:

```text
Falling air movement = one diagonal sweep over walkMove + verticalOffset.
```

This is not Unreal parity. It is a local semantic cleanup before time-budget,
reactive step-up, perch/edge, and MTD-like recovery work.

## Reference Context

- `docs/audits/kcc/07-lightweight-kcc-audit-work-plan.md`
  orders `Falling diagonal move` after the mode shell and before the time-budget
  loop.
- `docs/audits/kcc/03-time-budget-movement-loop-semantics.md`
  keeps same-tick `remainingTime` continuation as later work.
- `docs/audits/kcc/02-walking-falling-reactive-stepup-semantics.md`
  keeps reactive `TryStepUpOverBlock` inside Walking after a lateral blocking hit.
- `docs/reference/unreal/contracts/character-movement-walking-floor-step.md`
  records that `MoveAlongFloor` starts from a walkable floor and is not the
  Falling movement path.
- `docs/reference/unreal/contracts/floor-find-perch-edge.md`
  records that Falling landing is not identical to Walking support maintenance.

## F1 Contract

`SimulateWalking` owns:

- Walking lateral sweep/slide.
- Walking support validation.
- Walking support loss -> `Falling`.

`SimulateFalling` owns:

- `airDelta = walkMove + up * verticalOffset`.
- One diagonal air sweep loop.
- Landing when descending into a walkable hit.
- Ceiling response when ascending into a downward-facing hit.
- Air slide for non-walkable hits.

`SimulateFalling` must not call:

- `StepUp`
- Walking lateral `StepMove`
- Walking support `StepDown`

## Explicit Non-Goals

- No time-budget continuation after landing.
- No ledge-off continuation within the same tick.
- No reactive stair transaction.
- No perch/edge/floor-distance quality policy.
- No MTD-like recovery replacement.
- No SceneQuery multi-hit or filtered collector refactor.

## Expected Trace Meaning

The current trace fields still use legacy names:

- `afterStepUp`
- `afterStepMove`
- `afterStepDown`
- `stepDownHit*`

In F1 Falling:

- `afterStepUp` is a compatibility snapshot before air movement.
- `afterStepMove` is the post-`MoveFallingAir` snapshot.
- `afterStepDown` mirrors the same post-air state because landing is decided
  inside `MoveFallingAir`.
- `stepDownHit*` records landing/floor hit facts for trace compatibility.

Do not interpret a Falling trace `StepMove` culprit as proof that Walking
`MoveWalkingLateral` ran. In F1 it may mean the air move changed Y in the legacy
trace slot.

## Acceptance Cases

- Jump + forward into a wall should not use Walking `StepMove`.
- Falling + forward into a cube should sweep the diagonal air path instead of
  separate lateral/downward phases.
- Falling onto a walkable floor should switch to `Walking`.
- Walking on top of a cube should still use walking lateral movement plus support
  maintenance.

## Remaining Debt

The next semantic sessions are:

1. Time-budget loop: consume hit fraction and continue after mode transition.
2. Reactive `TryStepUpOverBlock`: Walking-only transaction after lateral blocker.
3. Floor quality/perch/edge: reject fake edge floors and support-only contacts.
4. MTD-like recovery: separate pose recovery from movement/landing policy.
