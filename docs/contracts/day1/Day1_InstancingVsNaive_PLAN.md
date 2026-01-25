# Day1 Implementation Plan: Instanced vs Naive (10k)

## 0) Integration Option Chosen: B (Composition)

**Rationale**: Create focused modules in `Renderer/DX12/` without touching `.sln`. Balances extensibility with minimal risk.

**vcxproj changes required NOW (S0)**:
- Add `d3dcompiler.lib` to Debug|x64 AdditionalDependencies
- Add `d3d12.lib;dxgi.lib;dxguid.lib;d3dcompiler.lib` to Release|x64 AdditionalDependencies

---

## 1) Scope

**In Scope**:
- 10k cube instances in 100x100 grid
- Runtime toggle: Instanced (1 draw) vs Naive (10k draws)
- Per-frame resources: allocator, upload buffer, transforms buffer, SRV slot
- Fence-gated reuse (no descriptor/resource stomp)
- CPU timing log: `mode, draw_calls, cpu_record_ms`
- Proof toggles: `stomp_Lifetime`, `break_RPIndexSwap`, `sentinel_Instance0`

**Out of Scope**:
- GPU timestamps (seam only)
- Depth buffer
- Texture arrays
- Resize handling
- Multiple pipelines

**Design-Ready Hooks**:
- `UploadArena` interface for future suballocation
- `ResourceStateTracker` seam (explicit barriers for now)
- Query heap allocation for GPU timestamps

---

## 2) Architecture

### Discrepancy Resolutions
| ID | Decision |
|----|----------|
| D1 | Use `FrameCount = 3` (triple buffering) |
| D2 | Use `DATA_STATIC_WHILE_SET_AT_EXECUTE` per contract |
| D3 | `RenderScene` owns `m_firstFrame` flag — single owner for initial state logic |
| D4 | IA/RS/OM set in Tick, not RecordDraw |

### Module Ownership

| Module | Location | Owns |
|--------|----------|------|
| **FrameContextRing** | `Renderer/DX12/FrameContextRing.h` | Per-frame allocators, fence values, upload/SRV slots |
| **ShaderLibrary** | `Renderer/DX12/ShaderLibrary.h` | Root sig, PSO, embedded HLSL compilation |
| **RenderScene** | `Renderer/DX12/RenderScene.h` | Cube VB/IB, transforms buffer, draw recording |
| **ToggleSystem** | `Renderer/DX12/ToggleSystem.h` | Mode flags, diagnostic levers |

### Key Policies

**FrameId vs BackBufferIndex (structurally enforced)**:
- `m_frameId` is a **monotonic uint64** incremented each frame (`m_frameId++`)
- Frame resources index: `m_frameId % FrameCount` (for allocators, upload buffers, SRV slots)
- Backbuffer index: `m_swapChain->GetCurrentBackBufferIndex()` (for RTV selection only)
- **API shape that prevents misuse**:
  ```
  FrameContext& BeginFrame();        // uses m_frameId++ internally, returns per-frame resources
  uint32_t GetBackBufferIndex();     // separate accessor for RTV only
  ```
- These are **never conflated** — BeginFrame owns frame resources, backbuffer index is RTV-only.

**Descriptors**: Per-frame SRV slot (slot = frameResourceIndex, NOT backbuffer index)

**Barriers + Initial State (single owner)**:
- `RenderScene` owns a `bool m_firstFrame` flag for transforms buffer
- First frame: buffer starts in COPY_DEST, skip SRV→COPY_DEST barrier, set `m_firstFrame = false`
- Subsequent frames: full cycle SRV → COPY_DEST → Copy → COPY_DEST → SRV
- **One place, one responsibility** — no scattered first-frame exceptions

**Root ABI**: `RP_FrameCB=0` (b0), `RP_TransformsTable=1` (t0 table)

---

## 3) Slice Plan

