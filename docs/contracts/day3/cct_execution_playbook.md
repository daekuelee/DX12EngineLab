# CCT Execution Playbook (Session-Safe)

Updated: 2026-02-25
Owner: CCT refactor lane (HitPolicy -> OverlapSet -> UE-deep policy)

## 0) Why this file exists
- `gpt.txt` is the plan SSOT, but it is intentionally high-level.
- This file is the implementation playbook for restart-safe execution.
- If chat/terminal context is lost, resume from this file + `gpt.txt`.

## 1) Non-Negotiable Invariants
- Keep velocity semantics exactly:
  - `v_next = (x_finalPre - x_sweep) / dt`
  - Recovery/post-cleanup must not contribute to velocity.
- Determinism:
  - fixed iteration caps
  - stable tie-break ordering
  - no random/unstable container iteration.
- No broad refactor in early batches:
  - small helpers + localized edits only.

## 2) Current State Snapshot
- Batch 1 done (callback seam + debug wiring).
- Main anchors:
  - `Engine/Collision/SceneQuery/SqCallbacks.h`
  - `Engine/Collision/SceneQuery/SqQuery.h` (`SweepCapsuleClosestHit_Callback`)
  - `Engine/Collision/CollisionWorld.{h,cpp}` (`MoveSweepResult`, `SweepCapsuleClosestMove`)
  - `Engine/Collision/KinematicCharacterController.{h,cpp}` (phase-tagged sweep debug accumulation)

## 3) Locked Order (Do not reorder)
1. Batch 2: Quick HitPolicy v1
2. Batch 3: OverlapSet diff v1
3. Batch 4: UE-deep HitPolicy v2
4. Batch 5: Owner/Character transform callback boundary
5. Batch 6: Primitive registry/ref-remap cleanup

---

## 4) Batch 2 - Quick HitPolicy v1 (next start point)

### 4.1 Goal
- Fast policy parity step before touching overlap-cache architecture.
- Make `None/Touch/Block` semantically real in movement sweep path.
- Handle `t==0` (initial overlap) explicitly and deterministically.

### 4.2 Scope (files/functions)
- `Engine/Collision/SceneQuery/SqCallbacks.h`
  - `CharacterMoveSweepCallback::PostFilter(...)`
- `Engine/Collision/SceneQuery/SqQuery.h`
  - `SweepCapsuleClosestHit_Callback(...)`
- `Engine/Collision/CollisionWorld.{h,cpp}`
  - `MoveSweepResult`
  - `SweepCapsuleClosestMove(...)`
- `Engine/Collision/KinematicCharacterController.cpp`
  - `SweepClosest(...)` consumption of new metadata

### 4.3 Patch Units
- B2-P1: Add touch capture buffer in query result path
  - Keep closest blocking hit selection as-is (`BetterHit` cascade).
  - Also retain bounded touches (`kMaxTouches` fixed-size) before terminal block.
  - Deterministic order rule:
    - sort by `t`, then existing tie-break fields.
- B2-P2: Explicit initial-overlap policy
  - For movement sweep:
    - initial overlap candidates can be converted to `Touch` to allow continued search.
    - preserve at least one deterministic representative for debug/policy.
  - Track counters:
    - initial-overlap seen
    - initial-overlap converted-to-touch
    - initial-overlap kept-as-block.
- B2-P3: Return compact policy metadata to movement
  - Extend `MoveSweepResult` with minimal metadata only (no heavy structs).
  - Suggested fields:
    - `hasInitialOverlapTouch`
    - `initialOverlapTouchCount`
    - `hadBlockingCandidate`
    - `selectedBlockWasInitialOverlap`.
- B2-P4: KCC consume metadata for diagnostics only (behavior-safe)
  - Do not change step logic yet.
  - Route metadata into debug counters to validate policy behavior in runs.

### 4.4 Out of Scope in Batch 2
- OverlapSet enter/stay/exit cache.
- Moving-out ignore rule (UE `ShouldIgnoreHitResult` equivalent).
- PullBackHit/opposed-normal selector parity.

### 4.5 Acceptance Criteria
- Build compiles.
- Existing movement behavior is not regressed in normal non-penetrating walk.
- Deterministic replay gives same selected blocker for identical input.
- Debug counters show non-zero initial-overlap conversions in forced t==0 scenes.

---

## 5) Batch 3 - OverlapSet diff v1

### 5.1 Goal
- Introduce persistent overlap state per character and diff it each tick.
- Separate persistent overlap events from transient sweep hits.

### 5.2 Scope
- `Engine/Collision/KinematicCharacterController.h/.cpp`
- If needed, thin helper in `Engine/Collision/CollisionWorld.h/.cpp` for deterministic overlap keys.

### 5.3 Patch Units
- B3-P1: Add persistent set containers (deterministic vectors, sorted).
  - Key: `(PrimType, index)` or remapped collider id.
- B3-P2: Build current overlap set from existing overlap query.
  - Use sorted deterministic contacts/ids.
- B3-P3: Diff old/new:
  - `entered = new - old`
  - `stayed  = new ∩ old`
  - `exited  = old - new`
- B3-P4: Expose debug counters only first:
  - entered/stayed/exited counts.
  - no gameplay callbacks yet.

### 5.4 Acceptance Criteria
- Stable counts under same replay.
- No impact on velocity semantics.
- No dynamic allocation in hot loop beyond existing vectors' reserved capacity.

---

## 6) Batch 4 - UE-deep HitPolicy v2

### 6.1 Goal
- Bring movement-side hit policy closer to UE semantics.

### 6.2 Scope
- `Engine/Collision/SceneQuery/SqQuery.h`
- `Engine/Collision/KinematicCharacterController.cpp`
- (optional) `CollisionWorld.cpp` if selector helper needed.

### 6.3 Patch Units
- B4-P1: Touch-before-block flush rule
  - Keep touches in front of selected block.
  - Discard touches behind terminal block time.
- B4-P2: Moving-out ignore rule (UE analogue)
  - For initial overlap blocker, ignore when moving out along normal above threshold.
- B4-P3: Initial overlap blocker selection parity
  - If multiple initial overlap blockers exist, choose most opposed normal to move delta.
- B4-P4: Pull-back robustness
  - Small retract on selected hit time to reduce t~0 re-hit loops.

### 6.4 Acceptance Criteria
- Seam jam and t~0 hitch reduced in targeted repro maps.
- No nondeterministic blocker flips across reruns.

---

## 7) Batch 5 - Owner/Transform callback boundary

### Goal
- Decouple KCC state ownership from direct internal state mutation.
- Keep hot path non-virtual; use function object/callback boundary.

### Minimum deliverable
- `GetTransform/SetTransform` style boundary for KCC integration points.
- No virtual in per-candidate sweep loops.

---

## 8) Batch 6 - Primitive registry / ref-remap cleanup

### Goal
- Make primitive/collider ownership and id remap explicit and stable.
- Prepare for tri-mesh/primitive growth without fragile index coupling.

### Minimum deliverable
- Documented stable id mapping layer.
- BVH local index -> stable collider id conversion path centralized.

---

## 9) Restart Checklist (next session)
1. Read `gpt.txt` first (plan SSOT).
2. Read this file and continue from Batch 2.
3. Before patching, print proposed diff chunk for Batch 2 only.
4. Apply small patch, compile, run minimal deterministic checks.

## 10) Resume Prompt
`gpt.txt SSOT 기준으로 진행. cct_execution_playbook.md 기준으로 Batch2(Quick HitPolicy v1)만 먼저 구현하고 diff 먼저 보여줘. Ref/UE는 read-only.`

