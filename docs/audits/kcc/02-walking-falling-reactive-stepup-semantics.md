# KCC Walking / Falling / Reactive StepUp Semantics Audit

Updated: 2026-05-09

## 0. Verdict

현재 `KinematicCharacterControllerLegacy`의 핵심 문제는 단일 수치 버그가 아니라 movement semantic이 섞여 있다는 점이다.

가장 위험한 혼합은 다음이다.

```text
Walking 상태에서 gravity를 verticalVelocity에 누적
-> verticalVelocity < 0
-> StepUp이 이것을 stairLift trigger로 사용
-> 실제 lateral blocking hit 없이 stepHeight pre-lift
-> StepDown이 나중에 보상하기를 기대
```

이 구조는 낮은 계단을 넘기 위한 speculative pre-lift로는 설명 가능하지만, wall / seam / corner에서는 "계단인지 확인하기 전에 먼저 위로 올리는" 동작이 된다.

Unreal `CharacterMovement`의 walking semantics와 비교하면, 적용해야 할 핵심은 전체 `MovementMode` 복제가 아니라 다음 두 가지다.

```text
1. Walking / Falling 실행 정책 분리
2. StepUp을 선행 phase가 아니라 lateral blocking hit에 대한 reactive transaction으로 이동
```

하루 안에 가능한 범위는 `Walking/Falling` mode skeleton과 `StepUp` speculative stairLift 제거까지다. Reactive `TryStepUpOverBlock`까지 완성하는 것은 가능하더라도 regression 위험이 높다.

## 1. Current DX12EngineLab Semantics

### 1.1 Current Tick Order

현재 tick 순서:

```text
PreStep
IntegrateVertical
Recover
StepUp
StepMove
StepDown
post Recover
Writeback
```

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:114-130`

현재 의미:

```text
1. verticalVelocity를 먼저 적분한다.
2. penetration recovery를 수행한다.
3. StepUp이 먼저 capsule을 위로 올린다.
4. StepMove가 올라간 위치에서 lateral sweep/slide를 수행한다.
5. StepDown이 m_currentStepOffset과 gravity drop을 합쳐 내려오며 ground를 찾는다.
```

즉 현재 구조는 "move first"가 아니라 "pre-lift first"이다.

### 1.2 Gravity Is Not the Core Bug

`IntegrateVertical`은 grounded 상태에서도 gravity를 항상 적용한다.

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:173-181`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:194-202`

이 자체는 반드시 틀린 것은 아니다. downward floor probe를 만들기 위해 grounded tick에서도 작은 downward tendency를 둘 수 있다.

문제는 이 값이 `StepUp`의 stair trigger로 재사용된다는 점이다.

### 1.3 StepUp Uses Falling-Like Velocity as Stair Trigger

현재 `StepUp`은 다음 조건으로 `stairLift`를 만든다.

```cpp
const bool hasLateralIntent = sq::LenSq(walkMove) > (kMinDist * kMinDist);
float stairLift = (m_state.verticalVelocity < 0.0f && hasLateralIntent)
    ? m_config.stepHeight
    : 0.0f;
```

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:227-236`

문제 상황:

```text
1. onGround 상태로 tick 시작
2. IntegrateVertical이 gravity를 적용해서 verticalVelocity < 0
3. W/A/S/D 입력으로 hasLateralIntent = true
4. StepUp이 m_config.stepHeight만큼 선행 lift
```

여기서 아직 확인하지 않은 것:

- 실제 lateral blocking hit이 있는가
- 그 hit이 낮은 stair side인가
- wall / seam / corner가 아닌가
- headroom이 있는가
- 위에서 앞으로 이동 가능한가
- 아래로 sweep했을 때 walkable ground가 있는가

따라서 현재 `StepUp`은 계단 오르기가 아니라 "lateral input이 있으면 먼저 계단이라고 가정"하는 구조다.

### 1.4 StepDown Is Overloaded

현재 `StepDown`은 다음을 동시에 맡는다.

- pre-lift 보상
- gravity-based downward movement
- walkable ground search
- no-reject retry
- extended stepHeight fallback
- overlap support fallback
- ground latch
- full drop / airborne transition

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:433-563`

특히 `dropDist`가 다음처럼 계산된다.

```cpp
float dropDist = m_currentStepOffset + downVelocityDt;
```

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:442-450`