### S0: Fix Build + Runtime Sanity
**Objective**: Both configs link AND app launches successfully
**Files**: `DX12EngineLab.vcxproj`
**Changes**:
- Line ~109: Add `d3dcompiler.lib` to Debug|x64
- Line ~122-125: Add `d3d12.lib;dxgi.lib;dxguid.lib;d3dcompiler.lib` to Release|x64
**Acceptance**:
- `msbuild /p:Configuration=Release /p:Platform=x64` succeeds
- `msbuild /p:Configuration=Debug /p:Platform=x64` succeeds
- **Runtime sanity**: App launches, window opens, clears to a color (catches missing DLL/runtime issues early)
**Build-green**: Debug|x64 + Release|x64 both compile, link, and run

### S1: FrameContextRing + Triple Buffering
**Objective**: Per-frame allocators with fence-gated reuse
**Files**:
- `Renderer/DX12/FrameContextRing.h` (new)
- `Renderer/DX12/Dx12Context.h` (update FrameCount=3, add FrameContextRing member)
- `Renderer/DX12/Dx12Context.cpp` (init ring, use in Render)
**Acceptance**: Window opens, clears to color, no validation errors, log shows `frameId=0,1,2` cycling
**Build-green**: Both configs compile and link

### S2: ShaderLibrary + Root Sig + PSO
**Objective**: Embedded HLSL compiles, PSO created
**Files**:
- `Renderer/DX12/ShaderLibrary.h` (new)
- `Renderer/DX12/ShaderLibrary.cpp` (new)
- `Renderer/DX12/Dx12Context.cpp` (init ShaderLibrary)
**Acceptance**: No D3DCompile errors, PSO valid, log shows "PSO created"
**Build-green**: Both configs compile and link

### S3: Cube Geometry (VB/IB)
**Objective**: Static cube mesh in DEFAULT heap
**Files**:
- `Renderer/DX12/RenderScene.h` (new)
- `Renderer/DX12/RenderScene.cpp` (new)
- `Renderer/DX12/Dx12Context.cpp` (init geometry)
**Acceptance**: Single white cube renders at origin
**Build-green**: Both configs compile and link

### S4: Transforms Buffer + SRV + 10k Grid
**Objective**: Per-frame upload→default copy, SRV binding, 100x100 grid visible
**Files**:
- `Renderer/DX12/RenderScene.cpp` (add transforms, per-frame buffers, SRV creation)
- `Renderer/DX12/FrameContextRing.h` (add upload buffer, SRV slot tracking)
**Acceptance**: 10k cubes visible in grid pattern, instance 0 sentinel works
**Build-green**: Both configs compile and link

### S5: ToggleSystem + Naive vs Instanced
**Objective**: Runtime toggle between 1 draw and 10k draws
**Files**:
- `Renderer/DX12/ToggleSystem.h` (new)
- `Renderer/DX12/RenderScene.cpp` (conditional draw loop)
- `DX12EngineLab.cpp` (key input to toggle)
**Acceptance**: Press key → mode switches, log shows `mode=instanced draws=1` vs `mode=naive draws=10000`
**Build-green**: Both configs compile and link

### S6: Timing + Evidence Log
**Objective**: CPU record timing, structured log output
**Files**:
- `Renderer/DX12/Dx12Context.cpp` (timing around record, log output)
**Acceptance**: Console shows `mode=instanced draws=1 cpu_record_ms=X.XXX` each frame
**Build-green**: Both configs compile and link

### S7: Proof Toggles (Microtests)
**Objective**: Diagnostic levers to prove correctness
**Files**:
- `Renderer/DX12/ToggleSystem.h` (add stomp_Lifetime, break_RPIndexSwap, sentinel flags)
- `Renderer/DX12/RenderScene.cpp` (conditional broken paths)
**Acceptance**: Each toggle produces expected failure (flicker, wrong binding, etc.)
**Build-green**: Both configs compile and link

---

## 4) Evidence Plan

### Log Schema
```
mode=<instanced|naive> draws=<1|10000> cpu_record_ms=<float> frameId=<0-2>
```

