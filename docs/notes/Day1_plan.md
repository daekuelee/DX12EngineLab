● I've compiled the complete Day1 plan with all required sections:

  Summary:
  - Option B (Composition): 4 new modules in Renderer/DX12/
  - 8 slices (S0-S7): Each build-green, smallest buildable steps
  - Discrepancies resolved: FrameCount=3, DATA_STATIC_WHILE_SET_AT_EXECUTE, initial state tracking
  - Evidence: Log schema + screenshot naming defined
  - Risk matrix: 4 proof toggles with expected symptoms

─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
 Ready to code?

 Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌
 Day1 Implementation Plan: Instanced vs Naive (10k)

 0) Integration Option Chosen: B (Composition)

 Rationale: Create focused modules in Renderer/DX12/ without touching .sln. Balances extensibility with minimal risk.

 vcxproj changes required NOW (S0):
 - Add d3dcompiler.lib to Debug|x64 AdditionalDependencies
 - Add d3d12.lib;dxgi.lib;dxguid.lib;d3dcompiler.lib to Release|x64 AdditionalDependencies

 ---
 1) Scope

 In Scope:
 - 10k cube instances in 100x100 grid
 - Runtime toggle: Instanced (1 draw) vs Naive (10k draws)
 - Per-frame resources: allocator, upload buffer, transforms buffer, SRV slot
 - Fence-gated reuse (no descriptor/resource stomp)
 - CPU timing log: mode, draw_calls, cpu_record_ms
 - Proof toggles: stomp_Lifetime, break_RPIndexSwap, sentinel_Instance0

 Out of Scope:
 - GPU timestamps (seam only)
 - Depth buffer
 - Texture arrays
 - Resize handling
 - Multiple pipelines

 Design-Ready Hooks:
 - UploadArena interface for future suballocation
 - ResourceStateTracker seam (explicit barriers for now)
 - Query heap allocation for GPU timestamps

 ---
 2) Architecture

 Discrepancy Resolutions
 ┌─────┬──────────────────────────────────────────────────────────────┐
 │ ID  │                           Decision                           │
 ├─────┼──────────────────────────────────────────────────────────────┤
 │ D1  │ Use FrameCount = 3 (triple buffering)                        │
 ├─────┼──────────────────────────────────────────────────────────────┤
 │ D2  │ Use DATA_STATIC_WHILE_SET_AT_EXECUTE per contract            │
 ├─────┼──────────────────────────────────────────────────────────────┤
 │ D3  │ Track initial state; first frame skips SRV→COPY_DEST barrier │
 ├─────┼──────────────────────────────────────────────────────────────┤
 │ D4  │ IA/RS/OM set in Tick, not RecordDraw                         │
 └─────┴──────────────────────────────────────────────────────────────┘
 Module Ownership
 ┌──────────────────┬──────────────────────────────────┬──────────────────────────────────────────────────────┐
 │      Module      │             Location             │                         Owns                         │
 ├──────────────────┼──────────────────────────────────┼──────────────────────────────────────────────────────┤
 │ FrameContextRing │ Renderer/DX12/FrameContextRing.h │ Per-frame allocators, fence values, upload/SRV slots │
 ├──────────────────┼──────────────────────────────────┼──────────────────────────────────────────────────────┤
 │ ShaderLibrary    │ Renderer/DX12/ShaderLibrary.h    │ Root sig, PSO, embedded HLSL compilation             │
 ├──────────────────┼──────────────────────────────────┼──────────────────────────────────────────────────────┤
 │ RenderScene      │ Renderer/DX12/RenderScene.h      │ Cube VB/IB, transforms buffer, draw recording        │
 ├──────────────────┼──────────────────────────────────┼──────────────────────────────────────────────────────┤
 │ ToggleSystem     │ Renderer/DX12/ToggleSystem.h     │ Mode flags, diagnostic levers                        │
 └──────────────────┴──────────────────────────────────┴──────────────────────────────────────────────────────┘
 Key Policies

 - Frame ring: frameId % 3, NOT GetCurrentBackBufferIndex()
 - Descriptors: Per-frame SRV slot (slot = frameIndex)
 - Barriers: SRV → COPY_DEST → Copy → COPY_DEST → SRV (skip first transition on frame 0)
 - Root ABI: RP_FrameCB=0 (b0), RP_TransformsTable=1 (t0 table)

 ---
 3) Slice Plan

 S0: Fix Build + Verify Green

 Objective: Release|x64 links successfully
 Files: DX12EngineLab.vcxproj
 Changes:
 - Line ~109: Add d3dcompiler.lib to Debug|x64
 - Line ~122-125: Add d3d12.lib;dxgi.lib;dxguid.lib;d3dcompiler.lib to Release|x64
 Acceptance: msbuild /p:Configuration=Release /p:Platform=x64 succeeds
 Build-green: Debug|x64 + Release|x64 both compile and link

 S1: FrameContextRing + Triple Buffering

 Objective: Per-frame allocators with fence-gated reuse
 Files:
 - Renderer/DX12/FrameContextRing.h (new)
 - Renderer/DX12/Dx12Context.h (update FrameCount=3, add FrameContextRing member)
 - Renderer/DX12/Dx12Context.cpp (init ring, use in Render)
 Acceptance: Window opens, clears to color, no validation errors, log shows frameId=0,1,2 cycling
 Build-green: Both configs compile and link

 S2: ShaderLibrary + Root Sig + PSO

 Objective: Embedded HLSL compiles, PSO created
 Files:
 - Renderer/DX12/ShaderLibrary.h (new)
 - Renderer/DX12/ShaderLibrary.cpp (new)
 - Renderer/DX12/Dx12Context.cpp (init ShaderLibrary)
 Acceptance: No D3DCompile errors, PSO valid, log shows "PSO created"
 Build-green: Both configs compile and link

 S3: Cube Geometry (VB/IB)

 Objective: Static cube mesh in DEFAULT heap
 Files:
 - Renderer/DX12/RenderScene.h (new)
 - Renderer/DX12/RenderScene.cpp (new)
 - Renderer/DX12/Dx12Context.cpp (init geometry)
 Acceptance: Single white cube renders at origin
 Build-green: Both configs compile and link

 S4: Transforms Buffer + SRV + 10k Grid

 Objective: Per-frame upload→default copy, SRV binding, 100x100 grid visible
 Files:
 - Renderer/DX12/RenderScene.cpp (add transforms, per-frame buffers, SRV creation)
 - Renderer/DX12/FrameContextRing.h (add upload buffer, SRV slot tracking)
 Acceptance: 10k cubes visible in grid pattern, instance 0 sentinel works
 Build-green: Both configs compile and link

 S5: ToggleSystem + Naive vs Instanced

 Objective: Runtime toggle between 1 draw and 10k draws
 Files:
 - Renderer/DX12/ToggleSystem.h (new)
 - Renderer/DX12/RenderScene.cpp (conditional draw loop)
 - DX12EngineLab.cpp (key input to toggle)
 Acceptance: Press key → mode switches, log shows mode=instanced draws=1 vs mode=naive draws=10000
 Build-green: Both configs compile and link

 S6: Timing + Evidence Log

 Objective: CPU record timing, structured log output
 Files:
 - Renderer/DX12/Dx12Context.cpp (timing around record, log output)
 Acceptance: Console shows mode=instanced draws=1 cpu_record_ms=X.XXX each frame
 Build-green: Both configs compile and link

 S7: Proof Toggles (Microtests)

 Objective: Diagnostic levers to prove correctness
 Files:
 - Renderer/DX12/ToggleSystem.h (add stomp_Lifetime, break_RPIndexSwap, sentinel flags)
 - Renderer/DX12/RenderScene.cpp (conditional broken paths)
 Acceptance: Each toggle produces expected failure (flicker, wrong binding, etc.)
 Build-green: Both configs compile and link

 ---
 4) Evidence Plan

 Log Schema

 mode=<instanced|naive> draws=<1|10000> cpu_record_ms=<float> frameId=<0-2>

 Screenshot Naming

 - captures/day1_instanced_10k.png
 - captures/day1_naive_10k.png
 - captures/day1_sentinel_proof.png

 Debug Validation

 - DX12 debug layer ON in Debug builds
 - Zero validation errors in happy path
 - GetDeviceRemovedReason() logged on failure

 ---
 5) Contract Patch Suggestions

 --- docs/contracts/Day1_InstancingVsNaive.md
 +++ docs/contracts/Day1_InstancingVsNaive.md
 @@ -106,7 +106,9 @@ Named pipeline lookup
  **Critical correction**
  - Descriptor range flags: do **NOT** claim `DATA_STATIC` if underlying data changes across frames.
    - Use **`DATA_STATIC_WHILE_SET_AT_EXECUTE`** (or equivalent safe default) because per-frame data is stable only during GPU execution of that command list.
 +- Initial resource state: transforms DEFAULT buffer should be created in `COPY_DEST` state.
 +  - First frame: skip SRV→COPY_DEST barrier (buffer already in COPY_DEST).
 +  - Subsequent frames: full barrier cycle SRV→COPY_DEST→copy→COPY_DEST→SRV.

 ---
 6) Risk Matrix
 ┌────────────────────────┬─────────────────────────┬─────────────────────────────────┐
 │          Risk          │      Proof Toggle       │        Expected Symptom         │
 ├────────────────────────┼─────────────────────────┼─────────────────────────────────┤
 │ Frame-ring ≠ swapchain │ stomp_Lifetime=true     │ Flicker/garbage                 │
 ├────────────────────────┼─────────────────────────┼─────────────────────────────────┤
 │ Descriptor stomp       │ break_OmitSetHeaps=true │ GPU reads garbage               │
 ├────────────────────────┼─────────────────────────┼─────────────────────────────────┤
 │ Root param mismatch    │ break_RPIndexSwap=true  │ Wrong data bound                │
 ├────────────────────────┼─────────────────────────┼─────────────────────────────────┤
 │ Instance ID wrong      │ sentinel_Instance0=true │ Instance 0 not at (9999,0,9999) │
 └────────────────────────┴─────────────────────────┴─────────────────────────────────┘
 ---
 7) Output Files to Create

 After approval, create:
 1. docs/contracts/Day1_InstancingVsNaive_PLAN.md - This plan content
 2. prompts/Day1_IMPLEMENT.md - Implementation prompt with guardrails

 ---
 Verification

 1. After each slice: msbuild DX12EngineLab.sln /p:Configuration=Debug /p:Platform=x64
 2. After each slice: msbuild DX12EngineLab.sln /p:Configuration=Release /p:Platform=x64
 3. Run exe, verify acceptance criteria
 4. Check debug output for validation errors
 5. Final: screenshot captures for evidence
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌

 Would you like to proceed?

 ❯ 1. Yes, clear context and auto-accept edits (shift+tab)
   2. Yes, auto-accept edits
   3. Yes, manually approve edits
   4. Type here to tell Claude what to change

 ctrl-g to edit in VS Code · ~/.claude/plans/scalable-crunching-honey.md