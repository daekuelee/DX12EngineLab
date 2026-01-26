PLAN — Architecture & Refactor Roadmap (DX12EngineLab)

You are a senior C++/DirectX12 engine engineer. Inspect this repository and produce a refactor plan that improves:
- Encapsulation (smaller functions, clear ownership)
- Composition over inheritance
- Testability/debuggability (assertions, proof hooks)
- Maintainability (separation of concerns, fewer “god” files)
WITHOUT changing runtime behavior/output.

Context:
- This codebase is fast-evolving with AI assistance, and code growth outpaces human reading speed.
- The refactor must be staged into small, reviewable PRs with clear verification steps.
- Keep performance neutral or better; avoid new per-frame allocations.
- No new third-party dependencies. C++17 is allowed.

Deliverables (write these files):
1) docs/refactor_plan.md
   Include:
   - Current architecture map: modules/classes and responsibilities (with file references)
   - Top 10 pain points: long functions, mixed responsibilities, hidden invariants (include function names + why)
   - Target architecture proposal using composition:
     * FrameOrchestrator (frame stages)
     * ResourceRegistry (resource + view ownership)
     * UploadArena/FrameUploadSlice (explicit per-frame upload slice)
     * ResourceStateTracker (single entry-point for transitions)
     * Render passes as composable units (no giant monolithic RecordCommands)
   - Concrete invariants/contracts for each module (MUST/SHOULD)
   - A staged PR roadmap (PR#1..PR#N), each shippable, each limited in scope:
     * What changes
     * Which files touched
     * How to verify (debug layer, HUD metrics, output equivalence)

2) docs/refactor_pr_checklist.md
   A universal checklist to run for every refactor PR:
   - Build configs
   - Debug layer expectations
   - HUD metrics sanity (alloc/map/barrier counts)
   - Rendering output equivalence checks
   - Common regressions to watch for

3) A table of the 5 longest functions in the repo:
   - file path
   - function name
   - current responsibilities (bullet list)
   - proposed split into smaller functions (suggested names + signatures)
   - what data each sub-function consumes/returns

Important constraints:
- Do NOT implement code changes in this PLAN step.
- Do NOT do big-bang rewrites.
- Prefer composition and small components with explicit APIs.

----------------------------------------------






 Updated plan
  ⎿  /plan to preview
      389  - [ ] No references to `FrameContext::transformsState` remain (grep returns 0)                                                                                     390  - [ ] Tracker validates state mismatch detection (optional: add debug test)
      391  - [ ] UAV barriers work for any compute resources (verify in future compute PRs)                                                                                   392 +- [ ] (Future compute PRs) Verify missing UAVBarrier after compute write reproduces visible corruption/flicker; adding UAVBarrier fixes it
      393                                                                                                                                                                     394  ### Barrier Verification
      395  - [ ] PIX GPU capture shows expected barrier counts

