# Unreal Policy Reference Plan For KCC Audits

Updated: 2026-05-10

## Verdict

Do not put detailed Unreal analysis in `AGENTS.md`.

`AGENTS.md` is for repo-wide behavior rules and navigation. The reusable Unreal
movement-policy evidence must live in `docs/reference/unreal/contracts/`, and
EngineLab comparison audits must live in `docs/audits/kcc/`.

## Why This Exists

The current KCC work needs Unreal as a movement-policy reference for:

- Walking floor movement
- Falling movement and landing continuation
- Floor finding
- Edge / perch / floor-distance acceptance
- Reactive step-up
- Start-penetration handling inside safe movement

But earlier notes mixed three different trust levels:

- raw source anchors
- generated context notes
- EngineLab audit conclusions

That is unsafe. Future sessions should not have to rediscover which level of
evidence they are using.

## Storage Model

Use this split:

| Kind | Store Here | Contains | Must Not Contain |
|---|---|---|---|
| Source discovery | `docs/reference/unreal/source-discovery.md` | where raw Unreal source is located | behavior conclusions |
| Unreal contracts | `docs/reference/unreal/contracts/` | raw-source-backed behavior contracts | EngineLab patch plans |
| EngineLab audits | `docs/audits/kcc/` | comparison against current EngineLab code | copied Unreal code |
| Session routing | `docs/agent-context/cct-refactor.md` | short links and current lane status | long analysis |

## Required Contract Mining Before Claims

Before saying "Unreal does X" in an EngineLab audit or source anchor, mine one of
these contracts:

| Topic | Contract File | Raw Source Anchors |
|---|---|---|
| Walking movement | `docs/reference/unreal/contracts/character-movement-walking.md` | `CharacterMovementComponent.cpp:5457`, `:5554` |
| Falling movement | `docs/reference/unreal/contracts/character-movement-falling.md` | `CharacterMovementComponent.cpp:4804` |
| Floor / perch / edge | `docs/reference/unreal/contracts/floor-find-perch-edge.md` | `CharacterMovementComponent.cpp:6941`, `:6949`, `:7100`, `:7352`, `:7380` |
| Reactive step-up | `docs/reference/unreal/contracts/reactive-stepup.md` | `CharacterMovementComponent.cpp:7450` |
| Safe move / penetration | `docs/reference/unreal/contracts/safemove-penetration.md` | `MovementComponent.cpp:558`, `:624` |

Source root:

```text
.ref/UnrealEngine/.ref/UnrealEngine
```

Trimmed search-hint files:

```text
UnrealFork/kinetic.cpp
UnrealFork/MovementComponent.cpp
UnrealFork/*.map.md
UnrealFork/*.context.md
```

The trimmed files are useful for navigation, but raw source should be checked
before accepting behavior claims.

## Audit Sequence

Run these in order when the KCC work needs Unreal comparison:

1. Mine the smallest Unreal contract card needed for the current question.
2. Run a local EngineLab audit against current KCC source.
3. Write the audit under `docs/audits/kcc/`.
4. Only then plan a production source patch.

Do not combine all Unreal contracts into one giant pass unless the task is
explicitly a reference-mining session.

## Immediate Use For Current KCC Work

The next KCC audits should use this order:

1. `StepMoveWalking` audit
   - likely only needs current EngineLab traces and source
   - Unreal `MoveAlongFloor` contract is useful but not mandatory for the small
     blocker/walkable classification cleanup
2. `StepDown` split audit
   - needs `floor-find-perch-edge.md` if it claims Unreal-style floor quality
   - can still split `FindGroundWalking` / `FindFloorForLanding` without full
     perch parity
3. `Mode Dispatch` audit
   - needs `character-movement-walking.md` and `character-movement-falling.md`
     only if it claims Unreal-style mode behavior
4. Reactive `StepUp`
   - must mine `reactive-stepup.md` first
5. MTD / recovery
   - should use PhysX as the primary mechanics reference, not Unreal

## What To Avoid

- Do not update `AGENTS.md` with detailed Unreal findings.
- Do not call `UnrealFork/*.context.md` authoritative.
- Do not claim "Unreal-style KCC" after only splitting modes.
- Do not mix Unreal movement policy with PhysX geometry mechanics without naming
  the layer boundary.
- Do not copy Unreal implementation code.

## Next Prompt To Mine A Contract

Use this prompt shape:

```text
Use enginelab-reference-contract-miner.
Mine only Unreal <topic> contract from raw source.
Do not inspect EngineLab production source.
Write the result to docs/reference/unreal/contracts/<file>.md.
Every behavior claim must cite raw source file:line evidence.
Context files under UnrealFork/ are search hints only.
```

## Next Prompt To Audit EngineLab Against A Contract

Use this prompt shape:

```text
Audit current EngineLab KCC <topic> against
docs/reference/unreal/contracts/<file>.md.
Do not modify production source.
Write the audit to docs/audits/kcc/<audit-file>.md.
Every EngineLab behavior claim must cite file:line evidence.
Every Unreal comparison must cite the contract card or raw source line.
```