이 말은 `StepDown`이 "floor find"만 하는 것이 아니라, 앞선 `StepUp`의 speculative lift를 회수하는 compensation phase라는 뜻이다.

wall / seam / corner에서 `StepDown`이 legal floor를 놓치면, pre-lift가 완전히 회수되지 않거나 `fullDrop` / latch / overlap support가 섞여 증상이 불안정해진다.

### 1.5 onGround Is Used as State, Not Mode

현재 `CctState`에는 `onGround`, `wasOnGround`, `verticalVelocity`, `verticalOffset`만 있고 명시적 movement mode가 없다.

근거:

- `Engine/Collision/CctTypes.h:77-86`

문제:

```text
onGround == true  -> 결과 상태
Walking mode     -> 이번 tick을 어떤 정책으로 풀 것인가
```

이 둘은 다르다. 현재는 `onGround` 하나로 실행 정책과 결과 상태를 동시에 표현하려고 해서, "grounded인데 gravity를 falling처럼 누적"하는 혼합이 생긴다.

## 2. Unreal Walking Semantics

이 문서는 Unreal을 코드 복제 대상으로 쓰지 않는다. 목적은 movement phase contract를 분리하는 것이다.

### 2.1 Unreal Walking Does Not Pre-Lift Before Movement

Unreal `PhysWalking`은 walking velocity를 먼저 gravity-horizontal로 정리한다.

근거:

- `UnrealFork/kinetic.cpp:5536-5551`
- `UnrealFork/kinetic.cpp:5608-5611`

그 다음 lateral movement kernel인 `MoveAlongFloor`를 호출한다.

근거:

- `UnrealFork/kinetic.cpp:5632-5645`

`MoveAlongFloor` 안에서는 먼저 floor/ramp 기반 movement vector를 만들고, `SafeMoveUpdatedComponent`로 sweep movement를 수행한다.

근거:

- `UnrealFork/kinetic.cpp:5457-5468`

즉 Unreal walking의 기본 순서는 다음이다.

```text
1. walking velocity에서 gravity-axis component 제거
2. floor/ramp 기반 lateral movement 계산
3. 먼저 move sweep
4. blocking hit이 있으면 그 hit에 반응
```

현재 DX12EngineLab의 `StepUp -> StepMove` 선행 lift 구조와 다르다.

### 2.2 Unreal StepUp Is Reactive

Unreal은 `MoveAlongFloor`의 sweep 결과가 blocking hit일 때 `CanStepUp` / `StepUp`을 시도한다.

근거:

- `UnrealFork/kinetic.cpp:5482-5505`

실패하면 impact 처리 후 slide한다.

근거:

- `UnrealFork/kinetic.cpp:5504-5509`

따라서 핵심 contract:

```text
StepUp is a reaction to a lateral blocking hit.
StepUp is not a standalone pre-pass before movement.
```

### 2.3 Unreal StepUp Is Transactional

Unreal `StepUp` 내부는 다음 순서다.

```text
1. reject invalid hit / over-height / bad state
2. scoped movement update 시작
3. up move
4. forward move
5. optional slide adjustment
6. down move
7. height / walkability / edge tolerance / floor validation
8. success면 commit
9. failure면 RevertMove
```

근거:

- `UnrealFork/kinetic.cpp:7450-7512`
- up move: `UnrealFork/kinetic.cpp:7514-7524`
- forward move: `UnrealFork/kinetic.cpp:7526-7570`
- down move: `UnrealFork/kinetic.cpp:7573-7580`
- height / walkability / edge validation: `UnrealFork/kinetic.cpp:7583-7631`
- floor validation: `UnrealFork/kinetic.cpp:7633-7651`
- success return: `UnrealFork/kinetic.cpp:7661-7665`

DX12EngineLab에 그대로 복사할 필요는 없지만, 실패 시 state를 바꾸지 않는 transactional property는 필요하다.

### 2.4 Unreal Handles Penetration Inside Safe Move

Unreal `SafeMoveUpdatedComponent`는 move 후 initial penetration이면 penetration adjustment를 계산하고 resolve 후 원래 move를 retry한다.

근거:

- `UnrealFork/MovementComponent.cpp:558-585`

DX12EngineLab의 현재 `Recover`는 별도 phase로 존재한다.

근거:

