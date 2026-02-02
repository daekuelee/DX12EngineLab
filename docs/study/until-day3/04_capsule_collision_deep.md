# Capsule Collision Pipeline Deep Study

## Purpose

Document the Day3 capsule collision pipeline with refactor-safe contracts.
Covers depenetration, sweep+slide, step-up, and support query stages.

---

## Mental Model

```
TickFixed (Capsule Mode)
    │
    ├─ ResolveOverlaps_Capsule()      ← DEPEN: push out of geometry
    │   └─ up to 4 iterations, clamp per-overlap to 1.0
    │
    ├─ SweepY_Capsule()               ← Y movement (gravity/jump)
    │   └─ skin=0.01, single sweep
    │
    ├─ SweepXZ_Capsule()              ← XZ movement + slide
    │   └─ up to 4 sweeps, skin=0.01, OVERCLIP=1.001
    │
    ├─ ResolveXZ_Capsule_Cleanup()    ← residual XZ penetration
    │
    ├─ TryStepUp_Capsule()            ← IF sweepHit && IsWallLike
    │   ├─ UP: ProbeY (max 0.3 up, must reach 90%)
    │   ├─ FWD: ProbeXZ (must achieve >10% motion)
    │   ├─ DOWN: ProbeY (find ground)
    │   └─ PEN: ScanMaxXZPenetration (must be < 0.01)
    │
    ├─ Iteration loop (MAX=8)         ← residual resolution
    │   └─ XZ cleanup + Y resolve alternating
    │
    ├─ QuerySupport()                 ← ground detection
    │   └─ SUPPORT_EPSILON=0.05
    │
    └─ Populate CollisionStats        ← all stage outputs
```

---

## Code Anchors

| Stage | Anchor | Key Constants | Stats Written |
|-------|--------|---------------|---------------|
| Depen | `WorldState.cpp::ResolveOverlaps_Capsule` | MAX_ITERS=4, CLAMP=1.0 | depen* (6 fields) |
| SweepY | `WorldState.cpp::SweepY_Capsule` | SKIN=0.01 | sweepY* (5 fields) |
| SweepXZ | `WorldState.cpp::SweepXZ_Capsule` | MAX_SWEEPS=4, SKIN=0.01 | sweep* (10 fields) |
| Cleanup | `WorldState.cpp::ResolveXZ_Capsule_Cleanup` | MAX=1.6 | (none) |
| StepUp | `WorldState.cpp::TryStepUp_Capsule` | maxStep=0.3 | step* (5 fields) |
| Support | `WorldState.cpp::QuerySupport` | EPS=0.05 | support* (5 fields) |
| Geometry | `WorldState.h::WorldConfig` | r=1.4, hh=1.1 | - |

---

## Contracts

### Contract A: Depenetration

**Invariants:**
- [ ] Runs BEFORE any sweep
- [ ] Max 4 iterations (`MAX_DEPEN_ITERS`)
- [ ] Per-overlap push clamped to 1.0 (`MAX_DEPEN_CLAMP`)
- [ ] Total push per iteration clamped to 2.0 (`MAX_TOTAL_CLAMP`)
- [ ] Ignores overlaps < 0.001 (`MIN_DEPEN_DIST`)

