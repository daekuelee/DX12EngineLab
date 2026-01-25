Context
I’m refactoring a DirectX 12 engine codebase (DX12EngineLab). I’m aiming for a maintainable, “big-company” style architecture:
- clear responsibilities, encapsulation
- composition over inheritance
- avoid over-abstracting (thin wrappers only where they pay off)
- focus on correctness + readability first, then performance

My current (tentative) improvement direction (use as guidance, not a hard spec)

1) Pattern A: “Frame-slice linear allocation” (cleanest mental model)
- For transient GPU-facing allocations (especially descriptors): allocate linearly inside a per-frame slice
- Reset/reuse only after the frame fence signals (fence-gated lifetime)
- Goal: make lifetime mechanical and prevent overwrite hazards

2) Principles aligned with Microsoft samples / DX12 docs (guidance)
- Separate resource lifetime/creation from binding (descriptors + descriptor tables in heaps)
- Treat Root Signature as an explicit ABI (stable indices/registers; minimize params)
- Prefer root constants for small/frequently-changing per-draw data; descriptor tables for bulk data
- Make the frame flow explicit in code structure (root sig → root params → descriptors → PSO → draw)
- Separate shaders into .hlsl files, compile to .cso at build time, load binaries at runtime (avoid shader code hardcoded in C++)
- Performance hygiene: big allocation + suballocation patterns, reduce redundant state changes, keep root signature changes rare

What I want from you (Explore / Suggestions)
- Propose a few refactor directions (2–3), with different tradeoffs (minimal / moderate / more structured).
- For each direction, describe:
  - the main modules/components you’d introduce (if any) and why
  - ownership + lifetime rules (especially descriptors, per-frame resources, command allocators)
  - how you’d represent root signature + descriptor table “ABI” in a way that avoids binding bugs
  - how shader separation would be organized (folders, build, load path)
  - expected risks / likely failure modes in DX12 binding (wrong heap, wrong slot, wrong stride, wrong register, stale GPU handle)
  - what quick proof hooks you’d add (logs / debug overlays / microtests) to confirm correctness

Constraints / Preferences
- Composition over inheritance; keep class hierarchy shallow.
- Don’t rewrite the whole engine; keep scope incremental and reversible.
- I’m okay with “pragmatic” choices; prefer patterns commonly used in DX12 engines.
- Every suggested change should mention at least one simple way to validate it (even if it’s just logging).

Optional: if you need repo facts, ask for only the minimum (current root sig layout, descriptor heap usage, frame count, where shaders live).

ROLE / ATTITUDE
Think like a senior game engine engineer collaborating with me in a real production setting:
- be candid about tradeoffs and risk
- call out footguns early
- prefer decisions that keep the codebase understandable for teammates
- suggest a “small next step” that we can land safely

-------------------------------------------
 You've chosen Direction 1 (Shader extraction). Let me finalize the plan with a focused implementation roadmap.

● Updated plan
  ⎿  /plan to preview
       1  # DX12EngineLab Refactor Plan                                                                                                                                           2
       3 +**Selected:** Direction 1 - Shader Extraction                                                                                                                           4 +**Future:** Directions 2 & 3 documented below for reference
       5 +                                                                                                                                                                        6 +---
       7 +
       8  ## Current State Summary
       9
      10  **What's Good (preserve):**

● Updated plan
  ⎿  /plan to preview
       49                                                                                                                                                                         50  ---
       51                                                                                                                                                                         52 +## Implementation Plan (Direction 1)
       53 +                                                                                                                                                                       54 +### Step 1: Create HLSL Files
       55 +
       56 +Extract embedded shaders from `ShaderLibrary.cpp` (lines 14-184):
       57 +
       58 +| File | Source | Contents |
       59 +|------|--------|----------|
       60 +| `shaders/common.hlsli` | New | `TransformData` struct, `FrameCB`, register docs |
       61 +| `shaders/cube.hlsl` | `kVertexShader` + `kPixelShader` | Cube VS/PS with instancing |
       62 +| `shaders/floor.hlsl` | `kFloorVertexShader` + `kFloorPixelShader` | Floor VS/PS (no transform reads) |
       63 +| `shaders/marker.hlsl` | `kMarkerVertexShader` + `kMarkerPixelShader` | Marker pass-through VS/PS |
       64 +
       65 +### Step 2: Add FXC Build Rules
       66 +
       67 +Modify `DX12EngineLab.vcxproj`:
       68 +- Add `<FxCompile>` item group for each `.hlsl` file
       69 +- Output `.cso` to `$(OutDir)shaders/`
       70 +- Target: `vs_5_1` / `ps_5_1`
       71 +- Entry points: `VSMain`, `PSMain`
       72 +
       73 +### Step 3: Modify ShaderLibrary
       74 +
       75 +Replace `D3DCompile()` calls with CSO file loading:
       76 +
       77 +```cpp
       78 +// New method
       79 +bool ShaderLibrary::LoadCompiledShader(const wchar_t* path, ID3DBlob** blob);
       80 +
       81 +// In Initialize():
       82 +LoadCompiledShader(L"shaders/cube_vs.cso", &m_vsBlob);
       83 +LoadCompiledShader(L"shaders/cube_ps.cso", &m_psBlob);
       84 +// ... etc
       85 +```
       86 +
       87 +Remove embedded shader strings (lines 14-184 in ShaderLibrary.cpp).
       88 +
       89 +### Step 4: Validate
       90 +
       91 +1. `msbuild /p:Configuration=Debug /p:Platform=x64` - builds clean
       92 +2. Check `$(OutDir)shaders/*.cso` files exist
       93 +3. Run app - same visual output as before
       94 +4. Introduce HLSL syntax error - build fails (not runtime)
       95 +5. Debug layer: 0 errors
       96 +
       97 +---
       98 +
       99  ### Direction 2: Moderate (+ Linear Allocator + Pass Separation)
      100
      101  **Scope:** Add frame-slice linear allocator for upload heaps, separate render into phases.

● The plan is ready with a focused implementation roadmap for Direction 1 (shader extraction). The key steps are:

  1. Create HLSL files - Extract 6 shaders from embedded C++ strings                                                                                                         2. Add FXC build rules - Compile to .cso at build time
  3. Modify ShaderLibrary - Load CSO files instead of D3DCompile                                                                                                             4. Validate - Build, run, and confirm same visual output

─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                            Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ DX12EngineLab Refactor Plan

 Selected: Direction 1 - Shader Extraction
 Future: Directions 2 & 3 documented below for reference

 ---
 Current State Summary

 What's Good (preserve):
 - RootParam enum provides stable ABI contract
 - Per-frame SRV slots prevent descriptor stomp
 - Fence-gated triple buffering is correctly implemented
 - Resource state tracking per-frame

 Pain Points:
 - Shaders embedded as C++ strings (no syntax highlighting, no build-time validation)
 - Fixed descriptor heap size (no linear allocation for transient resources)
 - Monolithic Render() (~300 lines mixing upload/record/execute concerns)

 ---
 Three Refactor Directions

 Direction 1: Minimal (Shader Extraction Only)

 Scope: Extract shaders to .hlsl files, add build-time FXC compilation.

 Changes:
 shaders/
   cube.hlsl        # VS + PS for cubes
   floor.hlsl       # VS + PS for floor
   marker.hlsl      # VS + PS for markers
   common.hlsli     # Shared structs (TransformData, FrameCB)

 Files to modify:
 - Create: shaders/*.hlsl files
 - Modify: DX12EngineLab.vcxproj (add FXC compile rules → .cso output)
 - Modify: ShaderLibrary.cpp (replace D3DCompile with CSO file load)

 Validation:
 1. Build succeeds with msbuild and CSO files appear in output
 2. Delete embedded shader strings - app still runs
 3. HLSL syntax error → build fails (not runtime)

 Risk: Low | Effort: ~0.5-1 day | Reversibility: Full

 ---
 Implementation Plan (Direction 1)

 Step 1: Create HLSL Files

 Extract embedded shaders from ShaderLibrary.cpp (lines 14-184):
 ┌──────────────────────┬──────────────────────────────────────────┬──────────────────────────────────────────────┐
 │         File         │                  Source                  │                   Contents                   │
 ├──────────────────────┼──────────────────────────────────────────┼──────────────────────────────────────────────┤
 │ shaders/common.hlsli │ New                                      │ TransformData struct, FrameCB, register docs │
 ├──────────────────────┼──────────────────────────────────────────┼──────────────────────────────────────────────┤
 │ shaders/cube.hlsl    │ kVertexShader + kPixelShader             │ Cube VS/PS with instancing                   │
 ├──────────────────────┼──────────────────────────────────────────┼──────────────────────────────────────────────┤
 │ shaders/floor.hlsl   │ kFloorVertexShader + kFloorPixelShader   │ Floor VS/PS (no transform reads)             │
 ├──────────────────────┼──────────────────────────────────────────┼──────────────────────────────────────────────┤
 │ shaders/marker.hlsl  │ kMarkerVertexShader + kMarkerPixelShader │ Marker pass-through VS/PS                    │
 └──────────────────────┴──────────────────────────────────────────┴──────────────────────────────────────────────┘
 Step 2: Add FXC Build Rules

 Modify DX12EngineLab.vcxproj:
 - Add <FxCompile> item group for each .hlsl file
 - Output .cso to $(OutDir)shaders/
 - Target: vs_5_1 / ps_5_1
 - Entry points: VSMain, PSMain

 Step 3: Modify ShaderLibrary

 Replace D3DCompile() calls with CSO file loading:

 // New method
 bool ShaderLibrary::LoadCompiledShader(const wchar_t* path, ID3DBlob** blob);

 // In Initialize():
 LoadCompiledShader(L"shaders/cube_vs.cso", &m_vsBlob);
 LoadCompiledShader(L"shaders/cube_ps.cso", &m_psBlob);
 // ... etc

 Remove embedded shader strings (lines 14-184 in ShaderLibrary.cpp).

 Step 4: Validate

 1. msbuild /p:Configuration=Debug /p:Platform=x64 - builds clean
 2. Check $(OutDir)shaders/*.cso files exist
 3. Run app - same visual output as before
 4. Introduce HLSL syntax error - build fails (not runtime)
 5. Debug layer: 0 errors

 ---
 Direction 2: Moderate (+ Linear Allocator + Pass Separation)

 Scope: Add frame-slice linear allocator for upload heaps, separate render into phases.

 New component - FrameLinearAllocator:
 class FrameLinearAllocator {
     void Reset();  // Called in BeginFrame
     Allocation Allocate(uint64_t size, uint64_t alignment);
     // Returns { void* cpuPtr, D3D12_GPU_VIRTUAL_ADDRESS gpuVA }
 };

 Render phase separation:
 void Dx12Context::Tick() {
     FrameContext& ctx = m_frameRing.BeginFrame(m_frameId);

     // Phase 1: Upload (CPU)
     UpdateFrameConstants(ctx);
     UpdateTransforms(ctx);

     // Phase 2: Record (CPU)
     RecordBarriers(ctx);
     RecordPasses(ctx);  // Floor, Cubes, Markers

     // Phase 3: Execute (GPU)
     ExecuteAndPresent(ctx);
 }

 Files to modify:
 - Create: Renderer/DX12/FrameLinearAllocator.h/.cpp
 - Modify: FrameContextRing.h - integrate allocator
 - Modify: Dx12Context.cpp - split Render() into phases

 Validation:
 1. Log allocator offset - resets each frame?
 2. GBV clean - no "resource still in use" errors?
 3. Frame timing unchanged (no regression)

 Risk: Medium | Effort: ~2-3 days | Reversibility: High

 ---
 Direction 3: More Structured (+ Descriptor Ring + PSO Cache)

 Scope: Ring allocator for transient descriptors, PSO cache for deferred creation.

 New components:
 - DescriptorRingAllocator - fence-protected ring for shader-visible heap
 - PSOCache - hash-based lazy PSO creation
 - ResourceRegistry - separates resource lifetime from binding

 When to add:
 - Descriptor ring: when adding dynamic textures
 - PSO cache: when adding material variations
 - Resource registry: when dynamic resource creation is needed

 Risk: Medium-High | Effort: ~5-7 days | Reversibility: Medium

 ---
 Recommended Path (Incremental)

 Day N:   Direction 1 - Shader extraction
 Day N+1: Direction 2a - FrameLinearAllocator
 Day N+2: Direction 2b - Pass separation
 Later:   Direction 3 components (on-demand)

 Each step = separate PR with contract in docs/contracts/.

 ---
 Root Signature ABI (No Change)

 Current ABI is clean - preserve it:
 enum RootParam : uint32_t {
     RP_FrameCB = 0,          // b0 - inline CBV (ViewProj)
     RP_TransformsTable = 1,  // t0 - descriptor table (SRV)
     RP_InstanceOffset = 2,   // b1 - root constant (1 DWORD)
     RP_Count
 };

 Document in common.hlsli with comments matching register assignments.

 ---
 Common DX12 Failure Modes to Watch
 ┌───────────────────────┬────────────────────────┬────────────────────────────────────────────┐
 │        Failure        │        Symptom         │                 Prevention                 │
 ├───────────────────────┼────────────────────────┼────────────────────────────────────────────┤
 │ Wrong heap bound      │ GPU crash / corruption │ Assert heap type before SetDescriptorHeaps │
 ├───────────────────────┼────────────────────────┼────────────────────────────────────────────┤
 │ Wrong descriptor slot │ Wrong texture/buffer   │ Use named constants (RootParam enum)       │
 ├───────────────────────┼────────────────────────┼────────────────────────────────────────────┤
 │ Stale GPU handle      │ Flicker / old data     │ Fence-gated reuse (already done)           │
 ├───────────────────────┼────────────────────────┼────────────────────────────────────────────┤
 │ Wrong stride          │ Garbled vertex data    │ Assert stride matches shader expectation   │
 ├───────────────────────┼────────────────────────┼────────────────────────────────────────────┤
 │ Wrong register        │ Shader reads zero      │ HLSL register must match root param        │
 └───────────────────────┴────────────────────────┴────────────────────────────────────────────┘
 ---
 Quick Proof Hooks

 For each change, add one of:
 - Log: Print key values once/sec (frame index, allocator offset, descriptor slot)
 - Assert: Validate invariants (heap type, fence completion, buffer size)
 - Visual: Toggle that breaks correctness (stomp_Lifetime pattern already exists)

 ---
 Critical Files
 ┌──────────────────────────────────┬──────────────────────────────────┐
 │               File               │             Purpose              │
 ├──────────────────────────────────┼──────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.cpp  │ Replace D3DCompile with CSO load │
 ├──────────────────────────────────┼──────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp    │ Split monolithic Render()        │
 ├──────────────────────────────────┼──────────────────────────────────┤
 │ Renderer/DX12/FrameContextRing.h │ Extend with linear allocator     │
 ├──────────────────────────────────┼──────────────────────────────────┤
 │ DX12EngineLab.vcxproj            │ Add FXC compile rules            │
 ├──────────────────────────────────┼──────────────────────────────────┤
 │ docs/contracts/                  │ Document each change             │
 └──────────────────────────────────┴──────────────────────────────────┘
 ---
 Verification Plan

 After each direction:
 1. Build: msbuild /p:Configuration=Debug /p:Platform=x64 - green
 2. Run: Debug layer ON, 0 errors in happy path
 3. Visual: Same output as before (no regression)
 4. Stress: Run for 1000+ frames with GBV enabled


 -------------------------------------------
 Implementation complete. Here's a summary of what was done:

  Changes Made                                                                                                               
  1. Created HLSL shader files (shaders/):                                                                                     - common.hlsli - Shared types (TransformData, FrameCB, VSInput) with ABI documentation
  - cube_vs.hlsl / cube_ps.hlsl - Cube instanced rendering shaders
  - floor_vs.hlsl / floor_ps.hlsl - Floor shaders (no transform reads)
  - marker_vs.hlsl / marker_ps.hlsl - Diagnostic marker shaders

  2. Updated DX12EngineLab.vcxproj:
  - Added <FxCompile> items for all 6 shader files
  - Configured output to $(OutDir)shaders/*.cso
  - Set shader model 5.1 and correct entry points

  3. Modified ShaderLibrary.cpp:
  - Removed 170+ lines of embedded shader strings
  - Replaced CompileShaders() with LoadShaders()
  - Added LoadCompiledShader() to read CSO files from disk

  4. Updated ShaderLibrary.h:
  - Changed method signatures from CompileShaders to LoadShaders/LoadCompiledShader

  Verification Steps

  To verify on Windows (in VS2022 or cmd):

  # 1. Build the project
  msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64

  # 2. Check CSO files were generated
  dir x64\Debug\shaders\*.cso

  # 3. Run the app - should display same visual output as before

  # 4. Test build-time error detection: add syntax error to cube_vs.hlsl, rebuild

  Notes

  - MICROTEST_MODE is not yet supported with the new system (was a diagnostic mode with ByteAddressBuffer). Can be added
  later with separate shader files if needed.
  - Shader files use relative paths (shaders/*.cso) - app must be run from the output directory or working directory must be
  set correctly.





  -----------------------------------------


  After the last change, the app exits during shader load (not a crash). Output log:

Failed to open shader file: shaders/cube_vs.cso
ShaderLibrary: Failed to load shaders
Failed to initialize shader library

Please fix so the runtime can successfully find and load shaders/cube_vs.cso again.
Keep the change minimal and only inside the repo.

 The fix is minimal - just add a helper to resolve paths relative to the executable's location. This is a single-file change to ShaderLibrary.cpp.

────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?                                                                                                                                                                                                           
 Here is Claude's plan:                                                                                                                                                                                                   ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Fix: Shader CSO Path Resolution                                                                                                                                                                                          
 Problem

 App fails to load shaders at runtime:
 Failed to open shader file: shaders/cube_vs.cso
 ShaderLibrary: Failed to load shaders

 Root cause: LoadShaders() uses relative path L"shaders/cube_vs.cso" but:
 - CSO files are output to $(OutDir)shaders/ (e.g., x64/Debug/shaders/)
 - Working directory at runtime may not be the output directory

 Fix

 Resolve shader paths relative to the executable's directory using GetModuleFileNameW.

 Changes to ShaderLibrary.cpp

 1. Add helper function to get exe directory:
 static std::wstring GetExeDirectory()
 {
     wchar_t path[MAX_PATH];
     GetModuleFileNameW(nullptr, path, MAX_PATH);
     std::wstring exePath(path);
     size_t lastSlash = exePath.find_last_of(L"\\/");
     return (lastSlash != std::wstring::npos) ? exePath.substr(0, lastSlash + 1) : L"";
 }

 2. Modify LoadShaders() to use absolute paths:
 bool ShaderLibrary::LoadShaders()
 {
     std::wstring exeDir = GetExeDirectory();

     if (!LoadCompiledShader((exeDir + L"shaders/cube_vs.cso").c_str(), m_vsBlob))
         return false;
     // ... etc for all 6 shaders
 }

 3. Add include for <string> (for std::wstring)

 File to modify

 - Renderer/DX12/ShaderLibrary.cpp - add path resolution

 Verification

 1. Build: msbuild /p:Configuration=Debug /p:Platform=x64
 2. Run from any directory - should load shaders successfully
 3. Verify same visual output as before
 -------------------------------

 Shader question / request:

Right now PS is hard-coded to return faceDebugColors based on SV_PrimitiveID. 
Can you refactor this to be more “production-like” while still supporting debug?

- Keep a debug face-color mode, but don’t hard-lock the shader to it.
- Make colors configurable/managed (not scattered literals): e.g. constants in a shared header, a small constant buffer, or a palette table.
- Add a more dynamic option like:
  - mode A: face debug colors
  - mode B: instanceID-based coloring (stable per instance)
  - mode C: simple Lambert lighting using WorldPos/normal (so it looks like real shading)
- Keep changes minimal and readable; prefer common industry practice.
Also: Is the current “SV_PrimitiveID face coloring” approach something people actually keep around in real projects, or is it usually separated (different shader/PSO or debug toggle)?

-------------------------------------------
 I've written the plan addressing your questions and outlining a production-style approach with runtime mode switching. The key insight is that option #2
  (constant buffer toggle) is most practical for a dev sandbox - you get runtime switching without PSO swaps or recompiles.
                                                                                                                                                               ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Shader Coloring Refactor: Multi-Mode PS with Debug Support

 Industry Context (Answer to Your Question)

 Is SV_PrimitiveID face coloring kept in production?

 In real projects, debug visualization is typically handled one of these ways:

 1. Separate debug shaders/PSO (most common in shipped games) - Different PSO swapped in for debug mode. Cleaner separation, zero cost in release.
 2. Runtime toggle via constant buffer (common during development) - Single shader with mode switch. Convenient for dev iteration, slight overhead from
 branch.
 3. Preprocessor defines (#ifdef DEBUG_COLORS) - Compile-time switch between debug/release variants. Zero runtime cost but requires rebuild.
 4. RenderDoc/PIX overlays - External tool visualization, no shader changes needed.

 For a development sandbox, option #2 (constant buffer toggle) is the practical choice - it lets you switch modes at runtime without recompiling or swapping
 PSOs.

 ---
 Design

 Approach: Constant Buffer Mode Switch

 Add a small DebugCB constant buffer with:
 - colorMode: 0=FaceDebug, 1=InstanceID, 2=Lambert
 - Face colors defined as constants in shader (or separate header)

 This keeps one PSO, minimal changes, and enables runtime switching.

 Changes Required

 1. shaders/common.hlsli - Add debug constants

 // Color mode constants
 static const uint COLOR_MODE_FACE_DEBUG = 0;
 static const uint COLOR_MODE_INSTANCE_ID = 1;
 static const uint COLOR_MODE_LAMBERT = 2;

 // Debug CB - bound at b2 space0 (new root slot)
 cbuffer DebugCB : register(b2, space0)
 {
     uint ColorMode;
     uint _pad0, _pad1, _pad2;  // Pad to 16 bytes
 };

 // Face debug palette (shared so PS can use it)
 static const float3 kFaceColors[6] = {
     float3(0, 1, 0),      // -Z: Green
     float3(1, 1, 0),      // +Z: Yellow
     float3(0, 0, 1),      // -X: Blue
     float3(1, 0.5, 0),    // +X: Orange
     float3(1, 0, 0),      // +Y: Red
     float3(0, 1, 1),      // -Y: Cyan
 };

 2. shaders/cube_vs.hlsl - Pass instance ID and world normal

 struct VSOutput
 {
     float4 Pos : SV_Position;
     float3 WorldPos : TEXCOORD0;
     float3 Normal : TEXCOORD1;    // NEW: for Lambert
     uint InstanceID : TEXCOORD2;  // NEW: for instance coloring
 };

 Compute normal from local position (cube centered at origin, normals point outward along dominant axis).

 3. shaders/cube_ps.hlsl - Multi-mode pixel shader

 float4 PSMain(PSInput pin, uint primID : SV_PrimitiveID) : SV_Target
 {
     float3 color;

     if (ColorMode == COLOR_MODE_FACE_DEBUG)
     {
         uint faceIndex = (primID % 12) / 2;
         color = kFaceColors[faceIndex];
     }
     else if (ColorMode == COLOR_MODE_INSTANCE_ID)
     {
         // Stable hue per instance (golden ratio hash for good distribution)
         float hue = frac(pin.InstanceID * 0.618033988749);
         color = HueToRGB(hue);
     }
     else // COLOR_MODE_LAMBERT
     {
         float3 lightDir = normalize(float3(1, 1, -1));
         float NdotL = saturate(dot(normalize(pin.Normal), lightDir));
         color = float3(0.8, 0.8, 0.8) * (0.3 + 0.7 * NdotL);  // Gray + diffuse
     }

     return float4(color, 1.0);
 }

 4. Renderer/DX12/ShaderLibrary.h - Add RP_DebugCB to enum

 enum RootParam : uint32_t
 {
     RP_FrameCB = 0,
     RP_TransformsTable = 1,
     RP_InstanceOffset = 2,
     RP_DebugCB = 3,        // NEW: b2 space0
     RP_Count
 };

 5. Renderer/DX12/ShaderLibrary.cpp - Add root param for DebugCB

 Add new root parameter in CreateRootSignature():
 rootParams[RP_DebugCB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
 rootParams[RP_DebugCB].Constants.ShaderRegister = 2;  // b2
 rootParams[RP_DebugCB].Constants.RegisterSpace = 0;
 rootParams[RP_DebugCB].Constants.Num32BitValues = 4;  // uint + 3 pad
 rootParams[RP_DebugCB].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

 6. Renderer/DX12/Dx12Context.cpp - Set color mode before draw

 uint32_t colorMode = 0;  // Default to face debug (or make configurable)
 m_commandList->SetGraphicsRoot32BitConstants(RP_DebugCB, 1, &colorMode, 0);

 ---
 Files to Modify
 ┌─────────────────────────────────┬─────────────────────────────────────────────────┐
 │              File               │                     Changes                     │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ shaders/common.hlsli            │ Add DebugCB, color mode constants, face palette │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ shaders/cube_vs.hlsl            │ Pass Normal and InstanceID to PS                │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ shaders/cube_ps.hlsl            │ Multi-mode color selection                      │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.h   │ Add RP_DebugCB enum                             │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.cpp │ Add root param for DebugCB                      │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp   │ Set color mode constant before draw             │
 └─────────────────────────────────┴─────────────────────────────────────────────────┘
 ---
 Verification

 1. Build: msbuild /p:Configuration=Debug /p:Platform=x64
 2. Run with mode 0 (face debug): Should match current output exactly
 3. Run with mode 1 (instance ID): Each cube gets distinct stable color
 4. Run with mode 2 (Lambert): Cubes appear gray with directional lighting
 5. Debug layer: 0 errors

 ---
 Optional Future Extensions

 - Make ColorMode settable via keyboard (e.g., 1/2/3 keys)
 - Add more modes: wireframe overlay, depth visualization, UV debug
 - Move palette to structured buffer for runtime customization
 ---------------------------------------------------------------------------------
 Your shader multi-mode plan looks fine.

My higher priority next change: camera controls.
Right now I have camera presets (0/1/2 etc) that jump between fixed views. I want to switch to a real interactive camera:

- Use keyboard (arrow keys / WASD) to move the camera continuously (translation).
- Add look control (mouse or QE/arrow) for yaw/pitch (or at least yaw).
- Keep presets optionally (for quick reset), but the default should be “free camera”.
- Please propose a minimal, engine-friendly implementation plan:
  - input mapping
  - camera state (pos, yaw/pitch, speed)
  - per-frame update (dt)
  - View matrix update + how it flows into the existing FrameCB/ViewProj
- Keep it incremental and don’t break the existing render pipeline.
-----------------------------------------------------------------------------------------------

The plan is complete. Key points:

  - Single file change (Dx12Context.cpp) - keeps it minimal                                                                                                      - WASD + arrows for movement, Q/E for yaw rotation
  - Space/Ctrl for vertical movement                                                                                                                             - 1/2/3 preserved for quick preset resets
  - Delta time via QueryPerformanceCounter for frame-rate independent movement
  - View matrix built from position + yaw/pitch (no fixed target)

─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Free Camera Implementation Plan

 Current State

 - Camera uses preset descriptors (eye, target, up, fov, near/far)
 - Keys 1/2/3 switch between 3 presets via GetAsyncKeyState with debounce
 - BuildViewProj uses XMMatrixLookAtRH with fixed target
 - No delta time tracking - App::Tick() just calls Render()

 Design

 Camera State (add to Dx12Context.cpp)

 struct FreeCamera
 {
     XMFLOAT3 position = {0.0f, 180.0f, -220.0f};  // Start at preset A
     float yaw = 0.0f;      // Radians, 0 = looking along +Z
     float pitch = -0.5f;   // Radians, negative = looking down
     float fovY = XM_PIDIV4;
     float nearZ = 1.0f;
     float farZ = 1000.0f;
     float moveSpeed = 100.0f;   // Units per second
     float lookSpeed = 1.5f;     // Radians per second
 };

 static FreeCamera s_camera;
 static LARGE_INTEGER s_lastTime = {};
 static LARGE_INTEGER s_frequency = {};

 Input Mapping
 ┌─────────────────┬──────────────────────────┐
 │       Key       │          Action          │
 ├─────────────────┼──────────────────────────┤
 │ W / Up Arrow    │ Move forward             │
 ├─────────────────┼──────────────────────────┤
 │ S / Down Arrow  │ Move backward            │
 ├─────────────────┼──────────────────────────┤
 │ A / Left Arrow  │ Strafe left              │
 ├─────────────────┼──────────────────────────┤
 │ D / Right Arrow │ Strafe right             │
 ├─────────────────┼──────────────────────────┤
 │ Q               │ Yaw left (look left)     │
 ├─────────────────┼──────────────────────────┤
 │ E               │ Yaw right (look right)   │
 ├─────────────────┼──────────────────────────┤
 │ Space           │ Move up                  │
 ├─────────────────┼──────────────────────────┤
 │ Ctrl            │ Move down                │
 ├─────────────────┼──────────────────────────┤
 │ 1/2/3           │ Reset to preset position │
 └─────────────────┴──────────────────────────┘
 View Matrix from Yaw/Pitch

 static XMMATRIX BuildFreeCameraViewProj(const FreeCamera& cam, float aspect)
 {
     // Forward vector from yaw/pitch (RH: -Z is forward at yaw=0)
     float cosP = cosf(cam.pitch);
     XMFLOAT3 forward = {
         sinf(cam.yaw) * cosP,
         sinf(cam.pitch),
         cosf(cam.yaw) * cosP
     };

     XMVECTOR eye = XMLoadFloat3(&cam.position);
     XMVECTOR fwd = XMLoadFloat3(&forward);
     XMVECTOR target = XMVectorAdd(eye, fwd);
     XMVECTOR up = XMVectorSet(0, 1, 0, 0);

     XMMATRIX view = XMMatrixLookAtRH(eye, target, up);
     XMMATRIX proj = XMMatrixPerspectiveFovRH(cam.fovY, aspect, cam.nearZ, cam.farZ);
     return XMMatrixMultiply(view, proj);
 }

 Per-Frame Update

 static void UpdateFreeCamera(float dt)
 {
     // Movement input
     float moveX = 0.0f, moveY = 0.0f, moveZ = 0.0f;
     if (GetAsyncKeyState('W') & 0x8000 || GetAsyncKeyState(VK_UP) & 0x8000)    moveZ += 1.0f;
     if (GetAsyncKeyState('S') & 0x8000 || GetAsyncKeyState(VK_DOWN) & 0x8000)  moveZ -= 1.0f;
     if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000)  moveX -= 1.0f;
     if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000) moveX += 1.0f;
     if (GetAsyncKeyState(VK_SPACE) & 0x8000)  moveY += 1.0f;
     if (GetAsyncKeyState(VK_CONTROL) & 0x8000) moveY -= 1.0f;

     // Look input
     float yawDelta = 0.0f;
     if (GetAsyncKeyState('Q') & 0x8000) yawDelta -= 1.0f;
     if (GetAsyncKeyState('E') & 0x8000) yawDelta += 1.0f;

     // Apply yaw
     s_camera.yaw += yawDelta * s_camera.lookSpeed * dt;

     // Build movement vectors in camera space
     float cosY = cosf(s_camera.yaw);
     float sinY = sinf(s_camera.yaw);
     XMFLOAT3 forward = { sinY, 0, cosY };  // Horizontal forward
     XMFLOAT3 right = { cosY, 0, -sinY };   // Horizontal right

     float speed = s_camera.moveSpeed * dt;
     s_camera.position.x += (forward.x * moveZ + right.x * moveX) * speed;
     s_camera.position.z += (forward.z * moveZ + right.z * moveX) * speed;
     s_camera.position.y += moveY * speed;
 }

 Preset Reset

 When 1/2/3 is pressed, copy preset values into s_camera:

 static void ResetCameraToPreset(CameraPreset preset)
 {
     const CameraPresetDesc& desc = kCameraPresets[static_cast<uint32_t>(preset)];
     s_camera.position = desc.eye;
     s_camera.fovY = desc.fovY;
     s_camera.nearZ = desc.nearZ;
     s_camera.farZ = desc.farZ;

     // Compute yaw/pitch from eye->target direction
     XMFLOAT3 dir = {
         desc.target.x - desc.eye.x,
         desc.target.y - desc.eye.y,
         desc.target.z - desc.eye.z
     };
     s_camera.yaw = atan2f(dir.x, dir.z);
     s_camera.pitch = atan2f(dir.y, sqrtf(dir.x*dir.x + dir.z*dir.z));
 }

 ---
 Files to Modify
 ┌───────────────────────────────┬──────────────────────────────────────────────────────────────────────────────────────────────────┐
 │             File              │                                             Changes                                              │
 ├───────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp │ Add FreeCamera state, delta time, UpdateFreeCamera(), BuildFreeCameraViewProj(), modify Render() │
 └───────────────────────────────┴──────────────────────────────────────────────────────────────────────────────────────────────────┘
 No new files needed - all changes in one file.

 ---
 Changes to Dx12Context.cpp

 1. Add FreeCamera struct and state (after CameraPresetDesc, ~line 36)
 2. Add timer variables for delta time calculation
 3. Add UpdateFreeCamera(float dt) function
 4. Add BuildFreeCameraViewProj() function
 5. Add ResetCameraToPreset() function
 6. Modify Initialize(): Initialize timer with QueryPerformanceFrequency
 7. Modify Render():
   - Calculate delta time at start
   - Call UpdateFreeCamera(dt)
   - Change preset keys to call ResetCameraToPreset()
   - Replace BuildViewProj(camDesc, aspect) with BuildFreeCameraViewProj(s_camera, aspect)

 ---
 Verification

 1. Build: msbuild /p:Configuration=Debug /p:Platform=x64
 2. Run - camera should start at preset A position
 3. WASD movement: Cubes should move in view as expected
 4. Q/E rotation: View should yaw left/right
 5. Space/Ctrl: Camera moves up/down
 6. Press 1/2/3: Camera resets to preset positions
 7. Debug layer: 0 errors

 ---
 Future Extensions (not in this PR)

 - Mouse look (WM_MOUSEMOVE + captured mouse)
 - Pitch control (R/F keys or mouse Y)
 - Shift for sprint, scroll wheel for speed adjustment
 - Smooth acceleration/deceleration
 -------------------------------------------------------
 Both Debug and Release build successfully. The free camera implementation is complete.

  Summary of Changes                                                                                                                                           
  Added to Renderer/DX12/Dx12Context.cpp:                                                                                                                      
  1. FreeCamera struct - Holds position, yaw, pitch, fov, near/far, movement speed, look speed
  2. Timer state - s_lastTime, s_frequency, s_timerInitialized for delta time calculation
  3. Helper functions:
    - ResetCameraToPreset() - Computes yaw/pitch from preset's eye→target direction
    - UpdateFreeCamera(dt) - Processes input and updates camera state
    - BuildFreeCameraViewProj() - Builds view matrix from yaw/pitch orientation
  4. Initialize() - Added timer init and initial camera reset to preset A
  5. Render() - Added delta time calculation, camera update, and switched to free camera view matrix

  Controls
  ┌─────────────────┬──────────────────────────┐
  │       Key       │          Action          │
  ├─────────────────┼──────────────────────────┤
  │ W / Up Arrow    │ Move forward             │
  ├─────────────────┼──────────────────────────┤
  │ S / Down Arrow  │ Move backward            │
  ├─────────────────┼──────────────────────────┤
  │ A / Left Arrow  │ Strafe left              │
  ├─────────────────┼──────────────────────────┤
  │ D / Right Arrow │ Strafe right             │
  ├─────────────────┼──────────────────────────┤
  │ Q               │ Yaw left                 │
  ├─────────────────┼──────────────────────────┤
  │ E               │ Yaw right                │
  ├─────────────────┼──────────────────────────┤
  │ Space           │ Move up                  │
  ├─────────────────┼──────────────────────────┤
  │ Ctrl            │ Move down                │
  ├─────────────────┼──────────────────────────┤
  │ 1/2/3           │ Reset to preset position │
  └─────────────────┴──────────────────────────┘
  ----------------------------------------
  ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Fix Moiré Pattern and Disable Corner Markers

 Problem

 1. Moiré on cube tops: The 100x100 grid of cubes with 0.9 scale creates high-frequency patterns (red tops + small gaps) that alias when viewed from a
 distance.
 2. Corner markers: Magenta debug markers are visible at screen corners - should be off by default.

 Solution

 1. Anti-Aliased Cube Edges (cube_ps.hlsl)

 Add edge darkening using fwidth to create smooth anti-aliased transitions at cube boundaries:

 float4 PSMain(PSInput pin, uint primID : SV_PrimitiveID) : SV_Target
 {
     // Face colors (existing)
     uint faceIndex = (primID % 12) / 2;
     float3 baseColor = faceDebugColors[faceIndex];

     // Anti-alias edges using world position derivatives
     float3 worldFrac = frac(pin.WorldPos);
     float3 edgeDist = min(worldFrac, 1.0 - worldFrac);
     float3 fw = fwidth(pin.WorldPos);
     float edgeFactor = smoothstep(0.0, fw.x * 2.0, min(edgeDist.x, edgeDist.z));

     // Darken edges slightly to break up aliasing
     float3 finalColor = baseColor * lerp(0.7, 1.0, edgeFactor);
     return float4(finalColor, 1.0);
 }

 2. Disable Markers by Default (ToggleSystem.h + Dx12Context.cpp)

 ToggleSystem.h - Add marker toggle:
 static bool IsMarkersEnabled() { return s_markersEnabled; }
 static void ToggleMarkers() { s_markersEnabled = !s_markersEnabled; }
 // ...
 static inline bool s_markersEnabled = false;  // OFF by default

 Dx12Context.cpp - Wrap marker drawing:
 // 15b. Draw corner markers (only if enabled)
 if (ToggleSystem::IsMarkersEnabled())
 {
     m_commandList->SetGraphicsRootSignature(m_shaderLibrary.GetMarkerRootSignature());
     m_commandList->SetPipelineState(m_shaderLibrary.GetMarkerPSO());
     m_scene.RecordDrawMarkers(m_commandList.Get());
     drawCalls += 1;
 }

 ---
 Files to Modify
 ┌───────────────────────────────┬─────────────────────────────────────────────┐
 │             File              │                   Change                    │
 ├───────────────────────────────┼─────────────────────────────────────────────┤
 │ shaders/cube_ps.hlsl          │ Add fwidth-based edge anti-aliasing         │
 ├───────────────────────────────┼─────────────────────────────────────────────┤
 │ Renderer/DX12/ToggleSystem.h  │ Add IsMarkersEnabled() toggle (default OFF) │
 ├───────────────────────────────┼─────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp │ Wrap marker draw call with toggle check     │
 └───────────────────────────────┴─────────────────────────────────────────────┘
 ---
 Verification

 1. msbuild /p:Configuration=Debug /p:Platform=x64
 2. Run - cubes should have less moiré, no magenta corner markers
 3. Move camera around - moiré pattern should be reduced
 4. Debug layer: 0 errors
 -------------------
  Ready to code?
                                                                                                                                                                Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Shader Coloring Refactor + Camera Cleanup

 Overview

 Two changes:
 1. Shader refactor: Multi-mode coloring via constant buffer (option #2 - runtime switching)
 2. Camera cleanup: Remove preset system, keep only free camera

 Coloring Modes
 ┌──────┬─────────────┬──────────────────────────────────────────────────┐
 │ Mode │    Name     │                   Description                    │
 ├──────┼─────────────┼──────────────────────────────────────────────────┤
 │ 0    │ Face Debug  │ Current behavior - face colors by SV_PrimitiveID │
 ├──────┼─────────────┼──────────────────────────────────────────────────┤
 │ 1    │ Instance ID │ Stable hue per instance (golden ratio hash)      │
 ├──────┼─────────────┼──────────────────────────────────────────────────┤
 │ 2    │ Lambert     │ Simple directional lighting (gray diffuse)       │
 └──────┴─────────────┴──────────────────────────────────────────────────┘
 ---
 Changes Required

 1. shaders/common.hlsli (NEW FILE)

 Add shared constants and DebugCB:

 // Color mode constants
 static const uint COLOR_MODE_FACE_DEBUG = 0;
 static const uint COLOR_MODE_INSTANCE_ID = 1;
 static const uint COLOR_MODE_LAMBERT = 2;

 // Debug CB - bound at b2 space0
 cbuffer DebugCB : register(b2, space0)
 {
     uint ColorMode;
     uint _pad0, _pad1, _pad2;  // Pad to 16 bytes
 };

 // Face debug palette
 static const float3 kFaceColors[6] = {
     float3(0, 1, 0),      // -Z: Green
     float3(1, 1, 0),      // +Z: Yellow
     float3(0, 0, 1),      // -X: Blue
     float3(1, 0.5, 0),    // +X: Orange
     float3(1, 0, 0),      // +Y: Red
     float3(0, 1, 1),      // -Y: Cyan
 };

 2. shaders/cube_vs.hlsl

 Pass Normal and InstanceID to PS:

 struct VSOutput
 {
     float4 Pos : SV_Position;
     float3 WorldPos : TEXCOORD0;
     float3 Normal : TEXCOORD1;    // NEW: for Lambert
     uint InstanceID : TEXCOORD2;  // NEW: for instance coloring
 };

 Compute normal from local vertex position (cube normals point along dominant axis).

 3. shaders/cube_ps.hlsl

 Multi-mode color selection:

 #include "common.hlsli"

 float3 HueToRGB(float hue)
 {
     float3 rgb = abs(hue * 6.0 - float3(3, 2, 4)) * float3(1, -1, -1) + float3(-1, 2, 2);
     return saturate(rgb);
 }

 float4 PSMain(PSInput pin, uint primID : SV_PrimitiveID) : SV_Target
 {
     float3 color;

     if (ColorMode == COLOR_MODE_FACE_DEBUG)
     {
         uint faceIndex = (primID % 12) / 2;
         color = kFaceColors[faceIndex];
     }
     else if (ColorMode == COLOR_MODE_INSTANCE_ID)
     {
         float hue = frac(pin.InstanceID * 0.618033988749);
         color = HueToRGB(hue);
     }
     else // COLOR_MODE_LAMBERT
     {
         float3 lightDir = normalize(float3(1, 1, -1));
         float NdotL = saturate(dot(normalize(pin.Normal), lightDir));
         color = float3(0.8, 0.8, 0.8) * (0.3 + 0.7 * NdotL);
     }

     // Keep anti-alias edge darkening from moiré fix
     float3 worldFrac = frac(pin.WorldPos);
     float3 edgeDist = min(worldFrac, 1.0 - worldFrac);
     float3 fw = fwidth(pin.WorldPos);
     float edgeFactor = smoothstep(0.0, fw.x * 2.0, min(edgeDist.x, edgeDist.z));
     color *= lerp(0.7, 1.0, edgeFactor);

     return float4(color, 1.0);
 }

 4. Renderer/DX12/ShaderLibrary.h

 Add RP_DebugCB to RootParam enum:

 enum RootParam : uint32_t
 {
     RP_FrameCB = 0,
     RP_TransformsTable = 1,
     RP_InstanceOffset = 2,
     RP_DebugCB = 3,        // NEW: b2 space0
     RP_Count
 };

 5. Renderer/DX12/ShaderLibrary.cpp

 Add root parameter for DebugCB in CreateRootSignature():

 rootParams[RP_DebugCB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
 rootParams[RP_DebugCB].Constants.ShaderRegister = 2;  // b2
 rootParams[RP_DebugCB].Constants.RegisterSpace = 0;
 rootParams[RP_DebugCB].Constants.Num32BitValues = 4;  // uint + 3 pad
 rootParams[RP_DebugCB].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

 6. Renderer/DX12/ToggleSystem.h

 Add color mode toggle:

 enum class ColorMode : uint32_t { FaceDebug = 0, InstanceID = 1, Lambert = 2 };

 static ColorMode GetColorMode() { return s_colorMode; }
 static void CycleColorMode() {
     s_colorMode = static_cast<ColorMode>((static_cast<uint32_t>(s_colorMode) + 1) % 3);
 }
 // ...
 static inline ColorMode s_colorMode = ColorMode::FaceDebug;

 7. Renderer/DX12/Dx12Context.cpp

 Set color mode constant before cube draw:

 uint32_t colorMode = static_cast<uint32_t>(ToggleSystem::GetColorMode());
 m_commandList->SetGraphicsRoot32BitConstants(RP_DebugCB, 1, &colorMode, 0);

 ---
 Files Summary
 ┌─────────────────────────────────┬─────────────────────────────────────────────┐
 │              File               │                   Action                    │
 ├─────────────────────────────────┼─────────────────────────────────────────────┤
 │ shaders/common.hlsli            │ CREATE - shared constants, DebugCB, palette │
 ├─────────────────────────────────┼─────────────────────────────────────────────┤
 │ shaders/cube_vs.hlsl            │ MODIFY - add Normal/InstanceID outputs      │
 ├─────────────────────────────────┼─────────────────────────────────────────────┤
 │ shaders/cube_ps.hlsl            │ MODIFY - multi-mode color selection         │
 ├─────────────────────────────────┼─────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.h   │ MODIFY - add RP_DebugCB enum                │
 ├─────────────────────────────────┼─────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.cpp │ MODIFY - add root param                     │
 ├─────────────────────────────────┼─────────────────────────────────────────────┤
 │ Renderer/DX12/ToggleSystem.h    │ MODIFY - add ColorMode toggle               │
 ├─────────────────────────────────┼─────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp   │ MODIFY - set color mode constant            │
 └─────────────────────────────────┴─────────────────────────────────────────────┘
 ---
 ---
 Part 2: Camera Preset Cleanup

 Remove from Renderer/DX12/Dx12Context.cpp

 Delete the following (keeping only FreeCamera):

 // DELETE these:
 enum class CameraPreset : uint32_t { A, B, C, Count };
 struct CameraPresetDesc { ... };
 static const CameraPresetDesc kCameraPresets[] = { ... };
 static CameraPreset s_currentCameraPreset = CameraPreset::A;
 static bool s_camKeyWasDown[3] = { ... };
 static const char* GetCameraPresetName(CameraPreset preset) { ... }
 static XMMATRIX BuildViewProj(const CameraPresetDesc& desc, float aspect) { ... }
 static void ResetCameraToPreset(CameraPreset preset) { ... }

 // DELETE hotkey handling block in Render() (lines checking keys 1,2,3)

 Keep only FreeCamera

 - FreeCamera struct with position/yaw/pitch
 - UpdateFreeCamera(float dt)
 - BuildFreeCameraViewProj()
 - Timer state for delta time

 Updated Files Summary
 ┌─────────────────────────────────┬─────────────────────────────────────────────────┐
 │              File               │                     Action                      │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ shaders/common.hlsli            │ CREATE - shared constants, DebugCB, palette     │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ shaders/cube_vs.hlsl            │ MODIFY - add Normal/InstanceID outputs          │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ shaders/cube_ps.hlsl            │ MODIFY - multi-mode color selection             │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.h   │ MODIFY - add RP_DebugCB enum                    │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.cpp │ MODIFY - add root param                         │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ Renderer/DX12/ToggleSystem.h    │ MODIFY - add ColorMode toggle                   │
 ├─────────────────────────────────┼─────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp   │ MODIFY - set color mode + remove camera presets │
 └─────────────────────────────────┴─────────────────────────────────────────────────┘
 ---
 Verification

 1. msbuild /p:Configuration=Debug /p:Platform=x64
 2. Run with mode 0: Should match current face-colored output
 3. Run with mode 1: Each cube gets distinct stable color
 4. Run with mode 2: Cubes appear gray with directional shading
 5. Camera: WASD/QE controls work, no 1/2/3 preset hotkeys
 6. Debug layer: 0 errors
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌
Implementation complete. Here's a summary of all changes:

  Shader Refactor                                                                                                                                                ┌─────────────────────────────────┬─────────────────────────────────────────────────────────────────────────┐
  │              File               │                                 Changes                                 │                                                  ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
  │ shaders/common.hlsli            │ Added COLOR_MODE_* constants, DebugCB at b2, kFaceColors palette        │
  ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
  │ shaders/cube_vs.hlsl            │ Added Normal and InstanceID outputs, normal computed from dominant axis │
  ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
  │ shaders/cube_ps.hlsl            │ Multi-mode color selection (FaceDebug, InstanceID, Lambert)             │
  ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/ShaderLibrary.h   │ Added RP_DebugCB = 3 to RootParam enum                                  │
  ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/ShaderLibrary.cpp │ Added root parameter for b2 (4 DWORDs, pixel-visible)                   │
  ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/ToggleSystem.h    │ Added ColorMode enum, CycleColorMode(), GetColorModeName()              │
  ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/Dx12Context.cpp   │ Added include + color mode constant binding before cube draw            │
  └─────────────────────────────────┴─────────────────────────────────────────────────────────────────────────┘
  Camera Cleanup

  Removed from Dx12Context.cpp:
  - CameraPreset enum, CameraPresetDesc struct, kCameraPresets[]
  - s_currentCameraPreset, s_camKeyWasDown[3]
  - GetCameraPresetName(), BuildViewProj(), ResetCameraToPreset()
  - Camera preset hotkey handling (1/2/3 keys)

  Kept: FreeCamera struct, UpdateFreeCamera(), BuildFreeCameraViewProj(), timer state

  Build Status

  - Debug x64: PASS
  - Release x64: PASS

  Note: ColorMode defaults to FaceDebug (mode 0) - matches the original behavior. To cycle modes at runtime, you'll need to add a hotkey that calls
  ToggleSystem::CycleColorMode().
  -------------------------------------------------------
  I ran the app in Debug and the color modes (0/1/2) do not switch at runtime — it stays stuck on mode 0.

Please wire up a runtime hotkey to change ColorMode (e.g., 'C' cycles 0→1→2) by calling ToggleSystem::CycleColorMode(), and add a one-line debug print when it changes (e.g., “ColorMode = FaceDebug/InstanceID/Lambert”).

Keep the camera presets (1/2/3) disabled as intended, and keep the change minimal.
--------------------------------------------------------------------
 Ready to code?
                                                                                                                                                                Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Add ColorMode Hotkey

 Problem

 ColorMode toggle was implemented in ToggleSystem but no hotkey was wired up to change it at runtime.

 Solution

 Add 'C' key handler in DX12EngineLab.cpp WM_KEYDOWN handler, following the same pattern as 'T' and 'G' keys.

 Changes Required

 DX12EngineLab.cpp (line ~214)

 After the 'G' key handler, add:

 // 'C' key cycles color mode (FaceDebug -> InstanceID -> Lambert)
 else if (wParam == 'C')
 {
     Renderer::ToggleSystem::CycleColorMode();
     char buf[64];
     sprintf_s(buf, "ColorMode = %s\n", Renderer::ToggleSystem::GetColorModeName());
     OutputDebugStringA(buf);
 }

 Verification

 1. Build Debug x64
 2. Run app, press 'C' to cycle modes
 3. Observe debug output: "ColorMode = face", "ColorMode = instance", "ColorMode = lambert"
 4. Verify cubes change visually