- pre-sweep recover: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:117-124`
- post-sweep recover: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:135-141`
- `Recover` implementation: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:585-630`

따라서 Unreal 방식으로 전환한다고 해서 반드시 `Recover`를 `SafeMove` 내부로 즉시 옮겨야 하는 것은 아니다. 하지만 contract는 분명해야 한다.

```text
Recover는 pose-only penetration cleanup이다.
Recover는 mode, velocity, ground state를 결정하지 않는다.
Reactive StepUp transaction 안에서 Recover를 자동 호출하지 않는다.
```

## 3. Target DX12EngineLab Semantics

### 3.1 Minimal Modes

Unreal 전체 movement mode는 DX12EngineLab에 과하다.

지금 필요한 것은 최소 두 개다.

```cpp
enum class CctMoveMode {
    Walking,
    Falling,
};
```

선택적으로 future-proof를 위해 `Disabled` 또는 `None`은 둘 수 있지만, 현재 bug fix에는 필요 없다.

`NavWalking`, `Swimming`, `Flying`, `Custom`은 현재 범위 밖이다.

### 3.2 Walking Mode Contract

`Walking`은 다음을 의미한다.

```text
Character has walkable support.
Movement is solved as lateral motion plus support validation.
Gravity is not accumulated into verticalVelocity as a falling displacement.
StepUp can only be attempted reactively after a lateral blocking hit.
```

Required state:

- `moveMode == Walking`
- `onGround == true` as compatibility mirror
- `verticalVelocity == 0`
- `verticalOffset == 0`
- `groundNormal` is the last accepted walkable support normal

Allowed operations:

- pre-sweep `Recover` as pose-only cleanup
- `StepMoveWalking`
- reactive `TryStepUpOverBlock`
- `FindGround` / walking `StepDown` as support maintenance

Not allowed:

- `verticalVelocity < 0` as stair trigger
- unconditional `stepHeight` pre-lift
- `StepDown` full-drop teleport as walking support maintenance

### 3.3 Falling Mode Contract

`Falling`은 다음을 의미한다.

```text
Character has no accepted walkable support or is jumping.
Gravity is integrated into verticalVelocity.
Vertical displacement is part of movement.
Landing transitions back to Walking.
```

Required state:

- `moveMode == Falling`
- `onGround == false`
- `verticalVelocity` is integrated by gravity
- `verticalOffset = verticalVelocity * dt`

Allowed operations:

- gravity integration
- vertical sweep / combined motion sweep
- ceiling clip when moving upward
- landing on walkable ground

Not allowed:

- reactive stair step while falling
- `StepUp` stair transaction without walkable support
- treating wall contact normal as ground support

### 3.4 Compatibility Rule

For transition safety, `onGround` should not be removed immediately.

Use this rule:

```text
moveMode is the policy SSOT.
onGround is a compatibility mirror for WorldState / HUD / existing call sites.
```

Meaning:

- `moveMode == Walking` implies `onGround = true`
- `moveMode == Falling` implies `onGround = false`
- any code that writes `onGround` should be audited and eventually replaced by mode transition helpers

## 4. Recover Compatibility

### 4.1 Current Recover Contract

Current `Recover`:

- collects overlap contacts
- sums over-penetration correction vectors
- normalizes push direction
- applies clamped pose push
- does not edit velocity or ground state

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:590-625`

This is compatible with the target mode split if it remains pose-only.

### 4.2 What Must Not Happen

When introducing `Walking/Falling`, do not let `Recover` become an implicit movement mode transition.

Bad:

```text
Recover pushed upward -> assume grounded
Recover pushed sideways -> assume wall slide complete
Recover during StepUp transaction -> commit partial step
```

Correct:

```text
Recover only produces pose correction.
Ground is accepted only by walkable support query / down sweep.
Velocity is computed from intent movement, not recovery displacement.
```

Current velocity writeback already tries to exclude recovery displacement.

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:132-144`
- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:692-705`

### 4.3 Can Unreal Penetration Policy Be Plugged In Directly?

Not directly in one day.

Unreal's `SafeMoveUpdatedComponent` integrates move, penetration resolution, and retry in one API.

근거:

- `UnrealFork/MovementComponent.cpp:558-585`

DX12EngineLab currently has external pre/post `Recover` phases. That can remain for now, but the API contract must be explicit:

