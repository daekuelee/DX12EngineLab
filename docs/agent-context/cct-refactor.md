# CCT Refactor Context

Updated: 2026-05-10

## Purpose

This document fixes the current CCT context so SceneQuery work does not turn into uncontrolled KCC policy work.

The intended direction is:

- PhysX is the primary reference for CCT mechanics when applicable.
- Unreal is the movement-policy integration reference for walking, floor, step, slope, snap, and gameplay-facing decisions.

## Current Active Code

Phase2/F1 mode semantics are now the active direction for KCC work:

- `CctMoveMode` owns `Walking` / `Falling` policy.
- `onGround` is a compatibility mirror for `WorldState`, HUD, and older call sites.
- `Walking` does not accumulate gravity into `verticalVelocity`.
- `SimulateWalking` owns walking lateral movement and walking support validation.
- `SimulateFalling` owns a single diagonal air sweep over `walkMove + verticalOffset`.
- `StepUp` speculative stair pre-lift is removed from the active Falling path.
- Falling no longer borrows Walking lateral `StepMove` or Walking `StepDown`.
- Current F1 does not implement same-tick `remainingTime` continuation.

Current public KCC and world headers are bridge headers:

- `Engine/Collision/KinematicCharacterController.h:2-5`
  aliases `KinematicCharacterController` to `KinematicCharacterControllerLegacy`.
- `Engine/Collision/CollisionWorld.h:2-5`
  aliases `CollisionWorld` to `CollisionWorldLegacy`.

Current fixed-step integration calls KCC through `WorldState`:

- `Engine/WorldState.cpp:189-192`
  calls `m_cct->Tick(cctInput, fixedDt)`.

Current KCC phase order:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp`
  runs `PreStep`, `IntegrateVertical`, pre-sweep `Recover`, then dispatches to
  `SimulateWalking` or `SimulateFalling`, then post-sweep `Recover` and
  `Writeback`.

Current velocity invariant:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:709-721`
  computes velocity from `m_xFinalPre - m_xSweep`, excluding recovery and
  post-sweep cleanup from velocity.

## Current Policy Mixture

The current KCC source contains legacy phase/filter language from mixed reference work:

- `Engine/Collision/KinematicCharacterControllerLegacy.h:9-19`
  documents the current phase order.
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp`
  still preserves legacy trace slot names such as `afterStepUp`,
  `afterStepMove`, and `afterStepDown`.
- Walking lateral movement still uses the existing `StepMoveHitView` /
  `QueryStepMoveHit` boundary.
- Walking support maintenance still uses `stepDown*` trace fields for
  compatibility.

Target decision:

- Treat these as historical implementation clues, not as final architecture.
- Replace mixed policy with PhysX-compatible CCT mechanics plus Unreal-compatible movement policy where needed.

## SceneQuery Dependencies

KCC depends on SceneQuery for these facts:

- sweep hit fraction / TOI
- hit normal direction
- initial overlap handling
- deterministic hit selection
- overlap contact normal and depth
- source primitive identity after `CollisionWorld` remap

Current query path:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:753-760`
  calls `CollisionWorldLegacy::SweepCapsuleClosest(...)`.
- `Engine/Collision/CollisionWorldLegacy.cpp:53-70`
  calls `sq::SweepCapsuleClosestHit_Fast(...)` and remaps primitive indices.
- `Engine/Collision/CollisionWorldLegacy.cpp:99-115`
  calls `sq::OverlapCapsuleContacts_Fast(...)` and remaps contact indices.

## Known Risk Areas

### StepUp Gate

- Legacy risk: older `StepUp` behavior enabled stair lift when vertical velocity
  was negative and lateral intent existed.

Risk:

- Grounded ticks apply gravity before step phases, so `verticalVelocity < 0` can be common.
  StepUp legality must become an explicit movement policy, not an accidental consequence of sweep filtering.

Current expectation:

- `Walking` keeps `verticalVelocity == 0`.
- `Falling` applies positive jump/upward motion inside `MoveFallingAir`.
- Auto stair climb is intentionally incomplete until a future reactive transaction.

### Wall Normal Vertical Noise

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:411-414`
  removes the up component from non-walkable wall normals before slide.

Risk:

- This protects lateral slide from ratcheting upward, but only after SceneQuery selected and reported a hit.
  Source/proxy normal instability can still affect selected hit behavior.

### Recovery Push Direction

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:616-642`
  sums overlap contact normals over slop, normalizes the accumulated direction,
  applies `recoverAlpha`, then clamps by `contactOffset`.

