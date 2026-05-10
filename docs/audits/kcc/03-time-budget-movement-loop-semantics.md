# KCC Phase3 Time-Budget Movement Loop Semantics

## Purpose

Phase2 introduced explicit `Walking` / `Falling` policy and removed speculative
`StepUp` pre-lift. It intentionally does not solve continuous same-tick mode
transitions.

This document preserves the Phase3 context so the next session does not rebuild
the reasoning from chat history.

## Problem

The current KCC tick is still phase-linear:

```text
IntegrateVertical
Recover
StepUp
StepMove
StepDown
post Recover
Writeback
```

That order can classify mode changes, but it does not spend remaining frame time
after a transition.

Examples:

- `Falling -> Walking`: a character lands halfway through the tick, but the
  remaining walking movement is not re-solved in the same tick.
- `Walking -> Falling`: a character walks off a ledge halfway through the tick,
  but falling motion begins on the next tick.
- `Walking -> wall hit`: a lateral hit should consume hit fraction and spend the
  remaining fraction on slide or reactive step-up.

## Target Semantics

Phase3 should make time budget the movement-loop SSOT.

```text
remainingTime = dt

while remainingTime > 0 and iterations < maxSimulationIterations:
  if mode == Walking:
    solve walking movement for timeSlice
    if support lost at fraction t:
      consume t
      SetModeFalling()
      continue with remainingTime

  if mode == Falling:
    solve air movement for timeSlice
    if landed at fraction t:
      consume t
      SetModeWalking(landingNormal)
      continue with remainingTime
```

## Unreal Reference Contract

Use Unreal only for movement-policy integration, not low-level geometry.

Relevant checked-in reference excerpts:

- `ex2.cpp:4804-4971`: `PhysFalling` tracks `remainingTime`, detects valid
  landing, adds `subTimeTickRemaining`, and calls `ProcessLanded`.
- `ex2.cpp:6272-6308`: `ProcessLanded` changes post-landed physics and calls
  `StartNewPhysics(remainingTime, Iterations)`.
- `ex2.cpp:5457-5530`: `MoveAlongFloor` consumes hit fraction and then attempts
  `StepUp` or `SlideAlongSurface` with the remaining fraction.

Extract the contract, not code.

## Required EngineLab Design Work

Current `CctInput.walkMove` is a per-tick displacement with `dt` already baked in.
That is weak for a time-budget loop.

Phase3 must choose one of these input contracts:

1. Add a velocity-like input such as `walkVelocity`, then compute per-substep
   displacement from `timeSlice`.
2. Use a short-term bridge: derive `remainingWalkMove = originalWalkMove *
   remainingFraction`.

The bridge is acceptable for a narrow refactor, but the long-term contract should
prefer velocity/time input.

## Reactive StepUp Placement

Reactive `TryStepUpOverBlock` belongs inside the Walking branch after a lateral
blocking hit, not before `StepMove`.

Trigger sketch:

```text
hit.hit
hit.t > tSkin + nearZeroEps
!IsWalkable(hit.normal)
IsWalking()
remaining lateral move is non-trivial
not initial-overlap / skin escape
```

Transaction sketch:

```text
save position/state
sweep up by stepHeight
sweep forward by remaining displacement
sweep down
accept only walkable landing
validate height <= stepHeight + epsilon
success: commit and SetModeWalking
failure: restore and slide/block normally
```

Do not call `Recover` inside the transaction unless rollback also owns recovery
effects.

## Acceptance Cases

Phase3 should be validated with the existing KCC trace plus explicit repro runs.

- Landing continuation: falling into walkable floor while holding movement should
  land and spend remaining time as Walking in the same tick.
- Ledge continuation: walking off an edge should begin Falling in the same tick,
  not one tick later.
- Wall slide continuation: wall hit should consume hit fraction and spend the
  remaining movement on slide.
- Reactive step-up: stair climb should occur only after a lateral blocking hit
  and a successful up/forward/down transaction.

## Non-Goals

- Do not rewrite SceneQuery as part of Phase3.
- Do not add all-hits/multi-hit unless closest-hit trigger fails a recorded repro.
- Do not rewrite `Recover` into MTD in the same patch.
- Do not present Phase2 as solving same-tick continuous movement.