```text
Sweep/Step functions should not call Recover internally unless the transaction owns rollback.
Recover result must not feed velocity or mode.
```

## 5. Hit Policy Compatibility

### 5.1 Current Query Shape

KCC uses `SweepClosest`.

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:711-718`

`CollisionWorldLegacy::SweepCapsuleClosest` delegates to `SweepCapsuleClosestHit_Fast`.

근거:

- `Engine/Collision/CollisionWorldLegacy.cpp:53-70`

`SweepCapsuleClosestHit_Fast` selects one best hit using `BetterHit`.

근거:

- `Engine/Collision/SceneQuery/SqQueryLegacy.h:121-197`
- tie-break: `Engine/Collision/SceneQuery/SqQueryLegacy.h:59-73`

### 5.2 Is Closest Hit Enough?

For a first reactive step-up implementation: conditionally yes.

It is enough if the first version's promise is narrow:

```text
Use the StepMove blocking hit as the obstacle candidate.
Attempt a transaction.
If validation fails, fall back to slide.
Do not claim full Unreal floor/perch parity.
```

It is not enough for full parity because Unreal's floor acceptance includes edge tolerance, perch, floor distance bands, and more nuanced hit reuse.

Therefore:

```text
V1 reactive step-up can use closest hit.
V2 floor quality and seam/perch fixes probably need richer hit/support policy.
```

### 5.3 Current Dot / Approach Filter

Current `SweepFilter` is a normal predicate:

```text
accept when Dot(hitNormal, refDir) >= minDot
```

근거:

- `Engine/Collision/SceneQuery/SqTypes.h:143-159`
- `Engine/Collision/SceneQuery/SqNarrowphaseLegacy.h:62-76`
- `Engine/Collision/SceneQuery/SqQueryLegacy.h:174-181`

Current `StepMove` uses:

```text
refDir = -Normalize(remaining)
minDot = 1e-4
filterInitialOverlap = true
```

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:326-337`

This is compatible with reactive step-up as the first movement filter.

Rules:

- Keep approach filter for `StepMove` movement blocking.
- Do not use approach filter as ground support acceptance.
- Use walkable/up filter for downward support query.
- Use ceiling/down filter for upward headroom query.
- Do not use one filter for every phase.

### 5.4 Filter Policy by Stage

Recommended V1:

| Stage | Query | Filter |
|---|---|---|
| `StepMoveWalking` | lateral sweep | approach filter, initial overlaps filtered |
| `TryStepUpOverBlock` up | upward sweep | ceiling/headroom filter |
| `TryStepUpOverBlock` forward | lateral sweep at lifted pose | approach filter or no-filter plus blocking validation |
| `TryStepUpOverBlock` down | downward sweep | walkable ground filter |
| `FindGroundWalking` | downward/support query | walkable-only filter |
| `Recover` | overlap contacts | no movement mode decision |

## 6. Semantic Compatibility Judgment

This is the key refactor question:

```text
Can Unreal-style Walking/Falling + reactive StepUp semantics be inserted into
the current DX12EngineLab KCC without first rewriting Recover, SweepFilter,
and closest-hit SceneQuery?
```

Short answer:

```text
Yes, but only as a staged refactor with strict boundaries.

Recover can remain.
approachFilter can remain for lateral movement.
closestHit can remain for V1 reactive StepUp.

But none of these are sufficient for full Unreal-style floor quality,
perch/edge behavior, or complete seam robustness.
```

### 6.1 Recover Compatibility

Current `Recover` is compatible with `Walking/Falling` if it is treated as pose-only cleanup.

Evidence:

