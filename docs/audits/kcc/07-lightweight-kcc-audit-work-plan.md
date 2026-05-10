# Lightweight KCC Audit-To-Work Plan

Updated: 2026-05-10

## 0. Verdict

This document is the lightweight coordination map for the next KCC sessions.

The goal is not to run a large reference study before every change. The goal is
to decide which work can be implemented directly, which work needs a short local
audit, and which work must first mine Unreal / PhysX reference contracts.

Use the current branch state as the baseline, even though known KCC bugs remain.
Do not treat the current KCC as production-ready.

## 1. Why This Document Exists

`gpt.txt` emphasizes a small-map approach:

- one session should do one lane of work
- prompts should point at exact files and functions
- context should stay small
- reusable context should be written down so the next session does not rebuild it

For KCC work, that means each session must change one semantic contract at a
time. The common failure mode is asking one agent to fix `StepMove`, `StepDown`,
`Recover`, `Falling`, `Walking`, and reference behavior in the same pass.

## 2. Current Baseline

Current KCC direction:

- `CctMoveMode` owns `Walking` / `Falling` policy.
- `onGround` is a compatibility mirror.
- `walkMove` is currently caller-provided lateral displacement with `dt` baked in.
- `Tick()` still runs a phase-linear pipeline:

```text
PreStep
IntegrateVertical
Recover
StepUp
StepMove
StepDown
PostRecover
Writeback
```

Known problem shape:

- `StepMove` is intended to be lateral movement response, but can still consume
  support / walkable normals as if they were lateral blockers.
- `StepDown` still contains both Walking support maintenance and Falling landing
  logic.
- `Falling` still needs a distinct movement path; it should not reuse a Walking
  floor-move path.
- `Recover` remains pose-only, but can visually contribute to upward pop when
  contacts are ambiguous.

Known manual cases to keep using:

- `1.4`: wall-climb / wall latch case
- `2.6`: holding forward can push upward
- jump + `W` into wall / fake floating case
- cube top escape
- flat wall slide
- flat ground walk
- falling landing

## 3. Work Categories

### A. Direct Small Implementation

Use for changes where current code and existing trace evidence are enough.

Examples:

- `StepMove` must not classify walkable/support-like hits as
  `PositiveLateralBlocker`.
- Small debug field cleanup.
- Small enum / reject reason additions.
- Guardrails that do not change public API or SceneQuery behavior.

Rules:

- Read current source first.
- Run `git status --short` first.
- Keep write set small.
- Do not touch `StepDown`, `Recover`, or SceneQuery in the same session unless
  the prompt explicitly changes scope.

### B. Short Local Audit Then Implementation

Use when the implementation changes state ownership or phase responsibility.

Examples:

- splitting `StepDown` into `FindGroundWalking` and `FindFloorForLanding`
- introducing `SimulateWalking` / `SimulateFalling` shell functions
- changing `Tick()` mode dispatch
- preserving `Writeback` velocity invariant while changing movement flow

Audit questions:

- What state does the function read?
- What state does the function write?
- Where are `position`, `velocity`, `moveMode`, `onGround`, and `groundNormal`
  mutated?
- What invariant must not change?
- What is the next implementation session's write set?

### C. Reference Contract Mining First

Use when borrowing behavior from Unreal or PhysX.

Examples:

- Unreal `MoveAlongFloor`
- Unreal `PhysFalling`
- Unreal `FindFloor` / perch / floor distance
- Unreal reactive `StepUp`
- PhysX MTD / recovery / contact offset
- SceneQuery `filteredClosestHit` / multi-hit / touch-buffer policy

Rules:

- Use `enginelab-reference-contract-miner`.
- Do not inspect or patch EngineLab production source in that reference-mining
  session.
- Every reference behavior claim must cite raw source file:line evidence.
- Extract contracts, not code.

## 4. Immediate Audit Queue

Only these three audits are required before the next major KCC refactor work.

### Audit 1: StepMoveWalking Local Audit

Output:

```text
docs/audits/kcc/08-stepmovewalking-local-audit.md
```

