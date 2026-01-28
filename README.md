# DX12EngineLab

Engine-style DX12 sandbox built with **contracts + toggles + evidence** (not tutorial copy-paste).

## Day0.5 Contract
- Device + SwapChain + RTV heap
- Clear + Present works every frame
- Triple-buffer FrameContext (3 allocators + fence tracking)
- Debug layer ON (happy path has 0 errors)

## Day 1: Instanced vs Naive (10k Cubes)

**Goal**: Render 10,000 cubes with runtime toggle between instanced (1 draw) and naive (10k draws) modes.

**Outcome**:
- Both modes produce identical visual output
- 100x100 cube grid with proper 3D perspective and face lighting
- Triple-buffered frame contexts with fence-gated resource reuse

**Key Commits**:
- `df195e4` - Initial 10k instancing scaffold
- `5cac813` - Fix: `row_major` annotation for matrix layout match
- `4ed54f0` - Fix: Per-frame barrier state tracking
- `d434845` - Fix: Cube index buffer winding for all 6 faces

**Key Learning**:
- CPU row-major matrices require `row_major` HLSL annotation
- Per-frame resource state tracking prevents triple-buffer barrier mismatches

**Debug Narrative**: [docs/notes/day1/dailyNote.md](docs/notes/day1/dailyNote.md)

**Evidence**: [docs/contracts/day1/](docs/contracts/day1/)

**Next**: Day1.5 - Upload arena, resource state tracker

## Day1.5: Refactor & Infrastructure (2026-01-25)

**Goal**: Add frame-level allocators, descriptor management, and debug HUD.

**Key Changes**:
- `b646118` - FrameLinearAllocator (per-frame bump allocation, 1MB/frame)
- `d60981e` - DescriptorRingAllocator (1024-slot fence-protected ring)
- `d60981e` - PSOCache (hash-based lazy PSO creation)
- `e045a21` - ImGui HUD overlay (FPS, mode display, controls)
- `faf4fab` - Fix ImGui crash (pass CommandQueue to v1.91+ backend)
- `8943684` - DescriptorRingAllocator safety guards

**Debug Narrative**: [docs/notes/day1.5/dailyNote.md](docs/notes/day1.5/dailyNote.md)

**Evidence**: [docs/contracts/day1.5/](docs/contracts/day1.5/)

**Next**: Day1.7 - Composition-based refactor

## Day1.7: Composition-Based Architecture Refactor (2026-01-26)

**Goal**: Transform monolithic architecture into composition-based design with clear ownership, testable components, and explicit contracts. No runtime behavior changes.

**New Components**:
- `RenderContext` - Parameter bundle for render passes
- `ClearPass` / `GeometryPass` / `ImGuiPass` - Composable render passes
- `BarrierScope` - RAII barrier management
- `DiagnosticLogger` - Centralized throttled logging
- `ResourceStateTracker` - SOLE authority for resource state tracking
- `GeometryFactory` - Consolidated buffer creation with upload sync
- `PassOrchestrator` - Sequences render passes

**Key Changes**:
- Split `Initialize()` into focused helpers (Device, SwapChain, Subsystems)
- Migrate transforms to `ResourceRegistry` (handle-based ownership)
- Remove parallel state tracking (`FrameContext::transformsState` deleted)
- Extract `FreeCamera` to `Dx12Context` member

**Key Commit**: `8f32cb3` (19 files, +1382/-689 lines)

**Debug Narrative**: [docs/notes/day1.7/dailyNote.md](docs/notes/day1.7/dailyNote.md)

**Spec/Contract**: [docs/contracts/day1.7/finalRefact.md](docs/contracts/day1.7/finalRefact.md)

**Next**: Day2 - Upload arena, GPU culling

## Day2: UploadArena + Diagnostic Instrumentation (2026-01-26)

**Goal**: Create unified allocation front-door with diagnostic HUD instrumentation.

**New Components**:
- `UploadArena` - Wraps FrameLinearAllocator as single entry point for dynamic uploads
- Per-frame metrics: allocCalls, allocBytes, peakOffset, capacity

**Key Changes**:
- All upload allocations (CB, Transforms) now go through UploadArena
- HUD displays Upload Arena section when U toggle is enabled
- Runtime behavior unchanged (pure wrapper + instrumentation)

**Toggle**:
- Press `U` to toggle Upload Diagnostics ON/OFF
- When ON: HUD shows allocation metrics

**Expected Metrics** (per frame):
- allocCalls: 2 (FrameCB + Transforms)
- allocBytes: ~640KB
- peakOffset/capacity: ~640KB / 1MB (64%)

**Debug Narrative**: [docs/notes/day2/dailyNote.md](docs/notes/day2/dailyNote.md)

**Contract**: [docs/contracts/Day2.md](docs/contracts/Day2.md)

**Next**: Day2.5 - Failure mode proof (upload=bad toggle)

## Day3: Game Simulation Layer + Collision (2026-01-27)

**Goal**: ECS-lite game simulation with fixed-step physics, third-person camera, and AABB collision system.

**New Components**:
- `WorldState` - Pawn physics, camera state, collision config
- `InputSampler` - Header-only input sampling
- `CharacterRenderer` - Player cube rendering
- Spatial hash broadphase (100x100 grid)
- Axis-separated AABB collision (X->Z->Y)