- pre-sweep `Recover` runs before `m_xSweep` is captured: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:117-126`
- post-sweep `Recover` runs after `m_xFinalPre` capture and is excluded from velocity: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:132-144`
- `Recover` only applies `m_currentPosition = m_currentPosition + push`: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:585-630`

Compatibility rule:

```text
Recover is allowed to change position.
Recover is not allowed to decide Walking/Falling.
Recover is not allowed to set onGround.
Recover is not allowed to edit verticalVelocity.
Recover is not allowed inside TryStepUpOverBlock unless the transaction can rollback it.
```

Why:

- `Recover` push direction comes from overlap normals and summed penetration depths.
- That normal is not necessarily a ground normal, wall normal, or movement response normal.
- Therefore `Recover` cannot be used as a semantic signal for support or stepping.

Refactor implication:

```text
Keep pre/post Recover exactly as a cleanup phase during Stage 1/2.
Do not "Unrealize" Recover into SafeMoveUpdatedComponent yet.
Only later consider integrating penetration retry into a SafeMove-like API.
```

Compatibility verdict:

| Area | Verdict | Reason |
|---|---|---|
| Walking/Falling mode split | Compatible | Recover can stay outside mode policy if pose-only |
| Reactive StepUp transaction | Compatible with constraint | Do not call Recover inside transaction unless rollback is implemented |
| Floor/support ownership | Not compatible if abused | Recover normal must not become ground/support normal |
| Velocity semantics | Currently compatible | existing writeback excludes recovery displacement |

### 6.2 approachFilter Compatibility

Current `approachFilter` is compatible with Unreal-style reactive movement only for the lateral movement stage.

Evidence:

- `SweepFilter` is a dot predicate: `Engine/Collision/SceneQuery/SqTypes.h:143-159`
- query applies normal predicate before closest selection: `Engine/Collision/SceneQuery/SqQueryLegacy.h:174-181`
- `StepMove` uses `refDir = -Normalize(remaining)`, `minDot = 1e-4`, `filterInitialOverlap = true`: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:326-337`

Semantic meaning:

```text
StepMove approachFilter asks:
"Does this hit oppose the lateral movement direction enough to block this movement?"
```

That matches reactive `StepMoveWalking` as a first pass.

It does not mean:

```text
"Is this ground?"
"Is this a legal stair landing?"
"Is this a wall that can be stepped over?"
"Is this support?"
```

Therefore `approachFilter` should not be removed, but its ownership must be narrowed.

Correct use:

```text
StepMoveWalking lateral sweep:
  use approachFilter

TryStepUpOverBlock forward sweep:
  use approach-like movement blocking filter or explicit blocking validation

TryStepUpOverBlock down sweep:
  do not use approachFilter
  use walkable/up filter

FindGroundWalking:
  do not use approachFilter
  use walkable/up filter and support rules
```

Compatibility verdict:

| Area | Verdict | Reason |
|---|---|---|
| Lateral `StepMoveWalking` | Compatible | approach normal predicate matches movement-blocking semantics |
| Initial overlap handling | Compatible after recent patch | `filterInitialOverlap=true` makes t==0 hits obey movement predicate |
| `TryStepUpOverBlock` forward | Conditionally compatible | can reuse approach-style filter, but commit still needs down-sweep validation |
| Ground/support query | Not compatible | support must be walkable/up, not approach-direction based |
| Landing validation | Not compatible | landing needs walkable and height checks |

### 6.3 closestHit Compatibility

Current KCC receives a single closest/best hit.

Evidence:

- KCC calls `SweepClosest`: `Engine/Collision/KinematicCharacterControllerLegacy.cpp:711-718`
- `CollisionWorldLegacy::SweepCapsuleClosest` delegates to closest query: `Engine/Collision/CollisionWorldLegacy.cpp:53-70`
- `SweepCapsuleClosestHit_Fast` maintains one `best` hit: `Engine/Collision/SceneQuery/SqQueryLegacy.h:121-197`
- tie-break is deterministic but still single-hit: `Engine/Collision/SceneQuery/SqQueryLegacy.h:59-73`

The question is whether reactive StepUp can work with one closest hit.

V1 answer:

```text
Yes, if the closest StepMove hit is treated only as a candidate obstacle.
```

V1 reactive StepUp can do:

```text
1. StepMove gets closest blocking hit.
2. If it is non-walkable and not near-zero, try step transaction.
3. Transaction performs its own up / forward / down sweeps.
4. Down sweep must validate walkable landing.
5. If validation fails, fallback to slide.
```

This does not require all-hits yet because the closest hit is not the final step decision. It is only the trigger.

Where closest hit becomes insufficient:

```text
1. Closest wall hit hides a slightly farther valid stair side.
2. Seam/corner produces a feature/proxy hit that is a poor step candidate.
3. Down sweep needs to choose walkable ground over closer wall/edge noise.
4. Floor quality needs perch/edge tolerance.
5. Multiple hits at similar TOI need semantic grouping, not just deterministic tie-break.
```

Current filters reduce some of this risk but do not solve it fully.

