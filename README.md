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
