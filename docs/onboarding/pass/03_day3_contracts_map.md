# Day3+ Contracts Map

Contract reference for Day3+ simulation and collision systems.

---

## Anchor Convention

- **Primary**: `path::symbol` (function, struct, enum)
- **Secondary** (optional): `path:line (may drift; commit 74387ff)`
- No bare line numbers without symbol context

---

## 1. Fixed-step Loop Contract

**Anchor**: `Engine/App.h::FIXED_DT`, `Engine/App.cpp::Tick()`

**Invariants**:
- Fixed timestep dt = 1/60s (0.01667s) - never varies
- Accumulator capped at 0.25s (max 15 ticks per frame)
- Physics runs in while-loop until accumulator < FIXED_DT
- Accumulator reset to 0 on Initialize()

**Constants**:
| Name | Value | Defined At |
|------|-------|------------|
| FIXED_DT | 1/60 (0.01667s) | `Engine/App.h:48` |
| Accumulator cap | 0.25s | `Engine/App.cpp:42` |
| Max ticks/frame | 15 | Derived: 0.25 / 0.01667 |

**Break Symptoms**:
- **Spiral of death**: If accumulator uncapped, low framerate causes more physics ticks, causing lower framerate
- **Physics instability**: If dt varies, collision resolution becomes inconsistent
- **Jitter**: If accumulator not consumed properly, pawn stutters

**Proof**:
1. Toggle: N/A (always active)
2. HUD: Check "Speed" field - should be stable when walking
3. Log: Add breakpoint in `Tick()` while-loop, verify dt is always FIXED_DT
4. Expected: Pawn moves at constant speed regardless of framerate

---

## 2. ControllerMode Contract

**Anchor**: `Engine/WorldState.h::ControllerMode`

**Invariants**:
- Mode persists across respawn (reset only if explicitly requested)
- Dispatch is exhaustive: all collision paths check mode
- AABB=0 uses ResolveAxis, Capsule=1 uses Sweep/Depen/StepUp
- Mode change emits `[MODE]` log

**Constants**:
| Name | Value | Defined At |
|------|-------|------------|
| AABB | 0 | `Engine/WorldState.h:19` |
| Capsule | 1 | `Engine/WorldState.h:19` |

**Break Symptoms**:
- **Wrong collision algorithm**: Capsule sweep called in AABB mode = tunneling
- **Clipping**: AABB resolution called in Capsule mode = stuck in geometry
- **Silent mode change**: If `[MODE]` log not emitted, state may be corrupt

**Proof**:
1. Toggle: F6 to switch modes
2. HUD: "Ctrl" field shows "AABB" or "Capsule"
3. Log: `[MODE] ctrl=AABB` or `[MODE] ctrl=Capsule`
4. Expected: Collision behavior changes visibly (capsule slides on corners)

---

## 3. Spatial Grid Contract

**Anchor**: `Engine/WorldState.cpp::BuildSpatialGrid()`

**Invariants**:
- Built once at Initialize() - cubes don't move
- Grid is 100x100 cells (GRID_SIZE constant)
- Cell size = 2.0 units (world range -100 to +100 maps to 0-99)
- Origin at (-100, -100) in world XZ
- Each cell contains vector of cube indices

**Constants**:
| Name | Value | Defined At |
|------|-------|------------|
| GRID_SIZE | 100 | `Engine/WorldState.h:328` |
| Cell size | 2.0 units | Derived: 200 / 100 |
| World origin | (-100, -100) | `WorldToCellX/Z()` impl |

**Break Symptoms**:
- **Missing collisions**: Cube not registered in correct cells
- **O(n^2) fallback**: If grid not built, queries check all 10000 cubes
- **Out-of-bounds**: Pawn outside [-100,100] range queries empty cells

**Proof**:
1. Toggle: N/A (always built at init)
2. HUD: "Candidates" field - should be small (3-12) not 10000
3. Log: `[Collision] Built spatial hash: 10000 cubes in 100x100 grid`
4. Expected: Walk around, candidates stays low even in dense areas

---

## 4. Support/onGround Contract

**Anchor**: `Engine/WorldState.cpp::QuerySupport()`

**Invariants**:
- SUPPORT_EPSILON = 0.05f - gap tolerance for landing detection
- Jump-frame protected: `m_justJumpedThisTick` prevents false landing
- velY zeroed on landing (prevents re-triggering gravity)
- Support sources: FLOOR=0, CUBE=1, NONE=2

**Constants**:
| Name | Value | Defined At |
|------|-------|------------|
| SUPPORT_EPSILON | 0.05f | `Engine/WorldState.cpp:1059` |
| FLOOR | 0 | `Engine/WorldState.h:16` |
| CUBE | 1 | `Engine/WorldState.h:16` |
| NONE | 2 | `Engine/WorldState.h:16` |

**Break Symptoms**:
- **Stuck in air**: SUPPORT_EPSILON too small, pawn never lands
- **Infinite falling**: velY not zeroed, gravity keeps accumulating
- **Jump not working**: onGround stuck false, jump input ignored
- **Double jump**: Jump-frame protection missing, lands same frame as jump

**Proof**:
1. Toggle: F8 for verbose HUD
2. HUD: "Support" section shows Source, Gap, Snapped
3. Log: Check "onGround" field transitions false->true on landing
4. Expected: Jump -> onGround=false -> land -> onGround=true, Gap < 0.05

---

## 5. Capsule Sweep Contract

**Anchor**: `Engine/WorldState.cpp::SweepXZ_Capsule()`

**Invariants**:
- TOI (Time of Impact) is in [0,1] range
- Skin offset (SKIN_WIDTH=0.01f) prevents surface contact
- Slide on contact: remaining velocity projected along surface normal
- MAX_SWEEPS=4 iterations for corner cases

**Constants**:
| Name | Value | Defined At |
|------|-------|------------|
| SKIN_WIDTH | 0.01f | `Engine/WorldState.cpp:1391` |
| MAX_SWEEPS | 4 | `Engine/WorldState.cpp:1392` |

**Break Symptoms**:
- **Tunneling**: TOI computed wrong, pawn passes through cube
- **Stuck on corners**: MAX_SWEEPS too low, can't resolve complex contacts
- **Jitter**: SKIN_WIDTH too large or too small, oscillating push-out
- **No slide**: Normal computation wrong, pawn stops dead on walls

**Proof**:
1. Toggle: F8 for verbose, F6 to Capsule mode
2. HUD: "Sweep" section shows TOI, Normal, Applied
3. Log: `[SWEEP] req=(x,z) cand=N hit=1 toi=T n=(nx,nz) cube=I`
4. Expected: Walk into wall -> TOI < 1.0, slide along surface

---

## 6. Depenetration Contract

**Anchor**: `Engine/WorldState.cpp::ResolveOverlaps_Capsule()`

**Invariants**:
- MAX_DEPEN_ITERS = 4 iterations max
- MIN_DEPEN_DIST = 0.001f convergence threshold
- MAX_DEPEN_CLAMP = 1.0f per-overlap and total clamp
- Early exit on convergence (push magnitude < MIN_DEPEN_DIST)

**Constants**:
| Name | Value | Defined At |
|------|-------|------------|
| MAX_DEPEN_ITERS | 4 | `Engine/WorldState.cpp:1302` |
| MIN_DEPEN_DIST | 0.001f | `Engine/WorldState.cpp:1303` |
| MAX_DEPEN_CLAMP | 1.0f | `Engine/WorldState.cpp:1304` |

**Break Symptoms**:
- **Explosion**: MAX_DEPEN_CLAMP too high, large push-out teleports pawn
- **Stuck inside geometry**: MAX_DEPEN_ITERS too low, doesn't converge
- **hitMaxIter=true**: Convergence failed, collision stats show warning

**Proof**:
1. Toggle: F8 for verbose, F6 to Capsule mode
2. HUD: "Depen" section shows Iterations, Magnitude, Clamp
3. Log: `[DEPEN] iter=N mag=M clamp=C cnt=U`
4. Expected: Walk into cube -> depenIterations < 4, Clamp=false

---

## 7. Step-up Contract

**Anchor**: `Engine/WorldState.cpp::TryStepUp_Capsule()`

**Invariants**:
- maxStepHeight = 0.3f - max obstacle height to auto-climb
- 4 phases: up probe -> forward probe -> down settle -> penetration check
- Failure mask bits: UP_BLOCKED=0x01, FWD_BLOCKED=0x02, NO_GROUND=0x04, PENETRATION=0x08
- Only triggered on wall-like collision (horizontal normal)

**Constants**:
| Name | Value | Defined At |
|------|-------|------------|
| maxStepHeight | 0.3f | `Engine/WorldState.h:245` |
| STEP_FAIL_UP_BLOCKED | 0x01 | `Engine/WorldState.h:123` |
| STEP_FAIL_FWD_BLOCKED | 0x02 | `Engine/WorldState.h:124` |
| STEP_FAIL_NO_GROUND | 0x04 | `Engine/WorldState.h:125` |
| STEP_FAIL_PENETRATION | 0x08 | `Engine/WorldState.h:126` |

**Break Symptoms**:
- **Can't climb stairs**: Step-up logic disabled or maxStepHeight too low
- **Teleport upward**: maxStepHeight too high, climbs walls
- **Fall through**: Down settle phase finds no ground
- **Stuck**: Penetration check fails, step-up rejected

**Proof**:
1. Toggle: F7 + F8 for verbose, F6 to Capsule mode
2. HUD: "StepUp" section shows OK/FAIL, Height, Mask
3. Log: `[STEP_UP] try=1 ok=1 mask=0x00 h=H cube=I pos=(x,y,z)`
4. Expected: Walk up to 0.3-height obstacle -> step-up succeeds

---

## 8. Observability Contract

**Anchor**: `Renderer/DX12/ImGuiLayer.cpp::BuildHUDContent()`

**Invariants**:
- F6 toggles ControllerMode (AABB/Capsule)
- F7 toggles step-up grid test
- F8 toggles verbose HUD sections
- HUD reflects CollisionStats struct fields
- All collision code emits logs with prefixes

**HUD Sections**:
| Section | Condition | Key Fields |
|---------|-----------|------------|
| Toggles | Always | Ctrl, GridTest, Verbose |
| Position | Always | Pos X/Y/Z, Speed, OnGround |
| Collision | Always | Candidates, Contacts |
| Depen | Capsule + verbose | Iterations, Magnitude, Clamp |
| Sweep | Capsule + verbose | TOI, Normal, Applied |
| StepUp | Capsule + verbose | OK/FAIL, Height, Mask |
| Support | Verbose | Source, Gap, Snapped |
| MTV Debug | Verbose | PenX/Z, Axis, Magnitude |

**Log Prefixes**:
| Prefix | Source Function |
|--------|-----------------|
| `[Collision]` | `BuildSpatialGrid()`, `ResolveAxis()` |
| `[MODE]` | `ToggleControllerMode()` |
| `[SWEEP]` | `SweepXZ_Capsule()` |
| `[DEPEN]` | `ResolveOverlaps_Capsule()` |
| `[STEP_UP]` | `TryStepUp_Capsule()` |
| `[KILLZ]` | `CheckKillZ()` |

**Break Symptoms**:
- **Blind debugging**: F8 not working, no verbose sections
- **Missing diagnostics**: CollisionStats fields not populated
- **Stale HUD**: BuildSnapshot() not called each frame

**Proof**:
1. Toggle: F6 -> "Ctrl" changes, F7 -> "GridTest" changes, F8 -> more sections appear
2. HUD: All fields update in real-time
3. Log: Check VS Output for prefixes during gameplay
4. Expected: Each collision event produces corresponding log and HUD update

---

## Quick Reference: All Constants

| Constant | Value | Location |
|----------|-------|----------|
| FIXED_DT | 1/60 (0.01667s) | `Engine/App.h::FIXED_DT` |
| Accumulator cap | 0.25s | `Engine/App.cpp::Tick()` |
| GRID_SIZE | 100 | `Engine/WorldState.h::GRID_SIZE` |
| Cell size | 2.0 units | `Engine/WorldState.cpp::WorldToCellX()` |
| SUPPORT_EPSILON | 0.05f | `Engine/WorldState.cpp::QuerySupport()` |
| SKIN_WIDTH (XZ) | 0.01f | `Engine/WorldState.cpp::SweepXZ_Capsule()` |
| sweepSkinY | 0.01f | `Engine/WorldState.h::WorldConfig::sweepSkinY` |
| MAX_SWEEPS | 4 | `Engine/WorldState.cpp::SweepXZ_Capsule()` |
| MAX_DEPEN_ITERS | 4 | `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` |
| MAX_DEPEN_CLAMP | 1.0f | `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` |
| MIN_DEPEN_DIST | 0.001f | `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` |
| maxStepHeight | 0.3f | `Engine/WorldState.h::WorldConfig::maxStepHeight` |
| killZ | -50.0f | `Engine/WorldState.h::WorldConfig::killZ` |

---

## Quick Reference: CollisionStats Fields

| Field | Set By | Indicates |
|-------|--------|-----------|
| candidatesChecked | QuerySpatialHash | Cubes in range |
| contacts | ResolveAxis/Sweep | Actual collisions |
| hitMaxIter | ResolveAxis | Failed to converge |
| depenIterations | ResolveOverlaps_Capsule | Iteration count |
| depenClampTriggered | ResolveOverlaps_Capsule | Magnitude clamped |
| sweepHit | SweepXZ_Capsule | Collision detected |
| sweepTOI | SweepXZ_Capsule | Time of impact |
| stepSuccess | TryStepUp_Capsule | Step-up worked |
| stepFailMask | TryStepUp_Capsule | Failure reason bits |
| supportSource | QuerySupport | FLOOR/CUBE/NONE |
| snappedThisTick | QuerySupport | Snap occurred |
| xzStillOverlapping | ResolveXZ_MTV | Residual penetration |