Compatibility verdict:

| Area | Verdict | Reason |
|---|---|---|
| V1 reactive StepUp trigger | Compatible | closest hit can trigger a transaction, not commit it |
| V1 wall fallback | Compatible | failed transaction falls back to slide |
| V1 landing validation | Conditionally compatible | down sweep can use walkable filter, but edge/perch quality is limited |
| Full Unreal parity | Not compatible | needs richer floor/perch/edge acceptance and probably multi-candidate policy |
| Seam robustness | Partial | deterministic closest is stable, but not necessarily semantically best |

### 6.4 Can the Refactor Be Done Without SceneQuery Refactor?

Yes for the first semantic cleanup.

Do not start with SceneQuery.

Reason:

```text
The primary semantic bug is above SceneQuery:
Walking gravity feeds StepUp pre-lift, and StepUp runs before lateral blocking evidence.
```

SceneQuery limitations matter later, but they are not the first blocker for mode cleanup.

Stage feasibility:

| Stage | Needs SceneQuery refactor? | Needs Recover rewrite? | Needs allHits? | Feasible now? |
|---|---:|---:|---:|---:|
| Add `CctMoveMode` | No | No | No | Yes |
| mode-aware gravity | No | No | No | Yes |
| remove speculative `stairLift` | No | No | No | Yes |
| walking ground maintenance | No, if using existing walkable sweep | No | No | Yes, limited |
| V1 reactive `TryStepUpOverBlock` | No | No | No | Yes, risky |
| Unreal-style floor/perch quality | Probably | Maybe not | Maybe | No |
| robust seam/perch hit policy | Yes | No | Probably | No |

### 6.5 Exact Refactor Boundary

The first refactor should only move policy ownership.

Move these responsibilities:

```text
gravity integration:
  from unconditional IntegrateVertical
  to mode-specific Walking/Falling behavior

stair attempt:
  from pre-lift StepUp phase
  to reactive StepMove hit response

ground acceptance:
  from mixed StepDown compensation/full-drop
  to explicit Walking ground validation / Falling landing
```

Do not move these yet:

```text
Recover:
  keep as pre/post pose cleanup

SceneQuery:
  keep closest sweep + filters

Hit normal generation:
  keep raw geometric hit normals

allHits / collector:
  defer until floor/perch quality stage
```

This is the realistic answer:

```text
Unreal semantics can be partially inserted now because the required first change is policy ordering, not geometry query rewrite.

But the insertion must be conservative:
Walking/Falling + reactive StepUp trigger first,
SceneQuery/floor quality later.
```

## 7. Implementation Stages

### Stage 0: Document and Freeze Current Intent

This document is Stage 0.

Do not implement `Walking/Falling` until the target semantics above are accepted.

### Stage 1: Mode Skeleton

Goal:

- add `CctMoveMode`
- add `moveMode` to `CctState`
- keep `onGround` as mirror
- add transition helpers

Suggested helpers:

```cpp
void SetModeWalking(const sq::Vec3& groundNormal);
void SetModeFalling();
bool IsWalking() const;
bool IsFalling() const;
```

Semantics:

```text
SetModeWalking:
  moveMode = Walking
  onGround = true
  verticalVelocity = 0
  verticalOffset = 0
  groundNormal = accepted ground normal

SetModeFalling:
  moveMode = Falling
  onGround = false
```

Do not remove existing `onGround` reads in the same patch.

### Stage 2: Mode-Aware Vertical Integration

Goal:

- stop accumulating gravity during `Walking`
- integrate gravity only during `Falling`
- jump transitions from `Walking` to `Falling`

Walking:

```text
if jump:
  SetModeFalling()
  verticalVelocity = jumpSpeed
else:
  verticalVelocity = 0
  verticalOffset = 0
```

Falling:

```text
verticalVelocity -= gravity * dt
verticalVelocity = clamp(...)
verticalOffset = verticalVelocity * dt
```

### Stage 3: Remove Speculative StepUp

Goal:

- remove `verticalVelocity < 0 && hasLateralIntent` stair trigger
- keep jump upward / ceiling handling only if still needed

