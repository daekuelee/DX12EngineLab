# Quickstart Guide

Get the engine running and explore its capabilities.

---

## Build

### Visual Studio (Recommended)

1. Open `DX12EngineLab.sln` in Visual Studio 2022
2. Select configuration: **x64 / Debug**
3. Build: `Ctrl+Shift+B`
4. Run: `F5`

### Command Line

```bash
msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64
```

> **Source-of-Truth**: Build commands documented in `CLAUDE.md` at repo root.

---

## Run

The application opens a window showing:
- 10,000 cubes arranged in a 100×100 grid
- A floor plane (when G toggle is on)
- Free camera controlled by keyboard

### Camera Controls

| Key | Action |
|-----|--------|
| W/S or ↑/↓ | Move forward/backward |
| A/D or ←/→ | Strafe left/right |
| Space | Move up |
| Ctrl | Move down |
| Q/E | Rotate camera left/right |

> **Source-of-Truth**: `Dx12Context::UpdateCamera()` in `Renderer/DX12/Dx12Context.cpp`

---

## Runtime Toggles

Press these keys while running to explore different modes:

| Key | Effect | What to Observe |
|-----|--------|-----------------|
| **T** | Toggle draw mode | Instanced (1 draw) ↔ Naive (10k draws). No visual difference, CPU timing changes. |
| **C** | Cycle color mode | Face colors → Instance colors → Lambert shading |
| **G** | Toggle grid | Show/hide cubes. Floor always visible. |
| **U** | Upload diagnostics | Show Upload Arena HUD with allocation metrics |
| **M** | Toggle markers | Show corner marker triangles |

### Diagnostic Toggles (For Testing)

| Key | Effect | Use Case |
|-----|--------|----------|
| **F1** | Sentinel instance 0 | Moves instance 0 to (150, 50, 150) for visibility testing |
| **F2** | Stomp lifetime test | Uses wrong frame SRV - causes visible flicker/corruption |

> **Source-of-Truth**: `ToggleSystem` class in `Renderer/DX12/ToggleSystem.h`

---

## Verify Your Build

Run through this checklist to confirm the build works:

1. **Launch Debug build** - Window opens with cubes visible
2. **Press T twice** - Should toggle mode and return (no visual change)
3. **Press C three times** - Cycles through all color modes
4. **Press U** - HUD appears showing:
   - `allocCalls: 2` (FrameCB + Transforms)
   - `allocBytes: ~655KB`
   - `peakOffset` should match `allocBytes`
5. **Check Output window** - Look for `D3D12 ERROR` (should be none)

---

## Debug Output

The engine logs diagnostic information to Visual Studio's Output window:

| Prefix | Meaning |
|--------|---------|
| `DIAG:` | Periodic diagnostic dump (viewport, scissor, draw calls) |
| `PROOF:` | Frame/binding verification (once per second) |
| `PASS:` | Pass execution info (PSOs, SRV indices) |
| `WARNING:` | Diagnostic mode active (e.g., stomp test) |

---

## Project Structure

```
DX12EngineLab/
├── Renderer/DX12/         # Graphics layer (main focus)
│   ├── Dx12Context.*      # Main orchestrator
│   ├── FrameContextRing.* # Per-frame resource management
│   ├── ShaderLibrary.*    # Root sig + PSO creation
│   └── ...
├── Engine/                # Application layer
│   └── App.*              # Window + message pump
├── shaders/               # HLSL files
├── docs/
│   ├── contracts/         # Phase specifications
│   └── onboarding/        # This documentation
└── DX12EngineLab.sln      # VS2022 solution
```

---

## Further Reading

| Topic | Document | What to Look For |
|-------|----------|------------------|
| Component relationships | [10-architecture-map.md](10-architecture-map.md) | Ownership hierarchy, who creates what |
| Frame timeline | [20-frame-lifecycle.md](20-frame-lifecycle.md) | What happens each frame, synchronization |
| Debug techniques | [70-debug-playbook.md](70-debug-playbook.md) | Common errors and how to diagnose |

---

## Study Path

### Read Now
- This document (you're here)
- [10-architecture-map.md](10-architecture-map.md) - Get the big picture

### Read When Broken
- [70-debug-playbook.md](70-debug-playbook.md) - Validation error troubleshooting
- [20-frame-lifecycle.md](20-frame-lifecycle.md) - Synchronization issues

### Read Later
- [40-binding-abi.md](40-binding-abi.md) - When adding shader bindings
- [80-how-to-extend.md](80-how-to-extend.md) - When adding features

---

## Day3+ Track

Starting from Day3, the engine includes simulation, physics, and collision systems.

### Prerequisites
- Complete the base quickstart above (build + basic toggles working)
- Familiarity with fixed-timestep game loops

### Day3+ Reading Order

| Order | Document | Purpose |
|-------|----------|---------|
| 1 | [pass/01_day3_quickstart.md](pass/01_day3_quickstart.md) | Run baseline, explore toggles/HUD/logs |
| 2 | [pass/02_day3_system_map.md](pass/02_day3_system_map.md) | Ownership + dataflow diagram |
| 3 | [99_proof_template.md](99_proof_template.md) | Evidence bundle template |
| 4 | [pass/03_day3_contracts_map.md](pass/03_day3_contracts_map.md) | Contract reference (8 categories) |
| 5 | [pass/04_day3_failure_cookbook.md](pass/04_day3_failure_cookbook.md) | Failure patterns + proof steps |

> **Source-of-Truth**: `Engine/App.cpp::Tick()` is the Day3 simulation entry point.
