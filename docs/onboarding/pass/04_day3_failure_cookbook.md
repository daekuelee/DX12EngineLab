# Day3+ Failure Cookbook

Quick-reference failure patterns with proof steps.

---

## 1. Pawn Clips Through Cube (AABB Mode)

**Reproduction**: Walk at cube corner diagonally in AABB mode (F6 to AABB)

**Suspects**:
1. `Engine/WorldState.cpp::ResolveAxis()` - Single-axis resolution insufficient for corner
2. `Engine/WorldState.cpp::ResolveXZ_MTV()` - MTV choosing wrong axis
3. `Engine/WorldState.cpp::QuerySpatialHash()` - Cube not in candidate list

**Proof Steps**:
1. F6 to AABB mode, F8 for verbose
2. Walk diagonally into cube corner
3. HUD: Check "Candidates" > 0, "Contacts" > 0
4. Log: Look for `[Collision] cube=I axis=X/Z pen=P`
5. Expected: Contacts > 0, MTV resolves to single axis
6. Actual (if broken): Contacts = 0 OR pawn inside cube

**Guardrails**: Do NOT change spatial grid cell size or GRID_SIZE

---

## 2. Pawn Clips Through Cube (Capsule Mode)

**Reproduction**: Walk fast at cube in Capsule mode (F6 to Capsule)

**Suspects**:
1. `Engine/WorldState.cpp::SweepXZ_Capsule()` - TOI computation wrong
2. `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` - Depenetration insufficient
3. `Engine/WorldState.h::WorldConfig::walkSpeed` - Speed too high for sweep resolution

**Proof Steps**:
1. F6 to Capsule mode, F8 for verbose
2. Sprint (Shift) into cube
3. HUD: Check "Sweep" section - TOI should be < 1.0
4. Log: Look for `[SWEEP] hit=1 toi=T`
5. Expected: TOI computed, pawn stops before cube
6. Actual (if broken): TOI=1.0 (no hit) OR pawn inside cube

**Guardrails**: Do NOT increase walkSpeed above 60 units/sec without sweep review

---

## 3. Pawn Stuck Floating in Air

**Reproduction**: Jump and land near cube edge

**Suspects**:
1. `Engine/WorldState.cpp::QuerySupport()` - SUPPORT_EPSILON too small
2. `Engine/WorldState.cpp::QuerySupport()` - Cube support detection failing
3. `Engine/WorldState.h::PawnState::onGround` - Flag stuck false

**Proof Steps**:
1. F8 for verbose
2. Jump onto cube, land near edge
3. HUD: Check "Support" section - Source, Gap
4. HUD: Check "onGround" field
5. Expected: Source=CUBE or FLOOR, Gap < 0.05, onGround=true
6. Actual (if broken): Source=NONE, onGround=false, pawn floats

**Guardrails**: Do NOT reduce SUPPORT_EPSILON below 0.01f

---

## 4. Jump Doesn't Work / onGround Stuck False

**Reproduction**: Walk around, try to jump

**Suspects**:
1. `Engine/WorldState.cpp::QuerySupport()` - Support query returns NONE
2. `Engine/WorldState.cpp::TickFixed()` - Jump input ignored when onGround=false
3. `Engine/WorldState.h::m_justJumpedThisTick` - Jump protection not cleared

**Proof Steps**:
1. F8 for verbose
2. Try to jump on flat floor
3. HUD: Check "onGround" before jump attempt
4. HUD: Check "Support" section - Source should be FLOOR
5. Expected: onGround=true, jump launches pawn
6. Actual (if broken): onGround=false, Space key does nothing

**Guardrails**: Do NOT remove m_justJumpedThisTick protection

---

## 5. Pawn Explodes / Teleports on Collision

**Reproduction**: Walk into geometry corner or seam

**Suspects**:
1. `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` - MAX_DEPEN_CLAMP too high
2. `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` - Convergence loop overshooting
3. `Engine/WorldState.cpp::ResolveAxis()` - Cumulative penetration adds up

**Proof Steps**:
1. F6 to Capsule, F8 for verbose
2. Walk into corner where multiple cubes meet
3. HUD: Check "Depen" section - Magnitude, Clamp
4. Log: Look for `[DEPEN] mag=M` with M > 1.0
5. Expected: Magnitude < 1.0, smooth push-out
6. Actual (if broken): Magnitude spikes, pawn teleports

**Guardrails**: Do NOT increase MAX_DEPEN_CLAMP above 2.0f

---

## 6. Pawn Jitters Against Wall

**Reproduction**: Walk into wall and hold direction

**Suspects**:
1. `Engine/WorldState.cpp::SweepXZ_Capsule()` - SKIN_WIDTH causing oscillation
2. `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` - Push direction flipping
3. `Engine/WorldState.cpp::SweepXZ_Capsule()` - Slide computation wrong

**Proof Steps**:
1. F6 to Capsule, F8 for verbose
2. Walk into wall, hold W key
3. HUD: Watch "Pos X/Z" - should be stable, not oscillating
4. HUD: Check "Sweep" TOI - should be small but stable
5. Expected: Pawn slides smoothly along wall
6. Actual (if broken): Position oscillates by ~0.01 units

**Guardrails**: Do NOT change SKIN_WIDTH without testing wall slide

---

## 7. Step-up Fails on Valid Stairs

**Reproduction**: Walk up to 0.2-height step, pawn blocked

**Suspects**:
1. `Engine/WorldState.cpp::TryStepUp_Capsule()` - Step-up not triggered
2. `Engine/WorldState.cpp::IsWallLike()` - Normal not detected as wall-like
3. `Engine/WorldState.h::WorldConfig::maxStepHeight` - Value too low