### Screenshot Naming
- `captures/day1_instanced_10k.png`
- `captures/day1_naive_10k.png`
- `captures/day1_sentinel_proof.png`

### Debug Validation
- DX12 debug layer ON in Debug builds
- Zero validation errors in happy path
- `GetDeviceRemovedReason()` logged on failure

---

## 5) Contract Patch Suggestions

```diff
--- docs/contracts/Day1_InstancingVsNaive.md
+++ docs/contracts/Day1_InstancingVsNaive.md
@@ -106,7 +106,9 @@ Named pipeline lookup
 **Critical correction**
 - Descriptor range flags: do **NOT** claim `DATA_STATIC` if underlying data changes across frames.
   - Use **`DATA_STATIC_WHILE_SET_AT_EXECUTE`** (or equivalent safe default) because per-frame data is stable only during GPU execution of that command list.
+- Initial resource state: transforms DEFAULT buffer should be created in `COPY_DEST` state.
+  - First frame: skip SRV→COPY_DEST barrier (buffer already in COPY_DEST).
+  - Subsequent frames: full barrier cycle SRV→COPY_DEST→copy→COPY_DEST→SRV.
```

---

## 6) Risk Matrix

| Risk | Proof Toggle | Expected Symptom |
|------|--------------|------------------|
| Frame-ring ≠ swapchain | `stomp_Lifetime=true` | Flicker/garbage |
| Descriptor stomp | `break_OmitSetHeaps=true` | GPU reads garbage |
| Root param mismatch | `break_RPIndexSwap=true` | Wrong data bound |
| Instance ID wrong | `sentinel_Instance0=true` | Instance 0 not at (9999,0,9999) |

---

## Verification

1. After each slice: `msbuild DX12EngineLab.sln /p:Configuration=Debug /p:Platform=x64`
2. After each slice: `msbuild DX12EngineLab.sln /p:Configuration=Release /p:Platform=x64`
3. Run exe, verify acceptance criteria
4. Check debug output for validation errors
5. Final: screenshot captures for evidence


● All Day1 slices (S0-S7) are complete. Both Debug|x64 and Release|x64 build successfully.

  Summary of what was implemented:

  S0: Fix Build + Runtime Sanity                                                                                                                                           - Added missing libs (d3dcompiler.lib) to Debug|x64
  - Added full lib set to Release|x64                                                                                                                                      - Enabled C++17 (stdcpp17) for inline static variables

  S1: FrameContextRing + Triple Buffering
  - FrameContextRing.h/cpp: Per-frame allocators with fence-gated reuse
  - FrameCount = 3 (triple buffering)
  - Monotonic frameId for frame resource selection (not backbuffer index)

  S2: ShaderLibrary + Root Sig + PSO
  - ShaderLibrary.h/cpp: Embedded HLSL, root signature, PSO
  - Root ABI: RP_FrameCB=0 (b0), RP_TransformsTable=1 (t0)

  S3: Cube Geometry (VB/IB)
  - RenderScene.h/cpp: 8-vertex cube with 36 indices in DEFAULT heap
  - One-shot upload helper with fence synchronization

  S4: Transforms Buffer + SRV + 10k Grid
  - Per-frame CBV (ViewProj), transforms upload/default buffers
  - Per-frame SRV slots to avoid descriptor stomping
  - 100x100 grid positioning (10k instances)

  S5: ToggleSystem + Naive vs Instanced
  - ToggleSystem.h: Runtime mode toggle (instanced vs naive)
  - Press T to switch between 1 draw (instanced) and 10k draws (naive)

  S6: Timing + Evidence Log
  - CPU timing with QueryPerformanceCounter
  - Log format: mode=instanced draws=1 cpu_record_ms=X.XXX frameId=0

  S7: Proof Toggles
  - 1 key: sentinel_Instance0 - moves instance 0 to distinct position
  - 2 key: stomp_Lifetime - uses wrong frame SRV (demonstrates stomp)
  - 3 key: break_RPIndexSwap - swaps root parameter indices (demonstrates wrong binding)