**Key Features**:
- Fixed-step physics at 60Hz with accumulator cap
- Camera-relative WASD movement with sprint (Shift)
- Jump with hitch-safe consumption flag (Space)
- Third-person camera with smooth follow and FOV change (45->55 sprint)
- Floor collision with epsilon tolerance and KillZ respawn
- Cube collision with spatial hash broadphase

**Key Commits**:
- `4a8686b` - ECS-lite foundation with fixed-step physics
- `1c1dd19` - Collision system with spatial hash
- `1b8b496` - Fix: Penetration sign for correct push direction
- `6131c19` - Fix: onGround toggle bug with epsilon tolerance
- `71a8dca` - Fix: Floor bounds to match rendered geometry

**Toggles/Controls**:
- `V` - Toggle camera mode (ThirdPerson/Free)
- `WASD` - Move (camera-relative)
- `Shift` - Sprint
- `Space` - Jump
- `Q/E` - Yaw rotation
- `R/F` - Pitch rotation
- Mouse - Look (in ThirdPerson mode)

**Debug Narrative**: [docs/notes/day3/daily_note.md](docs/notes/day3/daily_note.md)

**Plan Docs**: [docs/contracts/day3/](docs/contracts/day3/)

---

## Day 3.4-3.10: Collision System Refinement (2026-01-28)

**Goal**: Iterative debugging and fixing of the AABB collision system through proof-first methodology.

### Day 3.4: Iterative Solver & Bounds Fix
- `cubeHalfXZ`: 0.45 -> 0.9 (match rendered geometry)
- Iterative solver with 8 max iterations and epsilon convergence
- HUD diagnostics: iterationsUsed, contacts, maxPenetrationAbs

### Day 3.5: Support Query System
- `QuerySupport()` - Pure function returning SupportResult (FLOOR/CUBE/NONE)
- Single mutation point in TickFixed for snap/velY/onGround
- Strict Intersects: touching is NOT intersection

### Day 3.6: Multi-Issue Fix
- Cube shading: Normal priority Y > X > Z
- Character embedding: Leg offsetY fix
- Floor fall-through: Penetration recovery block

### Day 3.7: Face Culling & Movement
- Fix +Z/-Y face index winding (all 6 cube faces visible)
- Camera-relative A/D movement: `cross(fwd, up)` for right vector
- Axis-aware collision extents: X=1.4, Z=0.4

### Day 3.8: MTV-Based Collision
- MTV (Minimum Translation Vector) XZ resolution
- Resolve along axis with smaller penetration
- HUD: penX/penZ, mtvAxis, centerDiff

### Day 3.9: Wall-Climb Regression Fix
- Separable-axis XZ push-out (apply BOTH penX and penZ)
- Anti-step-up guard: Only allow upward Y if truly landing from above
- HUD: xzStillOverlapping, yStepUpSkipped

### Day 3.10: Penetration Sign Fix
- **Root Cause**: `ComputeSignedPenetration` returned inverted sign
- **Fix**: Single-line sign inversion corrects all axes
- All 6 cube faces now block symmetrically

**Key Commits**:
- `8883d1b` - Day3.4: Iterative solver and cube bounds
- `3dbd0f6` - Day3.5: QuerySupport system
- `5f60a6e` - Day3.7: Fix face winding
- `78a69b1` - Day3.8: Fix A/D movement direction
- `5846944` - Day3.8: MTV-based collision
- `1ae3fcc` - Day3.9: Wall-climb regression fix
- `b69e7ed` - Day3.10: Penetration sign fix

**Debug Narrative**: [docs/notes/day3/daily_note_half.md](docs/notes/day3/daily_note_half.md)

**Contracts**: [docs/contracts/day3/](docs/contracts/day3/)

---

## Day 3.11: Capsule Controller (2026-01-28)

**Goal**: Implement alternative controller mode using vertical capsule + sweep/slide collision.

**New Features**:
- Runtime toggle (F6) between AABB and Capsule modes
- Vertical capsule geometry with feet-bottom anchor
- Swept collision detection (Minkowski sum + slab method)
- Wall sliding via velocity clipping
- Depenetration safety net for spawn/teleport

**Key Commits**:
- `1665692` - Phase 0: Mode toggle + unified reset
- `fdf832d` - Phase 1: Capsule SSOT + HUD proof
- `a100801` - Phase 2: Depenetration safety net
- `d765038` - Phase 3: XZ sweep/slide MVP
- `cbb0794` - Fix: Radius match + escape logic

**Controls**:
- `F6` - Toggle controller mode (AABB/Capsule)

**Debug Narrative**: [docs/contracts/day3/daily_note_capsule.md](docs/contracts/day3/daily_note_capsule.md)

**SSOT Reference**: [docs/notes/sweep_capsule.md](docs/notes/sweep_capsule.md)

**Next**: Day4 - View FX (fog, sky gradient, exposure)

## Build & Run
- Open DX12EngineLab.sln in VS2022
- Select **x64 / Debug**
- Run (F5)

## Evidence
- Console/Debug log: frameIndex + fence values per frame
- captures/: screenshots (debug layer clean, etc.)

## Toggles (planned)
- draw: naive | instanced
- upload: bad | arena
- cull: off | cpu | gpu
- tex: off | array