● All spec clarifications have been applied. Here's the summary:

  Diff-Style Summary of Changes                                                                                                                                         
  Section: "RenderContext Struct (Parameter Bundling)" (line ~144)                                                                                                      
  Added:
  + **Growth Control Guidance:** `RenderContext` should contain per-frame/per-pass
  + parameters only. Long-lived globals (device, queues, registries) should stay
  + in a `RenderResources`/`DeviceContext` struct and be accessed via reference
  + or pointer. Avoid turning `RenderContext` into a "god struct."

  Section: "Module Contracts → ResourceStateTracker" (line ~148)

  Added:
  + **Important:** `UAVBarrier` is NOT a state transition; it is an ordering
  + constraint. A UAV barrier may be required even when the resource state
  + does not change (e.g., back-to-back compute dispatches writing to the same buffer).
  Modified:
  - MUST provide `UAVBarrier(resource)` for UAV synchronization (compute paths, counters, indirect args)
  + MUST provide `UAVBarrier(resource)` for compute paths (counters, indirect args, back-to-back dispatches)

  NEW Section: "Backbuffer Ownership vs. State Tracking" (after API Surface)

  Added:
  + | Component | Backbuffer Handling |
  + |-----------|---------------------|
  + | Dx12Context | Owns backbuffers via ComPtr (lifetime) |
  + | ResourceRegistry | Does NOT register backbuffers (ownership external) |
  + | ResourceStateTracker | DOES track via AssumeState(PRESENT) |
  +
  + Note: External resources can be tracked via AssumeState(), but ownership remains external.

  Section: "Verification Checklist → ResourceStateTracker-Specific"

  Added:
  + - [ ] (Future compute PRs) Verify missing UAVBarrier after compute write
  +       reproduces visible corruption/flicker; adding UAVBarrier fixes it

  ---
  Invariants Confirmation

  All plan invariants remain intact:
  ┌────────────────────────────────────────────────────────┬───────────┐
  │                       Invariant                        │  Status   │
  ├────────────────────────────────────────────────────────┼───────────┤
  │ After PR#10: grep transformsState returns 0 matches    │ Unchanged │
  ├────────────────────────────────────────────────────────┼───────────┤
  │ ResourceStateTracker is SOLE writer of resource states │ Unchanged │
  ├────────────────────────────────────────────────────────┼───────────┤
  │ ResourceRegistry stores ownership only, NOT state      │ Unchanged │
  ├────────────────────────────────────────────────────────┼───────────┤
  │ All passes receive const RenderContext&                │ Unchanged │
  ├────────────────────────────────────────────────────────┼───────────┤
  │ PR roadmap order (PR#1-12)                             │ Unchanged │
  ├────────────────────────────────────────────────────────┼───────────┤
  │ ~200 lines per PR target                               │ Unchanged │
  └────────────────────────────────────────────────────────┴───────────┘

──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                         Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ DX12EngineLab Refactor Plan

 Overview

 This plan transforms the DX12EngineLab from a monolithic architecture into a composition-based design with clear ownership, testable components, and explicit
 contracts. Changes are staged into small, reviewable PRs that preserve runtime behavior.

 Constraints:
 - No runtime behavior changes
 - No new per-frame allocations
 - No new third-party dependencies
 - C++17 allowed
 - Each PR targets ~200 lines changed (proof/HUD instrumentation tends to expand diffs)

 ---
 Current Architecture Map

 Core Files
 ┌─────────────────────────────────────────────┬──────────┬─────────────────────────────────────────────────────────────────────────────┐
 │                    File                     │   Size   │                               Responsibility                                │
 ├─────────────────────────────────────────────┼──────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.h/cpp             │ 99/821   │ Main orchestrator - device, swap chain, frame loop, all subsystem ownership │
 ├─────────────────────────────────────────────┼──────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/FrameContextRing.h/cpp        │ 84/229   │ Triple-buffered per-frame resources, fence synchronization                  │
 ├─────────────────────────────────────────────┼──────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/FrameLinearAllocator.h/cpp    │ 36/116   │ Bump-pointer upload allocator                                               │
 ├─────────────────────────────────────────────┼──────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/DescriptorRingAllocator.h/cpp │ 121/327  │ Shader-visible descriptor ring with fence-protected retirement              │
 ├─────────────────────────────────────────────┼──────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ResourceRegistry.h/cpp        │ 183/252  │ Handle-based resource ownership (infrastructure, not actively used)         │
 ├─────────────────────────────────────────────┼──────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.h/cpp           │ 72/360   │ Root signatures, shader loading, PSO creation                               │
 ├─────────────────────────────────────────────┼──────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/PSOCache.h/cpp                │ 149/~200 │ Hash-based PSO caching                                                      │
 ├─────────────────────────────────────────────┼──────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/RenderScene.h/cpp             │ 73/466   │ Geometry buffers and draw recording                                         │
 ├─────────────────────────────────────────────┼──────────┼─────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ImGuiLayer.h/cpp              │ 35/223   │ HUD integration                                                             │
 └─────────────────────────────────────────────┴──────────┴─────────────────────────────────────────────────────────────────────────────┘
 Ownership Hierarchy

 Dx12Context (owns everything)
   ├── Device, CommandQueue, SwapChain (ComPtr)
   ├── RTV/DSV Heaps (ComPtr)
   ├── DescriptorRingAllocator (member)
   ├── FrameContextRing (member)
   │   ├── Fence (ComPtr)
   │   ├── CommandAllocators[3] (ComPtr)
   │   ├── FrameLinearAllocators[3] (member)
   │   └── TransformsDefault[3] (ComPtr)
   ├── ResourceRegistry (member, unused)
   ├── ShaderLibrary (member)
   │   └── PSOCache (member)
   ├── RenderScene (member)
   └── ImGuiLayer (member)

 ---
 Top 10 Pain Points
 ┌─────┬──────────────────────────────────┬──────────────────────────────────────────────────────────────┬────────────────────────────────────────────────────────────┐
 │  #  │              Issue               │                           Location                           │                     Why It's a Problem                     │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 1   │ Monolithic Initialize()          │ Dx12Context.cpp:104-365                                      │ 262 lines, 14+ sequential subsystem inits, no cleanup on   │
 │     │                                  │                                                              │ partial failure                                            │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 2   │ Mixed state tracking             │ FrameContext::transformsState,                               │ No single source of truth; hard to audit barrier           │
 │     │                                  │ ResourceRegistry::Entry::state, inline barriers              │ correctness                                                │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 3   │ RecordPasses() does everything   │ Dx12Context.cpp:485-633                                      │ 149 lines: barriers, state setup, all draws, transitions   │
 │     │                                  │                                                              │ mixed together                                             │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 4   │ ResourceRegistry unused          │ ResourceRegistry.h/cpp                                       │ Infrastructure exists but not used; transforms buffer      │
 │     │                                  │                                                              │ tracked separately                                         │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 5   │ Code duplication in geometry     │ RenderScene.cpp                                              │ CreateCubeGeometry, CreateFloorGeometry,                   │
 │     │                                  │                                                              │ CreateMarkerGeometry are ~90% identical                    │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 6   │ Camera state is global static    │ Dx12Context.cpp:32-35                                        │ s_camera, s_lastTime as file-level statics; not            │
 │     │                                  │                                                              │ encapsulated                                               │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 7   │ Diagnostic logging scattered     │ Dx12Context.cpp (5+ locations)                               │ Throttled logging mixed throughout rendering code          │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 8   │ Backbuffer state implicit        │ RecordPasses()                                               │ No explicit tracking; relies on Present() leaving          │
 │     │                                  │                                                              │ resources in PRESENT state                                 │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 9   │ No RenderPass abstraction        │ RecordPasses()                                               │ All passes inline; hard to reorder, profile, or            │
 │     │                                  │                                                              │ selectively disable                                        │
 ├─────┼──────────────────────────────────┼──────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤
 │ 10  │ ShaderLibrary::CreatePSO()       │ ShaderLibrary.cpp:213-359                                    │ 147 lines: input layout + rasterizer + blend + 3 PSOs +    │
 │     │ complexity                       │                                                              │ marker root sig                                            │
 └─────┴──────────────────────────────────┴──────────────────────────────────────────────────────────────┴────────────────────────────────────────────────────────────┘
 ---
 Single Source of Truth for Resource State

 Problem: State is currently tracked in multiple places:
 - FrameContext::transformsState - per-frame state for transforms buffer
 - ResourceRegistry::Entry::state - per-resource state slot (unused)
 - Inline barriers - implicit state for backbuffers, depth buffer

 Solution - Ownership Split:
 ┌──────────────────────┬───────────────────────────────────────┬──────────────────────────────────────────────┐
 │      Component       │            Responsibility             │                State Tracking                │
 ├──────────────────────┼───────────────────────────────────────┼──────────────────────────────────────────────┤
 │ ResourceRegistry     │ Resource ownership, handles, lifetime │ NONE (remove Entry::state or make read-only) │
 ├──────────────────────┼───────────────────────────────────────┼──────────────────────────────────────────────┤
 │ ResourceStateTracker │ State truth + barrier emission        │ SOLE writer of resource states               │
 └──────────────────────┴───────────────────────────────────────┴──────────────────────────────────────────────┘
 Migration Plan:
 1. PR#9: Migrate transforms to ResourceRegistry (handles only, state remains in FrameContext temporarily)
 2. PR#10: Introduce ResourceStateTracker, register all tracked resources, delete FrameContext::transformsState
 3. PR#10: Register backbuffers via AssumeState() API for external/swapchain resources

 Invariant: After PR#10, there is exactly ONE place to query or modify resource state: ResourceStateTracker.

 ---
 Target Architecture (Composition-Based)

 New Components

 Dx12Context (orchestrator only)
   ├── RenderResources (struct holding device, queue, swap chain)
   ├── FrameOrchestrator (owns frame stages)
   │   ├── DescriptorRingAllocator
   │   ├── FrameContextRing
   │   └── ResourceStateTracker (new) ← SOLE state authority
   ├── PassOrchestrator (sequences render passes)
   │   ├── ClearPass (new)
   │   ├── GeometryPass (new)
   │   └── ImGuiPass (new)
   ├── GeometryFactory (new, shared buffer creation)
   ├── ShaderLibrary (refactored)
   └── RenderScene

 RenderContext Struct (Parameter Bundling)

 To prevent parameter explosion in pass extraction, introduce a context bundle:

 // Renderer/DX12/RenderContext.h
 struct RenderContext
 {
     ID3D12GraphicsCommandList* cmd;
     ID3D12Device* device;

     // Frame resources
     FrameContext& frame;
     DescriptorRingAllocator& descRing;

     // Rendering resources
     ShaderLibrary& shaders;
     RenderScene& scene;

     // Views
     D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
     D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
     D3D12_VIEWPORT viewport;
     D3D12_RECT scissor;

     // Frame data
     D3D12_GPU_VIRTUAL_ADDRESS frameCBAddress;
     D3D12_GPU_DESCRIPTOR_HANDLE srvTableHandle;
 };

 Passes receive const RenderContext& plus pass-specific inputs only.

 Growth Control Guidance: RenderContext should contain per-frame/per-pass parameters only. Long-lived globals (device, queues, registries) should stay in a
 RenderResources/DeviceContext struct and be accessed via reference or pointer. Avoid turning RenderContext into a "god struct."

 Module Contracts

 ResourceStateTracker

 Scope: Whole-resource state tracking only (no per-subresource tracking). Subresource tracking is future work.

 Important: UAVBarrier is NOT a state transition; it is an ordering constraint. A UAV barrier may be required even when the resource state does not change (e.g.,
 back-to-back compute dispatches writing to the same buffer).

 - MUST track state for every registered resource
 - MUST validate StateBefore matches tracked state before emitting barrier
 - MUST hard-fail (debugbreak) on state mismatch in Debug builds
 - MUST provide Register(resource, initialState) for owned resources
 - MUST provide AssumeState(resource, state) for external resources (swapchain backbuffers)
 - MUST provide UAVBarrier(resource) for compute paths (counters, indirect args, back-to-back dispatches)
 - SHOULD batch multiple barriers into single ResourceBarrier call
 - SHOULD log transitions when diagnostics enabled

 API Surface:
 class ResourceStateTracker
 {
 public:
     // Registration
     void Register(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, const char* debugName = nullptr);
     void AssumeState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);  // For external resources
     void Unregister(ID3D12Resource* resource);

     // Transitions
     void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES targetState);
     void UAVBarrier(ID3D12Resource* resource);  // For compute synchronization

     // Barrier emission
     void FlushBarriers(ID3D12GraphicsCommandList* cmdList);  // Emit batched barriers

     // Query
     D3D12_RESOURCE_STATES GetState(ID3D12Resource* resource) const;
 };

 Backbuffer Ownership vs. State Tracking

 Swapchain backbuffers have special handling because they are externally managed by DXGI:
 ┌──────────────────────┬───────────────────────────────────────────────────────────────────────────┐
 │      Component       │                            Backbuffer Handling                            │
 ├──────────────────────┼───────────────────────────────────────────────────────────────────────────┤
 │ Dx12Context          │ Owns backbuffers via ComPtr<ID3D12Resource> (lifetime)                    │
 ├──────────────────────┼───────────────────────────────────────────────────────────────────────────┤
 │ ResourceRegistry     │ Does NOT register backbuffers (ownership external to registry)            │
 ├──────────────────────┼───────────────────────────────────────────────────────────────────────────┤
 │ ResourceStateTracker │ DOES track backbuffers via AssumeState(PRESENT) after swap chain creation │
 └──────────────────────┴───────────────────────────────────────────────────────────────────────────┘
 Note: External resources (created outside ResourceRegistry) can still be tracked via AssumeState(), but ownership remains external. The tracker only manages state
 truth, not lifetime.

 PassOrchestrator
 - MUST execute passes in fixed order: Clear -> Geometry -> UI
 - MUST manage backbuffer barrier scope (PRESENT <-> RENDER_TARGET)
 - SHOULD allow pass enable/disable for debugging

 GeometryFactory
 - MUST create buffers in DEFAULT heap
 - MUST upload via temporary UPLOAD buffer with fence wait
 - MUST transition to correct final state after upload

 ---
 Staged PR Roadmap

 PR#1: Add RenderContext + Extract ClearPass Helper (~150 lines)

 Files: RenderContext.h (new), ClearPass.h (new), Dx12Context.cpp
 Changes:
 - Define RenderContext struct to bundle common parameters
 - Move RT/depth clear into ClearPass::Record(const RenderContext& ctx)
 - ClearPass uses ctx.rtvHandle, ctx.dsvHandle from context

 Verify: Debug layer 0 errors, clear color unchanged, FPS stable

 PR#2: Extract GeometryPass Helper (~180 lines)

 Files: GeometryPass.h (new), Dx12Context.cpp
 Changes:
 - Move floor/cube draw logic into GeometryPass::Record(const RenderContext& ctx, const GeometryPassInputs& inputs)
 - GeometryPassInputs: draw mode, instance count, instance offset
 - Uses ctx.shaders, ctx.scene from context

 Verify: 10k cubes render in both modes, draw toggle works

 PR#3: Extract ImGuiPass Helper (~100 lines)

 Files: ImGuiPass.h (new), Dx12Context.cpp
 Changes:
 - Encapsulate ImGui render in ImGuiPass::Record(const RenderContext& ctx, ImGuiLayer& layer)
 - Minimal pass-specific inputs (just the layer reference)

 Verify: HUD displays correctly

 PR#4: Add BarrierScope RAII Helper (~80 lines)

 Files: BarrierScope.h (new), Dx12Context.cpp
 Changes: RAII helper for symmetric PRESENT<->RT barriers
 Verify: Debug layer 0 barrier validation errors

 PR#5: Split Initialize() - Device/SwapChain (~150 lines)

 Files: Dx12Context.h, Dx12Context.cpp
 Changes: Extract InitDevice(), InitSwapChain(), InitHeaps() private methods
 Verify: Debug layer 0 errors on startup

 PR#6: Split Initialize() - Subsystems (~120 lines)

 Files: Dx12Context.h, Dx12Context.cpp
 Changes: Extract InitFrameResources(), InitShaders(), InitScene(), InitImGui()
 Verify: All "initialized successfully" messages present

 PR#7: Extract FreeCamera to Member (~100 lines)

 Files: Dx12Context.h, Dx12Context.cpp
 Changes: Move FreeCamera struct and s_camera from statics to member
 Verify: Camera controls work identically (WASD, Q/E, Space/Ctrl)

 PR#8: Extract DiagnosticLogger (~120 lines)

 Files: DiagnosticLogger.h (new), Dx12Context.cpp
 Changes: Centralize throttled logging into DiagnosticLogger::LogThrottled()
 Verify: Same proof logs at 1/sec rate

 PR#9: Migrate Transforms to ResourceRegistry (~150 lines)

 Files: FrameContextRing.h/cpp, Dx12Context.cpp
 Changes:
 - Store ResourceHandle instead of ComPtr<ID3D12Resource> for transforms
 - Use registry for ownership (state tracking remains in FrameContext::transformsState temporarily)
 - Registry used for handle-based access only in this PR

 Verify: 10k cubes render, "ResourceRegistry Created" logs appear

 PR#10: Add ResourceStateTracker + Remove Parallel State (~200 lines)

 Files: ResourceStateTracker.h/cpp (new), FrameContextRing.h/cpp, Dx12Context.cpp
 Changes:
 - Implement ResourceStateTracker with Register/AssumeState/Transition/UAVBarrier APIs
 - Register transforms buffers via Register()
 - Register backbuffers via AssumeState() (initial state PRESENT after creation)
 - DELETE FrameContext::transformsState - tracker is now sole authority
 - If ResourceRegistry::Entry::state exists, mark as deprecated or remove
 - Update all barrier emission to go through tracker

 Verify:
 - Debug layer 0 errors
 - Tracker logs transitions
 - No compilation references to transformsState

 Critical Invariant: After this PR, grep for transformsState returns 0 matches.

 PR#11: Extract GeometryFactory (~180 lines)

 Files: GeometryFactory.h/cpp (new), RenderScene.cpp
 Changes: Consolidate buffer creation into factory, deduplicate upload logic
 Verify: All geometry renders correctly

 PR#12: Add PassOrchestrator (~150 lines)

 Files: PassOrchestrator.h/cpp (new), Dx12Context.cpp
 Changes: Orchestrator sequences ClearPass, GeometryPass, ImGuiPass
 Verify: Full scene renders correctly

 ---
 5 Longest Functions - Proposed Splits

 1. Dx12Context::Initialize() (262 lines)
 ┌─────────────────────┬──────────────────────────┬───────────────────────────────────┬────────────────────────────┐
 │    Sub-function     │        Signature         │             Consumes              │          Returns           │
 ├─────────────────────┼──────────────────────────┼───────────────────────────────────┼────────────────────────────┤
 │ InitDebugLayer()    │ void InitDebugLayer()    │ -                                 │ -                          │
 ├─────────────────────┼──────────────────────────┼───────────────────────────────────┼────────────────────────────┤
 │ InitFactory()       │ bool InitFactory()       │ -                                 │ m_factory                  │
 ├─────────────────────┼──────────────────────────┼───────────────────────────────────┼────────────────────────────┤
 │ InitDevice()        │ bool InitDevice()        │ m_factory                         │ m_adapter, m_device        │
 ├─────────────────────┼──────────────────────────┼───────────────────────────────────┼────────────────────────────┤
 │ InitCommandQueue()  │ bool InitCommandQueue()  │ m_device                          │ m_commandQueue             │
 ├─────────────────────┼──────────────────────────┼───────────────────────────────────┼────────────────────────────┤
 │ InitSwapChain()     │ bool InitSwapChain()     │ m_factory, m_commandQueue, m_hwnd │ m_swapChain, m_backBuffers │
 ├─────────────────────┼──────────────────────────┼───────────────────────────────────┼────────────────────────────┤
 │ InitRenderTargets() │ bool InitRenderTargets() │ m_device, m_swapChain             │ m_rtvHeap                  │
 ├─────────────────────┼──────────────────────────┼───────────────────────────────────┼────────────────────────────┤
 │ InitDepthBuffer()   │ bool InitDepthBuffer()   │ m_device                          │ m_dsvHeap, m_depthBuffer   │
 ├─────────────────────┼──────────────────────────┼───────────────────────────────────┼────────────────────────────┤
 │ InitSubsystems()    │ bool InitSubsystems()    │ m_device                          │ all subsystems             │
 └─────────────────────┴──────────────────────────┴───────────────────────────────────┴────────────────────────────┘
 2. Dx12Context::RecordPasses() (149 lines)
 ┌────────────────────────┬─────────────────────────────────────────────────────────────────────┬──────────────────────────┐
 │       Component        │                              Signature                              │      Responsibility      │
 ├────────────────────────┼─────────────────────────────────────────────────────────────────────┼──────────────────────────┤
 │ BarrierScope           │ RAII struct                                                         │ PRESENT<->RT transitions │
 ├────────────────────────┼─────────────────────────────────────────────────────────────────────┼──────────────────────────┤
 │ ClearPass::Record()    │ void Record(const RenderContext& ctx)                               │ Clear RT and depth       │
 ├────────────────────────┼─────────────────────────────────────────────────────────────────────┼──────────────────────────┤
 │ GeometryPass::Record() │ void Record(const RenderContext& ctx, const GeometryPassInputs& in) │ Draw floor/cubes         │
 ├────────────────────────┼─────────────────────────────────────────────────────────────────────┼──────────────────────────┤
 │ ImGuiPass::Record()    │ void Record(const RenderContext& ctx, ImGuiLayer& layer)            │ Draw HUD                 │
 └────────────────────────┴─────────────────────────────────────────────────────────────────────┴──────────────────────────┘
 3. ShaderLibrary::CreatePSO() (147 lines)
 ┌──────────────────────────┬─────────────────────────────────────────────────────────┬─────────────────┐
 │       Sub-function       │                        Signature                        │     Returns     │
 ├──────────────────────────┼─────────────────────────────────────────────────────────┼─────────────────┤
 │ GetDefaultRasterizer()   │ D3D12_RASTERIZER_DESC GetDefaultRasterizer()            │ Rasterizer desc │
 ├──────────────────────────┼─────────────────────────────────────────────────────────┼─────────────────┤
 │ GetDefaultBlend()        │ D3D12_BLEND_DESC GetDefaultBlend()                      │ Blend desc      │
 ├──────────────────────────┼─────────────────────────────────────────────────────────┼─────────────────┤
 │ GetDefaultDepthStencil() │ D3D12_DEPTH_STENCIL_DESC GetDefaultDepthStencil()       │ Depth desc      │
 ├──────────────────────────┼─────────────────────────────────────────────────────────┼─────────────────┤
 │ CreateCubePSO()          │ ID3D12PipelineState* CreateCubePSO(rtvFormat)           │ PSO             │
 ├──────────────────────────┼─────────────────────────────────────────────────────────┼─────────────────┤
 │ CreateFloorPSO()         │ ID3D12PipelineState* CreateFloorPSO(rtvFormat)          │ PSO             │
 ├──────────────────────────┼─────────────────────────────────────────────────────────┼─────────────────┤
 │ CreateMarkerPSO()        │ ID3D12PipelineState* CreateMarkerPSO(device, rtvFormat) │ Root sig + PSO  │
 └──────────────────────────┴─────────────────────────────────────────────────────────┴─────────────────┘
 4. Dx12Context::Render() (109 lines)
 ┌───────────────────┬──────────────────────────────────┬─────────────────────────────┐
 │   Sub-function    │            Signature             │       Responsibility        │
 ├───────────────────┼──────────────────────────────────┼─────────────────────────────┤
 │ PreFrame()        │ float PreFrame()                 │ Delta time, camera update   │
 ├───────────────────┼──────────────────────────────────┼─────────────────────────────┤
 │ BeginGPUFrame()   │ FrameContext& BeginGPUFrame()    │ Fence wait, allocator reset │
 ├───────────────────┼──────────────────────────────────┼─────────────────────────────┤
 │ UploadFrameData() │ FrameData UploadFrameData(ctx)   │ CB + transforms allocations │
 ├───────────────────┼──────────────────────────────────┼─────────────────────────────┤
 │ RecordFrame()     │ void RecordFrame(ctx, frameData) │ Command recording           │
 ├───────────────────┼──────────────────────────────────┼─────────────────────────────┤
 │ SubmitFrame()     │ void SubmitFrame(ctx)            │ Execute, present            │
 ├───────────────────┼──────────────────────────────────┼─────────────────────────────┤
 │ PostFrame()       │ void PostFrame()                 │ Increment frame counter     │
 └───────────────────┴──────────────────────────────────┴─────────────────────────────┘
 5. DescriptorRingAllocator::Allocate() (107 lines)
 ┌───────────────────────────┬─────────────────────────────────────────────────────┬────────────────────┐
 │       Sub-function        │                      Signature                      │   Responsibility   │
 ├───────────────────────────┼─────────────────────────────────────────────────────┼────────────────────┤
 │ ValidateCapacity()        │ bool ValidateCapacity(count, tag)                   │ Check space exists │
 ├───────────────────────────┼─────────────────────────────────────────────────────┼────────────────────┤
 │ CalculateContiguousFree() │ uint32_t CalculateContiguousFree()                  │ Head/tail math     │
 ├───────────────────────────┼─────────────────────────────────────────────────────┼────────────────────┤
 │ TryWrap()                 │ bool TryWrap(count)                                 │ Handle wrap-around │
 ├───────────────────────────┼─────────────────────────────────────────────────────┼────────────────────┤
 │ AllocateContiguous()      │ DescriptorAllocation AllocateContiguous(count, tag) │ Actual allocation  │
 └───────────────────────────┴─────────────────────────────────────────────────────┴────────────────────┘
 ---
 Verification Checklist

 For every refactor PR:

 Build

 - VS2022 x64 Debug builds clean (0 errors, 0 warnings)
 - VS2022 x64 Release builds clean

 Runtime

 - Debug layer reports 0 errors
 - Debug layer reports 0 warnings (or only known/documented ones)
 - Application starts and renders without crash
 - Application shuts down cleanly (no leaks)

 Visual Equivalence

 - Clear color is sky blue (#87CEEB)
 - 10k cubes visible in default view
 - Floor visible below cubes
 - HUD displays FPS and mode info
 - Draw mode toggle (T) works
 - Color mode cycle (C) works
 - Camera controls (WASD/Q/E/Space/Ctrl) work

 Performance

 - FPS within +/- 5% of baseline
 - No new per-frame allocations

 ResourceStateTracker-Specific (PR#10+)

 - All tracked resources registered before first use
 - Backbuffers registered via AssumeState(PRESENT) after swap chain creation
 - No references to FrameContext::transformsState remain (grep returns 0)
 - Tracker validates state mismatch detection (optional: add debug test)
 - UAV barriers work for any compute resources (verify in future compute PRs)
 - (Future compute PRs) Verify missing UAVBarrier after compute write reproduces visible corruption/flicker; adding UAVBarrier fixes it

 Barrier Verification

 - PIX GPU capture shows expected barrier counts
 - No redundant barriers (same resource, same state)
 - No missing barriers (debug layer catches these)
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌

