# Day3+ System Map

Navigation map for Day3+ simulation and collision systems. Covers ownership, dataflow, and observation points.

> **Scope**: Overview navigation only. Algorithm deep-dives deferred to future PR#DOCS-3 study pack.

---

## Purpose

This document helps you:
- Navigate the Day3+ codebase efficiently
- Understand ownership and data flow
- Know where to observe runtime behavior
- Find the right file/symbol for any question

---

## Ownership Hierarchy

```
App (Engine/App.cpp)
├── WorldState (Engine/WorldState.cpp) - owns simulation state
│   ├── PawnState - position, velocity, yaw/pitch
│   ├── WorldConfig - gravity, jump, walkSpeed constants
│   ├── CollisionStats - per-tick metrics for HUD
│   ├── m_spatialGrid[100][100] - cube lookup acceleration
│   ├── m_extras[] - extra colliders (ceiling, stairs)
│   └── m_controllerMode - AABB or Capsule
├── InputSampler (implicit in Tick) - keyboard/mouse → InputState
└── Dx12Context (Renderer/DX12/) - receives snapshots from WorldState
    └── ImGuiLayer - displays HUDSnapshot diagnostics
```

### Key Ownership Rules

1. **WorldState owns all simulation state** - Pawn position/velocity never modified outside WorldState
2. **Renderer receives snapshots** - `BuildSnapshot()` creates read-only HUD data
3. **Input flows one-way** - App samples input, passes to WorldState, never reversed
4. **ToggleSystem is global** - Renderer-side toggles (grid, camera) in `ToggleSystem.h`

---

## Simulation Pipeline Overview

Each frame follows this sequence:

```
Tick() [Engine/App.cpp]
  │
  ├─→ Sample input (keyboard state)
  │
  ├─→ Accumulator += frameDt
  │
  └─→ while (accumulator >= FIXED_DT)
        │
        ├─→ TickFixed(input, FIXED_DT) [Engine/WorldState.cpp]
        │     │
        │     ├─→ Apply movement input (camera-relative)
        │     ├─→ Apply gravity to velocity
        │     ├─→ Resolve collisions (mode-dependent)
        │     ├─→ Update onGround status
        │     └─→ Check KillZ respawn
        │
        └─→ accumulator -= FIXED_DT

  ├─→ TickFrame(frameDt) - smoothing (camera, FOV, sprint)
  │
  └─→ Render() - uses BuildSnapshot() for HUD
```

> **Source-of-Truth**: `Engine/App.h::FIXED_DT` = 1/60 second.

---

## Collision Modes Overview

The engine supports two collision modes, toggled with F6:

### AABB Mode (Default)

- **Resolution**: Discrete push-out using MTV (Minimum Translation Vector)
- **XZ handling**: `ResolveXZ_MTV()` - find deepest penetration, push out
- **Y handling**: `ResolveAxis()` on Y, then `QuerySupport()` for ground
- **Step-up**: Not supported

### Capsule Mode

- **Resolution**: Continuous sweep + slide
- **XZ handling**: `SweepXZ_Capsule()` - sweep test, slide on collision
- **Y handling**: `SweepY_Capsule()` - vertical sweep for gravity/jump
- **Step-up**: `TryStepUp_Capsule()` - auto-climb small obstacles
- **Cleanup**: `ResolveOverlaps_Capsule()` - depenetration pass

> **Behavior difference**: Capsule mode feels smoother on stairs, AABB mode is simpler to debug.

---

## Render Coupling Points

WorldState provides data to the renderer at these points:

| Data | Method | Used By |
|------|--------|---------|
| HUD diagnostics | `BuildSnapshot()` | `ImGuiLayer::BuildHUDContent()` |
| Pawn position | `GetPawnPosX/Y/Z()` | Character transform in `RenderScene` |
| Pawn yaw | `GetPawnYaw()` | Character rotation |
| View/Proj matrix | `BuildViewProj()` | Camera setup in `Dx12Context` |
| Collision stats | `GetCollisionStats()` | HUD collision display |
| Controller mode | `GetControllerMode()` | HUD mode indicator |

---

## Code Reading Order

Follow this order to understand Day3+ systems:

| Order | File | Symbol | Purpose |
|-------|------|--------|---------|
| 1 | Engine/App.cpp | `App::Tick()` | Frame entry, fixed-step loop |
| 2 | Engine/App.cpp | `App::TickFixed()` (calls WorldState) | Per-tick simulation dispatch |
| 3 | Engine/WorldState.h | `struct PawnState` | Position/velocity/flags definition |
| 4 | Engine/WorldState.h | `enum ControllerMode` | AABB vs Capsule enum |
| 5 | Engine/WorldState.h | `struct CollisionStats` | Per-tick metrics structure |
| 6 | Engine/WorldState.cpp | `WorldState::TickFixed()` | Simulation entry point |
| 7 | Engine/WorldState.cpp | `WorldState::ResolveXZ_MTV()` | AABB mode XZ resolution |
| 8 | Engine/WorldState.cpp | `WorldState::SweepXZ_Capsule()` | Capsule mode XZ sweep |
| 9 | Engine/WorldState.cpp | `WorldState::QuerySupport()` | Ground detection for both modes |
| 10 | Renderer/DX12/ImGuiLayer.cpp | `ImGuiLayer::BuildHUDContent()` | Diagnostic display |

---

## Configuration Constants

Key tuning values defined in `Engine/WorldState.h::struct WorldConfig`:

| Constant | Value | Defined At |
|----------|-------|------------|
| FIXED_DT | 1/60 (0.01667s) | `Engine/App.h::FIXED_DT` |
| Gravity | 30 | `Engine/WorldState.h::WorldConfig::gravity` |
| Jump velocity | 15 | `Engine/WorldState.h::WorldConfig::jumpVelocity` |
| Walk speed | 30 | `Engine/WorldState.h::WorldConfig::walkSpeed` |
| Capsule radius | 1.4 | `Engine/WorldState.h::WorldConfig::capsuleRadius` |
| Capsule half-height | 1.1 | `Engine/WorldState.h::WorldConfig::capsuleHalfHeight` |
| Max step height | 0.3 | `Engine/WorldState.h::WorldConfig::maxStepHeight` |
| Grid cells | 100x100 | `Engine/WorldState.h::GRID_SIZE` (private) |

### Derived Values

- **Total capsule height**: 2 * radius + 2 * halfHeight = 2 * 1.4 + 2 * 1.1 = 5.0
- **Max jump height**: v^2 / (2g) = 225 / 60 = 3.75 units

---

## Where to Observe

Map of observation points for debugging:

### Toggle → What Changes

| Toggle | Affects | Observe In |
|--------|---------|------------|
| F6 (Controller) | Collision algorithm | HUD "Ctrl" field, `[MODE]` logs |
| F7 (GridTest) | World layout | HUD "GridTest" field, visual stairs |
| F8 (Verbose) | HUD detail level | Additional HUD sections appear |
| G (Grid) | Cube visibility | Cubes show/hide |
| V (Camera) | Camera follow | Third-person ↔ free cam |

### HUD Section → Data Source

| HUD Section | Source Symbol |
|-------------|---------------|
| Position | `PawnState::posX/Y/Z` |
| Velocity/Speed | Computed from `PawnState::velX/Y/Z` |
| OnGround | `PawnState::onGround` |
| Candidates | `CollisionStats::candidatesChecked` |
| Contacts | `CollisionStats::contacts` |
| Depen iters | `CollisionStats::depenIterations` |
| Sweep TOI | `CollisionStats::sweepTOI` |
| StepUp result | `CollisionStats::stepSuccess` |

### Log Prefix → Code Location

| Prefix | Source Function |
|--------|-----------------|
| `[Collision]` | `BuildSpatialGrid()` |
| `[MODE]` | `ToggleControllerMode()` |
| `[SWEEP]` | `SweepXZ_Capsule()` |
| `[DEPEN]` | `ResolveOverlaps_Capsule()` |
| `[STEP_UP]` | `TryStepUp_Capsule()` |
| `[KILLZ]` | `TickFixed()` (respawn path) |

---

## File Quick Reference

| Component | File | Key Symbols |
|-----------|------|-------------|
| Frame loop | `Engine/App.cpp` | `Tick()`, `TickFixed()` |
| World simulation | `Engine/WorldState.cpp` | `TickFixed()`, `ResolveXZ_MTV()`, `SweepXZ_Capsule()` |
| State definitions | `Engine/WorldState.h` | `PawnState`, `WorldConfig`, `CollisionStats` |
| Input handling | `DX12EngineLab.cpp` | `WndProc()` WM_KEYDOWN |
| Toggles | `Renderer/DX12/ToggleSystem.h` | `ToggleGrid()`, `ToggleCameraMode()` |
| HUD display | `Renderer/DX12/ImGuiLayer.cpp` | `BuildHUDContent()` |

---

## Next Steps

After understanding this map:
1. Run verification steps in [01_day3_quickstart.md](01_day3_quickstart.md)
2. Document findings using [99_proof_template.md](../99_proof_template.md)
3. For algorithm deep-dives, await PR#DOCS-3 study pack