Risk:

- Mixed wall/floor/seam contacts can produce vertical pose correction.
  The current velocity contract excludes this from velocity, but visual floating and grounding side effects remain possible.

## CCT Direction

Do not choose a single external engine wholesale.

- PhysX should define the primary mechanics vocabulary for CCT query/recovery/contact offset where it maps cleanly.
- Unreal should define gameplay-facing walking/floor/step/slope/snap policy where PhysX mechanics alone are insufficient.

## Unreal Reference Storage

Do not place detailed Unreal analysis in `AGENTS.md`.

Reusable Unreal movement-policy contracts belong under
`docs/reference/unreal/contracts/`, starting from
`docs/reference/unreal/contracts/00-unreal-contract-index.md`.

KCC comparison and patch-planning audits belong under `docs/audits/kcc/`.
The current routing note is:

- `docs/audits/kcc/11-unreal-policy-reference-plan.md`

## Non-Goals

- Do not implement a KCC rewrite during SceneQuery audit.
- Do not make `StepUp` or `Recover` changes without deterministic tests.
- Do not claim seam bug is fixed by a SceneQuery change unless KCC repro evidence exists.
- Do not claim Phase2 implements Unreal-style continuous `remainingTime`.
- Do not claim stair behavior is complete after removing speculative `StepUp`.

## Phase2 Trace Evidence

The KCC trace path is the preferred Phase2 repro/evidence mechanism:

- `CctDebug` owns phase snapshots for `beforeTick`, `afterIntegrateVertical`,
  `afterPreRecover`, `afterStepUp`, `afterStepMove`, `afterStepDown`,
  `afterPostRecover`, and `afterWriteback`.
- `WorldState::ClassifyKccTraceCulprit` classifies upward-pop source as
  `Recover`, `StepUp`, `StepMove`, `StepDown`, `PostRecover`, or `Unknown`.
- Saved traces are written to `captures/debug/kcc_trace_last.txt` and timestamped
  files when the HUD `KCC Trace` save action is used.

Phase2 success criteria:

- Grounded wall/seam push must not produce `StepUp` as the upward-pop culprit.
- `Walking` frames should show `verticalVelocity == 0` after vertical integration.
- If upward pop remains but culprit is `Recover`, `PostRecover`, `StepMove`, or
  `StepDown`, the next lane is not more `StepUp` gating.

## F1 Current Lane

F1 is tracked in:

- `docs/audits/kcc/12-falling-airmove-v1-semantics.md`

Active F1 invariant:

```text
Falling uses one diagonal air sweep and does not call Walking StepMove/StepDown.
```

Do not claim this solves floor edge/perch, MTD-like recovery, or Unreal-style
same-tick remaining time.

## Phase3 Next Lane

Read `docs/audits/kcc/03-time-budget-movement-loop-semantics.md` before doing
reactive step-up, landing continuation, or ledge-off continuation work.

Phase3 must introduce a time-budget movement loop:

- `Falling -> Walking`: landing should consume hit time, then continue same tick
  as Walking with remaining time.
- `Walking -> Falling`: ledge support loss should consume supported movement time,
  then continue same tick as Falling with remaining time.
- `Walking` wall hit should consume hit fraction, then spend remaining movement on
  slide or reactive `TryStepUpOverBlock`.

## Active Closest-Only Stabilization Roadmap

Current KCC stabilization is tracked in:

- `docs/audits/kcc/05-kcc-closest-only-stabilization-roadmap.md`
- `docs/audits/kcc/06-stepmove-query-boundary-plan.md`

The active Session 1 contract is:

```text
StepMove does not consume raw SceneQuery hits directly.
StepMove asks QueryStepMoveHit for a stage-usable lateral blocker.
```

Session 1.1 is the required follow-up before another behavior patch:

```text
StepMove traces must distinguish first raw hit from final StepMoveHitView.
Use stepMoveLast* fields to decide whether the next lane is StepDown/support,
filteredClosestHit, or MTD-like recovery.
```

## First KCC Follow-Up After SceneQuery Audit

1. Decide PhysX-compatible CCT mechanics contract.
2. Decide Unreal-compatible movement policy contract.
3. Audit current phase/filter/recovery behavior against those contracts.
4. Audit step-up legality.
5. Audit recovery push direction and velocity exclusion.
6. Build seam/wall-climb repro test plan.
