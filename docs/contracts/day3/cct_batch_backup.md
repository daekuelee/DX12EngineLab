# CCT Batch Backup

Updated: 2026-02-25 (local session backup)

## SSOT
- Primary plan source: `gpt.txt`
- This file is a restart-safe mirror for next-session handoff.
- Execution detail source: `docs/contracts/day3/cct_execution_playbook.md`

## Locked Batch Order
1. Batch 1: Callback seam + query debug wiring
2. Batch 2: Quick HitPolicy v1
3. Batch 3: OverlapSet diff v1
4. Batch 4: UE-deep HitPolicy v2
5. Batch 5: Owner/Character transform callback boundary
6. Batch 6: Primitive registry/ref remap cleanup

## Current State
- Batch 1: completed
- Batch 2: in progress (next start point)

## Batch 1 Applied Files
- `Engine/Collision/SceneQuery/SqCallbacks.h`
- `Engine/Collision/SceneQuery/SqQuery.h`
- `Engine/Collision/CollisionWorld.h`
- `Engine/Collision/CollisionWorld.cpp`
- `Engine/Collision/KinematicCharacterController.h`
- `Engine/Collision/KinematicCharacterController.cpp`

## Next Start (Batch 2)
- Implement hit classification behavior (`None`, `Touch`, `Block`) for real query flow.
- Define initial-overlap (`t==0`) handling policy clearly for movement sweeps.
- Keep deterministic blocking selection (`BetterHit` tie-break cascade).
- Keep patch scope minimal and local to hit policy path.
- Follow patch units/acceptance criteria in execution playbook.

## Resume Prompt
Use this sentence at next startup:

`gpt.txt SSOT 기준으로 진행. Batch1 완료 상태에서 Batch2(Quick HitPolicy v1: None/Touch/Block, initial-overlap, deterministic blocker)부터 시작. Ref/UE는 read-only.`