Target files / functions:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp`
- `StepMove`
- `classifyStepMoveRawHit`
- `queryStepMoveHit`
- `SlideAlongNormal`
- `BuildLateralStepMoveResponseNormal`
- `CctStepMoveQueryKind`
- `CctStepMoveRejectReason`

Questions:

- Can `StepMove` be narrowed to `StepMoveWalking`?
- Can it consume only non-walkable lateral blockers?
- Where does `rawWalkable` get recorded but not used as a blocker rejection?
- What trace condition proves the bug?

Expected follow-up implementation:

```text
StepMove must classify walkable/support-like hits as not usable lateral blockers.
SlideAlongNormal may only run for PositiveLateralBlocker.
```

### Audit 2: StepDown Split Local Audit

Output:

```text
docs/audits/kcc/09-stepdown-split-local-audit.md
```

Target files / functions:

- `Engine/Collision/KinematicCharacterControllerLegacy.cpp`
- `StepDown`
- `evaluateSweepFloor`
- `applyAcceptedFloor`
- `HasWalkableSupport`
- `CctFloorSemantic`
- `CctFloorSource`
- `CctFloorRejectReason`

Questions:

- Which code belongs to Walking support maintenance?
- Which code belongs to Falling landing?
- Which branches are only fallback / latch behavior?
- Which decisions currently treat `InitialOverlapSweep` as floor?
- What minimal `CctFloorResult` is needed before implementation?

Expected follow-up implementation:

```text
Introduce FindGroundWalking and FindFloorForLanding boundaries without adding
perch, MTD, multi-hit, or full Unreal FindFloor parity.
```

### Audit 3: Mode Dispatch Local Audit

Output:

```text
docs/audits/kcc/10-mode-dispatch-local-audit.md
```

Target files / functions:

- `Tick`
- `IntegrateVertical`
- `Recover`
- `Writeback`
- `SetModeWalking`
- `SetModeFalling`
- `CctState`
- `CctDebug`

Questions:

- Where should `m_xSweep` be captured after mode dispatch is introduced?
- Where should `m_xFinalPre` be captured?
- How does `Recover` remain pose-only?
- How does `onGround` remain a mirror of `moveMode`?
- What can `SimulateWalking` and `SimulateFalling` do in v1 without adding a
  time-budget loop?

Expected follow-up implementation:

```text
Tick dispatches to SimulateWalking or SimulateFalling, but time-budget
continuation remains a later session.
```

## 5. Work Order

Recommended order:

```text
1. Audit 1: StepMoveWalking local audit
2. Implement StepMoveWalking cleanup
3. Audit 2: StepDown split local audit
4. Implement FindGroundWalking / FindFloorForLanding boundary
5. Audit 3: Mode dispatch local audit
6. Implement SimulateWalking / SimulateFalling shell
7. Mine Unreal PhysFalling / MoveAlongFloor contracts if needed
8. Implement Falling diagonal move
9. Implement time-budget loop
10. Implement reactive StepUp
11. Add floor quality / perch / support point
12. Add MTD-like recovery
13. Consider SceneQuery filteredClosestHit / multi-hit only if traces prove
    closest-only cannot represent the needed fact
```

## 6. Prompt Templates

### Local Audit Prompt

```text
너는 DX12EngineLab KCC local audit reviewer다.
대화와 리포트는 한국어로 작성한다.
production source 수정 금지.

목표:
<one-sentence goal>

대상:
- <file/function>
- <file/function>

조사할 것:
- 현재 state writer/reader
- position/velocity/mode/onGround mutation
- 이 함수가 실제로 맡는 책임
- 다음 구현에서 분리해야 할 책임
- 깨지면 안 되는 invariant

출력:
docs/audits/kcc/<topic>.md 에 들어갈 형태로 작성하되,
이번 턴에는 파일 생성하지 말고 본문만 보여줘.

마지막에 다음 구현 세션 프롬프트를 짧게 제안해라.
```

### Small Implementation Prompt

```text
너는 DX12EngineLab KCC production fix agent다.
대화는 한국어로 한다.
시작 전 git status --short 확인.
수정 범위는 아래 파일로 제한한다.

목표:
<one-sentence goal>

수정 대상:
- <file/function>

금지:
- StepDown 수정 금지
- Recover 수정 금지
- SceneQuery 수정 금지
- public API 변경 금지

성공 기준:
- <trace/test condition>

수정 후:
- git diff --check
- 가능한 최소 syntax/build check
- 변경 요약
```

### Reference Check Prompt

```text
enginelab-reference-contract-miner를 사용한다.
EngineLab production source를 보지 않는다.
Unreal/PhysX raw source에서 <topic> contract만 file:line evidence로 추출한다.

출력:
- contract 요약
- source file:line
- EngineLab audit에서 확인해야 할 질문
```

## 7. Stop Conditions

Stop the session if any of these happen:

- A single patch tries to modify `StepMove`, `StepDown`, and `Recover` together.
- A KCC session tries to change SceneQuery raw kernel behavior.
- A session claims Unreal / PhysX behavior without raw source file:line evidence.
- A session changes `walkMove` input contract without a dedicated audit.
- A session adds MTD-like recovery while also changing mode dispatch.
- A session adds multi-hit / touch-buffer support before KCC stage contracts are
  stable.
- A session claims a bug is fixed without trace or test evidence.

## 8. Parallel Codex Rule

Two local Codex sessions can run in parallel only when their write sets do not
overlap.

Allowed parallel work:

- Unreal Walking reference mining and Unreal Falling reference mining
- `StepMoveWalking` local audit and `StepDown` split local audit
- documentation-only prompt preparation for different sessions

Disallowed parallel work:

- two agents editing `KinematicCharacterControllerLegacy.cpp`
- one agent changing KCC state while another changes `CctTypes.h`
- one agent implementing `SimulateWalking` while another implements
  `SimulateFalling` before the shared mode contract is written

Production KCC implementation should be sequential until `SimulateWalking`,
`SimulateFalling`, floor result, and writeback invariants are documented.

## 9. Next Action

Run Audit 1 first:

```text
docs/audits/kcc/08-stepmovewalking-local-audit.md
```

Do not start `StepDown`, `Falling`, `time-budget`, `MTD-like`, or SceneQuery
policy changes before the `StepMoveWalking` boundary is clean.
