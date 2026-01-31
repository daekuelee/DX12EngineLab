# Day3+ Quickstart

Run the simulation baseline, explore toggles/HUD/logs, and capture first evidence.

---

## Prerequisites

- Completed base quickstart ([00-quickstart.md](../00-quickstart.md)) - build + basic toggles working
- Familiarity with fixed-timestep game loops
- Visual Studio 2022 with x64/Debug configuration

---

## Run the Simulation

### Default State

Launch the engine. Default behavior:
- **Third-person camera** follows pawn from behind
- **AABB collision mode** (discrete push-out)
- Pawn spawns above floor, falls to ground
- 100x100 cube grid with spatial hash collision

### Movement Controls

| Key | Action | Notes |
|-----|--------|-------|
| W/S | Move forward/backward | Camera-relative direction |
| A/D | Strafe left/right | Camera-relative direction |
| Space | Jump | Single press, requires ground contact |
| Shift | Sprint | Hold for 2x speed, FOV widens |
| Mouse | Look around | Yaw/pitch control |

> **Source-of-Truth**: Movement input is sampled in `Engine/App.cpp::Tick()` and applied in `Engine/WorldState.cpp::TickFixed()`.

---

## Physics Toggles

Press these keys to change collision/rendering behavior:

| Key | Effect | Defined At |
|-----|--------|------------|
| F6 | Toggle AABB ↔ Capsule controller | `DX12EngineLab.cpp` WM_KEYDOWN handler, calls `App::ToggleControllerMode()` |
| F7 | Toggle GridTest (step-up stairs map) | `DX12EngineLab.cpp` WM_KEYDOWN handler, calls `App::ToggleStepUpGridTest()` |
| F8 | Toggle HUD verbose mode | `DX12EngineLab.cpp` WM_KEYDOWN handler, calls `ToggleSystem::ToggleHudVerbose()` |
| G | Toggle grid (cubes) visibility | `DX12EngineLab.cpp` WM_KEYDOWN handler, calls `ToggleSystem::ToggleGrid()` |
| V | Toggle camera mode (Third ↔ Free) | `DX12EngineLab.cpp` WM_KEYDOWN handler, calls `ToggleSystem::ToggleCameraMode()` |

### Mode Differences

| Mode | XZ Resolution | Y Resolution | Step-Up |
|------|---------------|--------------|---------|
| AABB | MTV discrete push-out | Discrete snap | No |
| Capsule | Sweep + slide | Sweep | Yes (F7 map) |

> **Source-of-Truth**: Mode is stored in `Engine/WorldState.h::ControllerMode` enum.

---

## HUD Key Sections

The ImGui HUD displays real-time diagnostics. Key sections:

| Section | Shows | Rendered By |
|---------|-------|-------------|
| Toggles | Current mode states (Ctrl, GridTest, Verbose) | `Renderer/DX12/ImGuiLayer.cpp::BuildHUDContent()` |
| Position | Pos X/Y/Z, Speed, OnGround | `Renderer/DX12/ImGuiLayer.cpp::BuildHUDContent()` |
| Collision | Candidates, Contacts, Last hit cube | `Renderer/DX12/ImGuiLayer.cpp::BuildHUDContent()` |
| Depen (Capsule) | Iterations, magnitude (verbose only) | `Renderer/DX12/ImGuiLayer.cpp::BuildHUDContent()` |
| Sweep (Capsule) | Hit cube, TOI, normal (verbose only) | `Renderer/DX12/ImGuiLayer.cpp::BuildHUDContent()` |
| StepUp (Capsule) | OK/FAIL, height (verbose only) | `Renderer/DX12/ImGuiLayer.cpp::BuildHUDContent()` |

### Enabling Verbose Mode

Press **F8** to toggle verbose mode. When enabled:
- Additional Capsule-mode sections appear (Depen, Sweep, StepUp)
- Per-frame diagnostic values are shown
- Useful for debugging collision edge cases

---

## Debug Log Prefixes

The engine outputs tagged log messages to the VS Output window. Key prefixes:

| Prefix | Indicates | Emitted By |
|--------|-----------|------------|
| `[Collision]` | Spatial hash built, collision detection | `Engine/WorldState.cpp::BuildSpatialGrid()` |
| `[MODE]` | Controller mode change (AABB ↔ Capsule) | `Engine/WorldState.cpp::ToggleControllerMode()` |
| `[SWEEP]` | XZ sweep test result (Capsule mode) | `Engine/WorldState.cpp::SweepXZ_Capsule()` |
| `[DEPEN]` | Depenetration iteration (Capsule mode) | `Engine/WorldState.cpp::ResolveOverlaps_Capsule()` |
| `[STEP_UP]` | Step-up attempt result | `Engine/WorldState.cpp::TryStepUp_Capsule()` |
| `[KILLZ]` | Pawn killed (fell below world) | `Engine/WorldState.cpp::TickFixed()` |

### Filtering Logs

In VS Output window, use Find (Ctrl+F) with prefixes to filter relevant messages:
- `[MODE]` - Track mode toggles
- `[STEP_UP]` - Debug stair climbing
- `[KILLZ]` - Track respawns

---

## Verification Checklist

Run through these steps to verify Day3+ features work:

1. [ ] **Launch Debug build** - Window opens, pawn visible in third-person view
2. [ ] **Walk around** - WASD moves pawn, camera follows
3. [ ] **Jump** - Space causes pawn to jump, lands back on ground
4. [ ] **Sprint** - Hold Shift, observe FOV widen and speed increase
5. [ ] **Press F6** - HUD "Ctrl" toggles between AABB/Capsule
6. [ ] **Press F7** - HUD "GridTest" shows ON, cube layout changes to stairs
7. [ ] **Press F8** - HUD shows additional verbose sections (Depen, Sweep, StepUp)
8. [ ] **Check VS Output** - Look for `[MODE]`, `[Collision]` log prefixes

---

## Evidence Capture

### Screenshot Workflow

1. Run desired test scenario
2. Ensure HUD is visible with relevant values
3. Press PrintScreen or use Windows Snipping Tool
4. Save as `dayX_feature_state.png` in `captures/` directory
5. Annotate key HUD values if needed

### Log Capture Workflow

1. Open VS Output window before test
2. Clear output (right-click → Clear All)
3. Run test scenario
4. Select relevant log lines
5. Copy to evidence document with interpretation

### Evidence Template

Use [99_proof_template.md](../99_proof_template.md) for structured evidence bundles.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Pawn clips through cubes | Wrong controller mode | Press F6 to toggle mode |
| No verbose HUD sections | Verbose mode off | Press F8 to enable |
| No movement | ImGui capturing input | Click on game window |
| Cubes not visible | Grid toggled off | Press G to enable |
| Camera not following | Free camera mode | Press V to toggle |

---

## Next Steps

After completing this quickstart:
1. Read [02_day3_system_map.md](02_day3_system_map.md) for ownership/dataflow
2. Use [99_proof_template.md](../99_proof_template.md) to document findings
3. Explore capsule mode with F7 grid test for step-up behavior
