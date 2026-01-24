● Day1 Integration Analysis Summary

  ---
  A) Repo Map: Ownership
  ┌─────────────────────┬─────────────────────┬───────────────────────────────────┐                                                    │      Component      │        Owner        │             Location              │
  ├─────────────────────┼─────────────────────┼───────────────────────────────────┤                                                    │ Window Loop         │ DX12EngineLab.cpp   │ WinMain @ line 25, loop @ line 52 │
  ├─────────────────────┼─────────────────────┼───────────────────────────────────┤
  │ Device/Queue        │ Dx12Context (stub)  │ Renderer/DX12/Dx12Context.h:34-35 │
  ├─────────────────────┼─────────────────────┼───────────────────────────────────┤
  │ Swapchain           │ Dx12Context (stub)  │ Renderer/DX12/Dx12Context.h:36    │
  ├─────────────────────┼─────────────────────┼───────────────────────────────────┤
  │ Per-frame Resources │ Dx12Context (stub)  │ cmd alloc/list @ line 45-46       │
  ├─────────────────────┼─────────────────────┼───────────────────────────────────┤
  │ Renderer            │ Engine::App::Tick() │ Engine/App.cpp:16 (empty)         │
  └─────────────────────┴─────────────────────┴───────────────────────────────────┘
  Structure:
  Engine/App.h/cpp        → app lifecycle (stub)
  Renderer/DX12/          → Dx12Context, Dx12Debug (skeleton)
  src/app/, src/gfx/      → placeholders (.gitkeep only)

  ---
  B) Entry Point

  - WinMain: DX12EngineLab.cpp:25
  - Frame Loop: DX12EngineLab.cpp:52 → calls g_app.Tick() @ line 72
  - Per-frame hook: Engine/App.cpp:16 App::Tick() — currently empty

  ---
  C) ex.cpp Status: NOT_BUILT

  Evidence: DX12EngineLab.vcxproj compiled sources:
  - DX12EngineLab.cpp ✓
  - Engine\App.cpp ✓
  - Renderer\DX12\Dx12Context.cpp ✓
  - Renderer\DX12\Dx12Debug.cpp ✓
  - ex.cpp ✗ (not listed)

  ex.cpp is a reference harness containing complete Day1 implementation (instancing vs naive toggle, frame ring, descriptors).

  ---
  D) Integration Options
  ┌───────────────┬──────────────────┬────────────────┬───────────────────┐
  │    Aspect     │    A: Minimal    │ B: Composition │ C: Full Refactor  │
  ├───────────────┼──────────────────┼────────────────┼───────────────────┤
  │ Files Changed │ Dx12Context only │ +5 new modules │ All + src/ reorg  │
  ├───────────────┼──────────────────┼────────────────┼───────────────────┤
  │ .vcxproj Edit │ None             │ None           │ Minimal (filters) │
  ├───────────────┼──────────────────┼────────────────┼───────────────────┤
  │ Risk          │ 2/5              │ 2/5            │ 3/5               │
  ├───────────────┼──────────────────┼────────────────┼───────────────────┤
  │ Extensibility │ 2/5              │ 4/5            │ 5/5               │
  ├───────────────┼──────────────────┼────────────────┼───────────────────┤
  │ Day2+ Ready   │ Constrained      │ Good           │ Excellent         │
  └───────────────┴──────────────────┴────────────────┴───────────────────┘
  Option A: Paste ex.cpp logic into Dx12Context::Render(). Fast but monolithic.

  Option B (Recommended): Create focused modules:
  - FrameContextRing.h/cpp – per-frame alloc/CB/SRV slots
  - ShaderLibrary.h/cpp – PSO + root sig
  - ResourceSystem.h/cpp – buffers, upload arena
  - RenderScene.h/cpp – geometry, transforms, draw list
  - ToggleSystem.h/cpp – mode/diagnostics

  Option C: Full src/app/ + src/gfx/ canonical layout. Architecture win, but more scope.

  ---
  E) Risks/Footguns
  ┌────────────────────────┬─────────────────┬──────────────────────────────────────────────────────────────────────────────────────┐
  │          Risk          │    Severity     │                                      Mitigation                                      │
  ├────────────────────────┼─────────────────┼──────────────────────────────────────────────────────────────────────────────────────┤
  │ Frame-ring ≠ swapchain │ HIGH            │ ex.cpp correct: frameId % kFrameCount for resources, GetCurrentBackBufferIndex() for │
  │  index                 │                 │  RTV. Contract explicit.                                                             │
  ├────────────────────────┼─────────────────┼──────────────────────────────────────────────────────────────────────────────────────┤
  │ Descriptor lifetime    │ HIGH            │ ex.cpp: per-frame SrvSlot. Copy pattern exactly.                                     │
  │ stomp                  │                 │                                                                                      │
  ├────────────────────────┼─────────────────┼──────────────────────────────────────────────────────────────────────────────────────┤
  │ Barrier ordering       │ MEDIUM          │ ex.cpp shows: SRV→COPY_DEST→COPY→SRV cycle. Replicate order.                         │
  ├────────────────────────┼─────────────────┼──────────────────────────────────────────────────────────────────────────────────────┤
  │ Root param ABI         │ MEDIUM          │ ex.cpp enum RP_FrameCB=0, RP_TransformsTable=1 matches HLSL. Verify bindings.        │
  │ mismatch               │                 │                                                                                      │
  ├────────────────────────┼─────────────────┼──────────────────────────────────────────────────────────────────────────────────────┤
  │ **Release              │ x64 missing     │ CRITICAL                                                                             │
  │                        │ libs**          │                                                                                      │
  ├────────────────────────┼─────────────────┼──────────────────────────────────────────────────────────────────────────────────────┤
  │ No shader build rules  │ HIGH            │ First .hlsl won't compile. Manual vcxproj add required.                              │
  └────────────────────────┴─────────────────┴──────────────────────────────────────────────────────────────────────────────────────┘
  ---
  F) Recommended Next Step

  Path: Option B (Clean Composition)

  1. Fix build bug first: Add d3d12.lib;dxgi.lib;dxguid.lib to Release|x64 in vcxproj
  2. Create 5 modules in Renderer/DX12/ (no vcxproj edit needed if added via VS IDE)
  3. Port ex.cpp logic into modules following its proven patterns
  4. Wire Dx12Context::Render() as: BeginFrame → UpdateScene → RecordDraw → EndFrame → Present
  5. Add shader build step when first .hlsl arrives

  Rationale: Low risk, no .sln edit, modular for Day2 extensions, ex.cpp is proven reference.