**Break Symptoms:**
- Pawn teleports (clamp too high or missing)
- Pawn stuck in geometry (iterations too low)
- [Cookbook #5](../../onboarding/pass/04_day3_failure_cookbook.md#5-pawn-explodes--teleports-on-collision)
- [Cookbook #11](../../onboarding/pass/04_day3_failure_cookbook.md#11-depen-iterations-hit-max-without-convergence)

**Guardrails:**
- Do NOT increase MAX_DEPEN_CLAMP above 2.0
- Do NOT reduce MAX_DEPEN_ITERS below 3

**Proof:**
```bash
rg "MAX_DEPEN_ITERS|MAX_DEPEN_CLAMP|MAX_TOTAL_CLAMP" Engine/WorldState.cpp
```

---

### Contract B: SweepXZ

**Invariants:**
- [ ] Runs AFTER depenetration
- [ ] Max 4 sweep iterations (`MAX_SWEEPS`)
- [ ] Skin width = 0.01 (`SKIN_WIDTH`)
- [ ] Slide uses OVERCLIP = 1.001 for corner handling
- [ ] TOI in range [0, 1], 0 means immediate collision

**Break Symptoms:**
- Pawn clips through cube (TOI wrong)
- Pawn stuck (slide not computed)
- [Cookbook #2](../../onboarding/pass/04_day3_failure_cookbook.md#2-pawn-clips-through-cube-capsule-mode)

**Guardrails:**
- Do NOT remove depenetration before sweep
- Do NOT change SKIN_WIDTH without testing wall slide

**Proof:**
```bash
rg "MAX_SWEEPS|SKIN_WIDTH|OVERCLIP" Engine/WorldState.cpp
```

---

### Contract C: Corner/Edge Handling

**Invariants:**
- [ ] ClipVelocityXZ projects remainder along wall
- [ ] OVERCLIP (1.001) prevents parallel-to-wall oscillation
- [ ] Multiple sweeps handle corner reflection

**Break Symptoms:**
- Jitter against wall (OVERCLIP wrong)
- Stuck at corners (only 1 sweep iteration)
- [Cookbook #6](../../onboarding/pass/04_day3_failure_cookbook.md#6-pawn-jitters-against-wall)

**Guardrails:**
- Do NOT reduce MAX_SWEEPS below 2
- Do NOT remove OVERCLIP

**Proof:**
```bash
rg "ClipVelocityXZ|OVERCLIP" Engine/WorldState.cpp
```

---

### Contract D: Step-Up

**Invariants:**
- [ ] Only attempts if: sweepHit AND IsWallLike (normalXZ > 0.8)
- [ ] Phase order: UP → FWD → DOWN → PEN check
- [ ] UP must reach 90% of maxStepHeight (0.27 for 0.3 max)
- [ ] FWD must achieve >10% of requested motion
- [ ] DOWN uses SETTLE_EXTRA = 2*sweepSkinY
- [ ] PEN check: residual XZ pen < 0.01
- [ ] HOLE_EPSILON (0.05) prevents stepping into holes

**Break Symptoms:**
- Step-up fails on valid stairs (threshold too high)
- Step-up succeeds on walls (IsWallLike wrong)
- [Cookbook #7](../../onboarding/pass/04_day3_failure_cookbook.md#7-step-up-fails-on-valid-stairs)

**StepFailMask Values:**
| Mask | Value | Meaning |
|------|-------|---------|
| UP_BLOCKED | 0x01 | Couldn't rise enough |
| FWD_BLOCKED | 0x02 | No forward clearance |
| NO_GROUND | 0x04 | No surface to land on |
| PENETRATION | 0x08 | Final pose overlaps |

**Guardrails:**
- Do NOT change phase order
- Do NOT increase maxStepHeight above 0.5

**Proof:**
```bash
rg "TryStepUp_Capsule|StepFailMask|maxStepHeight" Engine/WorldState.cpp Engine/WorldState.h
```

---

### Contract E: Support/onGround

**Invariants:**
- [ ] QuerySupport runs AFTER all movement resolved
- [ ] SUPPORT_EPSILON = 0.05 (gap tolerance)
- [ ] Support sources: FLOOR, CUBE, NONE
- [ ] Y snap only if gap < SUPPORT_EPSILON
- [ ] m_justJumpedThisTick prevents immediate re-snap after jump

**Break Symptoms:**
- Floating after landing (epsilon too small)
- Can't jump (onGround stuck false)
- [Cookbook #4](../../onboarding/pass/04_day3_failure_cookbook.md#4-jump-doesnt-work--onground-stuck-false)

**Guardrails:**
- Do NOT reduce SUPPORT_EPSILON below 0.01
- Do NOT remove m_justJumpedThisTick protection

**Proof:**
```bash
rg "SUPPORT_EPSILON|QuerySupport|m_justJumpedThisTick" Engine/WorldState.cpp
```

---

### Contract F: Observability

**Invariants:**
- [ ] CollisionStats reset at TickFixed start
- [ ] Each stage writes its designated fields
- [ ] HUD capsule sections only show when controllerMode==Capsule
- [ ] F8 verbose reveals debug sections

**CollisionStats Fields by Stage:**
| Stage | Fields |
|-------|--------|
| Depen | depenApplied, depenTotalMag, depenClampTriggered, depenMaxSingleMag, depenOverlapCount, depenIterations |
| SweepXZ | sweepHit, sweepTOI, sweepHitCubeIdx, sweepCandCount, sweepReq/Applied/SlideDx/Dz, sweepNormalX/Z |
| SweepY | sweepYHit, sweepYTOI, sweepYHitCubeIdx, sweepYReq/AppliedDy |
| StepUp | stepTry, stepSuccess, stepFailMask, stepHeightUsed, stepCubeIdx |
| Support | supportSource, supportY, supportCubeId, supportGap, snappedThisTick |

**Break Symptoms:**
- HUD shows stale data (stats not reset)
- [Cookbook #10](../../onboarding/pass/04_day3_failure_cookbook.md#10-hud-shows-stalewrong-collision-data)

**Proof:**
```bash
rg "m_collisionStats\s*=" Engine/WorldState.cpp  # Reset site
rg "CollisionStats" Engine/WorldState.h  # Struct definition
```

---

## Failure Linkage

### → Cookbook #2: Pawn Clips (Capsule)

**Causal chain:** SweepXZ_Capsule TOI wrong OR depenetration skipped → pawn inside cube

**Contract violated:** B (SweepXZ)

**Observable:** HUD Sweep TOI=1.0 when should hit; Log `[SWEEP] hit=0`

### → Cookbook #6: Pawn Jitters Against Wall

**Causal chain:** SKIN_WIDTH or OVERCLIP causing oscillation → position flips each tick

**Contract violated:** C (Corner/Edge)

**Observable:** HUD Pos X/Z oscillates by ~0.01; Sweep TOI alternates

### → Cookbook #11: Depen Hits Max Iters

**Causal chain:** Multiple overlaps with opposing normals → pushes cancel out → no convergence

**Contract violated:** A (Depenetration)

**Observable:** HUD Depen iters=4; Log `[DEPEN] DONE iters=4` with pawn still stuck

### → Cookbook #12: Sweep TOI=0

**Causal chain:** Start inside geometry → sweep returns immediate collision → can't move

**Contract violated:** A→B ordering (depen before sweep)

**Observable:** HUD Sweep TOI=0.0000; Log `[SWEEP] toi=0.0000`

---

## Proof Steps

### Docs-Only Verification
```bash
# 1. Verify depenetration constants
rg "MAX_DEPEN_ITERS|MIN_DEPEN_DIST|MAX_DEPEN_CLAMP" Engine/WorldState.cpp

# 2. Verify sweep constants
rg "MAX_SWEEPS.*=|SKIN_WIDTH.*=|OVERCLIP" Engine/WorldState.cpp

# 3. Verify step-up constants
rg "maxStepHeight|HOLE_EPSILON|SETTLE_EXTRA" Engine/WorldState.cpp

# 4. Verify support epsilon
rg "SUPPORT_EPSILON" Engine/WorldState.cpp

# 5. Verify stage ordering in TickFixed
rg -n "ResolveOverlaps_Capsule|SweepY_Capsule|SweepXZ_Capsule|TryStepUp_Capsule|QuerySupport" Engine/WorldState.cpp
```

### Runtime Verification
1. **F6 to Capsule mode**: HUD shows capsule sections
2. **Walk into wall**: Sweep TOI < 1.0, slide computed
3. **Walk into stair (F7)**: StepUp try=1, ok=1, h≈0.2-0.3
4. **Walk into tall wall**: StepUp try=1, ok=0, mask=0x01 (UP_BLOCKED)
5. **F8 verbose**: Depen/Sweep/StepUp sections visible

---

## Refactor Checklist

**Must Preserve:**
| Item | Reason |
|------|--------|
| Depen before sweep | Sweep assumes no initial overlap |
| MAX_DEPEN_ITERS >= 3 | Convergence guarantee |
| MAX_SWEEPS >= 2 | Corner handling |
| Step-up phase order | Each phase depends on previous |
| SUPPORT_EPSILON >= 0.01 | Ground snap tolerance |
| Stats reset at tick start | Fresh diagnostics |
| Stats write by each stage | HUD observability |

**Safe to Change:**
- Internal helper implementations (same contract)
- Capsule radius/halfHeight (tuning)
- maxStepHeight <= 0.5 (tuning)
- Log formats (not contract)
- HUD layout (display only)

---

## See Also

- [03_fixedstep_pipeline_deep.md](03_fixedstep_pipeline_deep.md) - Fixed-step contracts
- [04_day3_failure_cookbook.md](../../onboarding/pass/04_day3_failure_cookbook.md) - Failure patterns