**Proof Steps**:
1. F6 to Capsule, F7 for grid test, F8 for verbose
2. Walk into stair step (should be < 0.3 height)
3. HUD: Check "StepUp" section - Try, OK/FAIL, Mask
4. Log: Look for `[STEP_UP] try=1 ok=0 mask=0xNN`
5. Expected: try=1, ok=1, pawn climbs step
6. Actual (if broken): try=0 OR mask shows failure reason

**Guardrails**: Do NOT disable enableStepUp without user consent

---

## 8. Step-up Succeeds on Too-High Obstacle

**Reproduction**: Pawn climbs obstacle > 0.3 units

**Suspects**:
1. `Engine/WorldState.cpp::TryStepUp_Capsule()` - maxStepHeight check wrong
2. `Engine/WorldState.cpp::ProbeY()` - Up probe allowing too much movement
3. `Engine/WorldState.h::WorldConfig::maxStepHeight` - Value too high

**Proof Steps**:
1. F6 to Capsule, F8 for verbose
2. Walk into known 0.5-height obstacle
3. HUD: Check "StepUp" Height field
4. Log: Look for `[STEP_UP] try=1 ok=1 h=H` with H > 0.3
5. Expected: try=1, ok=0, mask=UP_BLOCKED or similar
6. Actual (if broken): pawn climbs, h > maxStepHeight

**Guardrails**: Do NOT increase maxStepHeight above 0.5f

---

## 9. Respawn Loop (KillZ Triggers Repeatedly)

**Reproduction**: Pawn falls off edge, respawns, immediately falls again

**Suspects**:
1. `Engine/WorldState.h::WorldConfig::spawnX/Y/Z` - Spawn position over void
2. `Engine/WorldState.cpp::CheckKillZ()` - killZ threshold wrong
3. `Engine/WorldState.cpp::Initialize()` - Floor bounds incorrect

**Proof Steps**:
1. Jump off floor edge deliberately
2. Watch respawn behavior
3. HUD: Note "Pos Y" after respawn
4. Log: Look for repeated `[KILLZ] #N at pos=(x,y,z)`
5. Expected: Respawn at spawnY=5.0, fall to floor, stable
6. Actual (if broken): Respawn count increases rapidly

**Guardrails**: Do NOT change spawnX/Z without verifying floor bounds

---

## 10. HUD Shows Stale/Wrong Collision Data

**Reproduction**: Collision occurs but HUD doesn't update

**Suspects**:
1. `Renderer/DX12/ImGuiLayer.cpp::BuildHUDContent()` - Not using latest snapshot
2. `Engine/WorldState.cpp::BuildSnapshot()` - CollisionStats not copied
3. `Engine/App.cpp::Tick()` - Snapshot not refreshed each frame

**Proof Steps**:
1. Walk into cube, observe collision
2. Compare HUD "Contacts" to expected behavior
3. Check VS Output for collision logs
4. If logs show collision but HUD shows 0, snapshot is stale
5. Expected: HUD Contacts > 0 when visibly colliding
6. Actual (if broken): HUD shows 0 or previous frame's data

**Guardrails**: Do NOT cache HUDSnapshot across frames

---

## 11. [DEPEN] Iterations Hit Max Without Convergence

**Reproduction**: Get stuck in complex geometry intersection

**Suspects**:
1. `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` - Overlaps not reducing
2. `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` - Push direction canceling out
3. Multiple cubes with opposing normals creating deadlock

**Proof Steps**:
1. F6 to Capsule, F8 for verbose
2. Navigate to corner where 3+ cubes meet
3. HUD: Check "Depen" Iterations - should be < 4
4. HUD: Check if "hitMaxIter" shown (verbose mode)
5. Log: `[DEPEN] DONE iters=4` with residual overlaps
6. Expected: Iterations < 4, clean exit
6. Actual (if broken): Iterations = 4, pawn still overlapping

**Guardrails**: Do NOT reduce MAX_DEPEN_ITERS below 3

---

## 12. Sweep TOI=0 (Immediate Collision)

**Reproduction**: Start inside geometry or at exact surface contact

**Suspects**:
1. `Engine/WorldState.cpp::SweepXZ_Capsule()` - Already overlapping at sweep start
2. `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` - Depenetration didn't run first
3. Spawn position inside cube

**Proof Steps**:
1. F6 to Capsule, F8 for verbose
2. Move and observe sweep behavior
3. HUD: Check "Sweep" TOI - should be > 0
4. Log: Look for `[SWEEP] toi=0.0000`
5. Expected: TOI > 0 (some movement before hit)
6. Actual (if broken): TOI = 0, pawn can't move

**Guardrails**: Do NOT remove depenetration pass before sweep

---

## Diagnostic Quick Reference

### Toggle Keys

| Key | Effect |
|-----|--------|
| F6 | Toggle ControllerMode (AABB/Capsule) |
| F7 | Toggle step-up grid test |
| F8 | Toggle verbose HUD sections |

### Log Prefixes

| Prefix | What to Look For |
|--------|------------------|
| `[Collision]` | Spatial grid build, per-axis resolution |
| `[MODE]` | Controller mode changes |
| `[SWEEP]` | XZ sweep results (toi, normal, cube) |
| `[DEPEN]` | Depenetration iterations and magnitude |
| `[STEP_UP]` | Step-up attempts and results |
| `[KILLZ]` | Respawn triggers |

### HUD Fields to Watch

| Symptom | Watch These Fields |
|---------|-------------------|
| Clipping | Candidates, Contacts, TOI |
| Floating | onGround, Support Source/Gap |
| Explosion | Depen Magnitude, Clamp |
| Jitter | Pos X/Z stability, TOI stability |
| Step-up issues | StepUp Try/OK/Mask |
