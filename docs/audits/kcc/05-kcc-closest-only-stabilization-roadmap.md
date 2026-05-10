# KCC Closest-Only Stabilization Roadmap

Updated: 2026-05-09

## 0. Verdict

The current KCC can be improved without multi-hit first, but only within a
bounded scope.

Closest-only can still support:

- clearer `StepMove` lateral blocker semantics
- Walking/Falling and support ownership cleanup
- reactive StepUp after an actual lateral blocker
- a first `filteredClosestHit` policy layer

Closest-only cannot fully solve:

- balanced corner/seam depenetration
- simultaneous wall + ground contact reasoning
- robust recovery from true overlap without MTD-like contact collection

## 1. Session Order

| Session | Name | Goal | Needs multi-hit now? |
|---:|---|---|---|
| 1 | `QueryStepMoveHit` boundary | Stop `StepMove` from consuming raw SceneQuery hits directly | No |
| 2 | Walking/Falling + StepDown support contract | Make ground/support ownership explicit | Mostly no |
| 3 | Reactive StepUp transaction | Attempt step only after a positive lateral blocker | No |
| 4 | SceneQuery `filteredClosestHit` policy | Query closest stage-usable hit instead of closest-any hit | No |
| 5 | MTD-like recovery | Move true penetration correction out of movement response | Likely overlap collector |
| 6 | Multi-hit / touch buffer | Resolve cases closest-only cannot represent | Yes, only if evidence requires it |

## 2. Dependency Rule

KCC stage contracts come before SceneQuery policy expansion.

Do not change `BetterHit`, candidate ordering, or raw kernel behavior while
implementing Session 1. Those are SceneQuery invariants and belong to Session 4
or Session 6 after deterministic probes exist.

## 3. Stable Contracts To Preserve

```text
SceneQuery reports raw geometric hits.
KCC stages convert raw hits into stage-specific semantic views.
A stage may only respond to hit kinds it explicitly owns.
```

Session 1 establishes:

```text
StepMove asks QueryStepMoveHit for a stage-usable lateral blocker.
StepMove may only slide against PositiveLateralBlocker.
```

Future SceneQuery work must preserve that `StepMove` contract even if
`QueryStepMoveHit` is later backed by `filteredClosestHit` or multi-hit
collection.

## 4. Deferred Debt

| Debt | Why deferred |
|---|---|
| `BetterHit` tie-break policy | Query invariant; changing it during Session 1 would mix kernel selection changes with KCC behavior changes |
| `SweepFilter` expressiveness | Needs stage contracts first, then a SceneQuery policy design |
| ordinary TOI `t==0` rejection | Not the same as explicit initial-overlap rejection; needs probe coverage |
| multi-contact recovery | Requires MTD/overlap collector design, not StepMove |