Current code to remove:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:231-236`

After this stage, stair auto-climb may be temporarily disabled or degraded. That is acceptable if the goal is to remove wall-climb risk before restoring stair behavior.

### Stage 4: Walking Ground Maintenance

Goal:

- replace `StepDown` as "pre-lift compensation" with walking support maintenance
- ground miss transitions to `Falling` instead of unconditional full-drop teleport

Current risky behavior:

- `dropDist = m_currentStepOffset + downVelocityDt`
- no ground -> `m_currentPosition += downDelta`, `onGround = false`

근거:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp:449-563`

Target:

```text
FindGroundWalking:
  sweep down by small snap distance / stepHeight policy
  accept walkable hit only
  if accepted: snap and SetModeWalking
  if missed: SetModeFalling
```

Do not implement full Unreal edge/perch behavior in this stage.

### Stage 5: Reactive StepUp Transaction

Goal:

- restore stair auto-climb at the correct phase
- try step only after lateral blocking hit

Trigger inside `StepMoveWalking`:

```text
hit.hit
hit.t > tSkin + nearZeroEps
!IsWalkable(hit.normal)
IsWalking()
remaining length > epsilon
not initial-overlap / skin escape
```

Transaction:

```text
TryStepUpOverBlock:
  save current position and relevant state
  sweep up by stepHeight
  fail on penetration / ceiling block
  sweep forward by remaining displacement
  optionally slide once if forward hit but not stuck
  sweep down
  accept only walkable landing
  validate height <= stepHeight + epsilon
  success: commit final position and SetModeWalking
  failure: restore saved state, return false
```

Failure path:

```text
fallback to current StepMove slide/block
```

### Stage 6: Floor Quality / Perch / Edge Policy

Not a one-day task.

This stage can mine Unreal `FindFloor`, `ComputeFloorDist`, `IsWithinEdgeTolerance`, and perch behavior later.

## 7. One-Day Feasibility

### Reasonable in one day

- Stage 1: mode skeleton
- Stage 2: mode-aware gravity
- Stage 3: remove speculative stair pre-lift
- limited manual tests:
  - flat walking
  - wall forward input
  - seam/corner input
  - jump and landing
  - ledge falling

### Risky in one day

- Stage 4 if `StepDown` is heavily coupled to current behavior
- Stage 5 full reactive step-up transaction
- any all-hits collector or SceneQuery refactor
- Unreal-style edge/perch/floor-distance policy

### Recommended schedule

```text
Session A:
  Implement CctMoveMode and mode-aware vertical semantics.
  Remove speculative stairLift.
  Preserve current StepMove mitigation.

Session B:
  Split StepDown into Walking ground maintenance and Falling landing.

Session C:
  Add TryStepUpOverBlock as reactive transaction.

Session D:
  Add floor quality/perch/edge policy only after reference mining and tests.
```

## 8. Portfolio / README Wording

Safe wording:

```text
Identified and documented a KCC semantic leak where grounded gravity fed speculative StepUp.
Started refactoring toward explicit Walking/Falling movement policy and reactive StepUp.
```

Risky wording:

```text
Implemented Unreal-style CharacterMovement.
Completed KCC.
Fixed all seam/wall-climb issues.
PhysX/Unreal compatible controller.
```

## 9. Hard Rules for the Next Patch

1. Do not modify SceneQuery while introducing modes.
2. Do not change `Recover` direction policy in the same patch.
3. Do not implement `TryStepUpOverBlock` before mode semantics are explicit.
4. Do not remove `onGround` immediately; mirror it from mode.
5. Do not claim stair behavior is complete after removing speculative `StepUp`.
6. Any behavior patch must state which mode owns gravity, ground support, and velocity writeback.

## 10. Phase2 Implementation Note

2026-05-09 Phase2 direction:

- `CctMoveMode` is the movement-policy SSOT.
- `onGround` remains a compatibility mirror.
- `Walking` keeps `verticalVelocity == 0` unless a jump transitions to `Falling`.
- `StepUp` no longer performs `verticalVelocity < 0 && lateralIntent` stair lift.
- `StepDown` is the current compatibility function for Walking support validation
  and Falling landing.
- Same-tick transition continuation is not implemented here; see
  `docs/audits/kcc/03-time-budget-movement-loop-semantics.md`.
- Use `KCC Trace` and `captures/debug/kcc_trace_last.txt` to classify any
  remaining upward-pop culprit before touching `Recover`, `SceneQuery`, or
  reactive `TryStepUpOverBlock`.
