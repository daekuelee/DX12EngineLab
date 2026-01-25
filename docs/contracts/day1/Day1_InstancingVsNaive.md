# Day1 Contract — Instanced vs Naive (10,000)

## 1) Goal & Preconditions

### Goal
Build a DX12 sandbox that renders **10,000 copies of the same mesh** in two selectable modes:

- **Naive**: CPU submits **10,000** draw calls (one per instance).
- **Instanced**: CPU submits **1** draw call with `instanceCount = 10,000`, and the VS indexes transforms via `SV_InstanceID`.

This Day1 deliverable must be **engine-extensible by composition** (modules with explicit contracts), while keeping feature scope minimal.

### Success Criteria (non-negotiable)
- Scene visibly renders a 100×100 grid of cubes (or triangles) every frame.
- Runtime toggle switches **Naive ↔ Instanced** deterministically at a frame boundary.
- Logs show at least: `mode, draw_calls, cpu_record_ms` (GPU timestamps optional; seam must exist).
- No GPU lifetime bugs: no flicker/garbage caused by reusing in-flight allocators/resources/descriptors.

### Preconditions / Assumptions
- Windows + Visual Studio build works.
- DX12 debug layer can be enabled in Debug builds.
- Swapchain is **2–3 buffers** (typical), but **frames-in-flight may differ** later — do not hard-couple them in the design.

### Project-file hygiene (important for agent workflows)
- Avoid editing `.vcxproj` / `.sln` unless necessary.
  - Prefer adding new `.cpp/.h` under existing filters/folders without touching build settings.
  - If project changes are required (new include paths, preprocessor defs), do them as a **single, reviewed commit**.
- Keep runtime assets/shaders loadable from the repo structure (no absolute paths).

---

## 2) Architecture Design

### Core Principle
**Separate “swapchain backbuffer rotation” from “frame context reuse gating.”**
They are related but not identical:
- Swapchain backbuffers are chosen by `GetCurrentBackBufferIndex()`.
- Frame contexts are chosen by a **frame counter ring** and fenced reuse policy.

### Minimal Module Set (composition-first)

#### PlatformWindow
**Owns**
- Win32 window creation, message pump, resize events.
**Contract**
- Exposes: `HWND`, `GetClientSize()`, `PollEvents()`.

#### DeviceContext
**Owns**
- `ID3D12Device`, (debug layer enable), adapter selection, `ID3D12CommandQueue`, `ID3D12Fence`, fence event.
- Optional: `DeviceRemovedReason()` reporting seam.
**Contract**
- Exposes: `Device()`, `Queue()`, `Fence()`, `SignalFence()`, `WaitFence(v)`.

#### SwapchainPresenter
**Owns**
- Swapchain + backbuffers + RTV heap + RTV handles.
**Contract**
- Exposes: `CurrentBackBufferIndex()`, `BackBuffer(i)`, `RTV(i)`.
- Provides: `TransitionBB_PresentToRT()` and `TransitionBB_RTToPresent()` helpers (or at least “the caller must do it” contract).
**Extensibility seam**
- Resize: recreate backbuffers + RTVs; inform renderer to rebuild dependent resources if needed.

#### FrameContextRing (frames-in-flight)
**Owns (per FrameContext)**
- Command allocator (DIRECT)
- Optional: per-frame “upload arena” slice + per-frame descriptor slice
- Fence value for reuse gating
**Contract**
- `AcquireFrameContext(frameId)`:
  - **Waits** for the context fence to complete before reuse (unless explicitly in a “stomp microtest”).
- **FrameContext selection** is by `frameId % FrameCount` (NOT backbuffer index).
**Reason**
- Swapchain buffer count and frames-in-flight must be decoupled.

#### DescriptorSystem
**Owns**
- CBV/SRV/UAV shader-visible heap(s)
- A suballocator that can hand out:
  - **per-frame stable slots** (safe for in-flight command lists)
  - optional “global persistent slots” (rare)
**Contract**
- If a descriptor slot may be referenced by an in-flight command list, it must not be overwritten until its fence completes.
**Design note**
- Prefer **per-frame descriptor slices** to avoid accidental descriptor stomps.

#### ResourceSystem (Buffers + Arenas)
**Owns**
- `Buffer` wrappers (resource + size + current state + debug name)
- Upload strategy:
  - Day1 allowed: per-frame upload buffer(s) + persistent mapping
  - Must include a seam for: `UploadArena` suballocation per frame
**Contract**
- Allocations return `(resource, offset, cpuPtr, gpuVA)` and a **lifetime scope**.
- Resource state transitions are explicit (either by caller or a tracker seam).

#### ShaderPipelineLibrary
**Owns**
- HLSL compilation (or loads precompiled blobs later)
- Root signature + PSO creation
- Named pipeline lookup
**Contract (ABI)**
- Root parameter indices are stable and named (enum).
- Shader “mailboxes” must match CPU binding:
  - `b0 space0` = frame constants
  - `t0 space0` = transforms SRV table
**Critical correction**
- Descriptor range flags: do **NOT** claim `DATA_STATIC` if underlying data changes across frames.
  - Use **`DATA_STATIC_WHILE_SET_AT_EXECUTE`** (or equivalent safe default) because per-frame data is stable only during GPU execution of that command list.

#### RenderScene + DrawList
**Owns**
- Geometry (VB/IB) resources
- Instance source (transforms buffer)
- Builds a `DrawList` each frame:
  - `(pipeline, geometry, rootBindings, instanceCount, mode)`
**Contract**
- Recording uses a consistent ritual order:
  - Set PSO → Set RootSig → Set Heaps → Set Root Params → IA/RS/OM → Draw.

#### ToggleSystem
**Owns**
- Runtime toggles:
  - mode: instanced/naive
  - optional proof levers: bind-wrong-rootparam, omit-setheaps, mailbox shift, lifetime stomp
**Contract**
- Toggle takes effect **only at frame boundary** (deterministic behavior).

---

## 3) Implementation Strategy

### Step 0 — Keep the vertical slice runnable
Before modular refactors, maintain a “known-good” runnable path:
- Window + device + swapchain + RTVs
- One cube VB/IB (DEFAULT)
- RootSig + PSO
- Frame loop that clears + draws something

### Step 1 — Data model for 10,000 instances
- `kInstanceCount = 10,000`
- Transform layout in GPU buffer:
  - `StructuredBuffer<float4x4>` (`stride = 64 bytes`)
- Deterministic transform generation:
  - 100×100 grid
  - Optional sentinel: instance 0 translated far away (proves indexing correctness)

### Step 2 — Buffer update path (per frame)
- CPU writes transforms into a persistently mapped UPLOAD region.
- GPU reads transforms from DEFAULT buffer via SRV.
- Copy path each frame:
  - Upload → Default via `CopyBufferRegion`

### Step 3 — Resource state contract (explicit)
Transforms DEFAULT buffer must follow:
- If updated every frame:
  - `NON_PIXEL_SHADER_RESOURCE -> COPY_DEST -> NON_PIXEL_SHADER_RESOURCE`
Backbuffer must follow:
- `PRESENT -> RENDER_TARGET -> PRESENT`

### Step 4 — Two draw modes

#### Naive Mode (10,000 submissions)
- Loop `i = 0..9999`
- Submit:
  - `DrawIndexedInstanced(indexCount, 1, 0, 0, i)`
- Uses `StartInstanceLocation = i` so shader `SV_InstanceID` sees `iid = i`.

#### Instanced Mode (1 submission)
- Submit:
  - `DrawIndexedInstanced(indexCount, 10000, 0, 0, 0)`
- Shader uses `SV_InstanceID` directly.

### Step 5 — Measurement seam (minimum)
- CPU record timing:
  - measure time spent building the command list per frame
- GPU timestamps:
  - optional to fully implement on Day1, but keep the seam:
    - query heap allocation strategy
    - “read previous completed frame” rule to avoid stalling

### Step 6 — Proof toggles (diagnostic levers)
Include optional failure toggles that create deterministic breakage:
- Bind descriptor table to wrong root param index
- Mailbox shift (root sig expects `t1`, shader reads `t0`)
- Omit `SetDescriptorHeaps`
- Lifetime stomp (skip fence wait) → flicker/garbage eventually

---

## 4) Resource Management & Extensibility

### Fence-gated reuse (hard contract)
- Any per-frame allocator, upload slice, descriptor slice, and transient resource must not be reused until:
  - `CompletedFence >= FrameContext.fenceValue`

### Descriptor lifetime rules (hard contract)
- Shader-visible heap contents referenced by an in-flight command list must remain valid until fence completes.
- Recommended Day1 policy:
  - **Per-frame descriptor slice** for transforms SRV slot(s)
  - Avoid rewriting the same slot across frames unless it is fenced-safe

### Upload strategy (start simple, scale cleanly)
Day1 allowed:
- One upload buffer per frame context, persistently mapped.

Extensibility seam:
- `UploadArena` per frame (ring suballocation):
  - returns `(cpuPtr, gpuVA, resource, offset)`
  - enforces alignment:
    - CBVs: 256-byte alignment
    - Structured buffers: 16/64-byte appropriate alignment (keep explicit)
  - fence-gated reuse when the frame completes

### Resource state tracking seam (you should plan this early)
Even if Day1 uses explicit barriers directly in Tick:
- Provide a place for a future `ResourceStateTracker`:
  - tracks current state per resource
  - emits minimal transitions
This avoids “barrier spaghetti” when you add:
- multiple passes
- compute/copy queues
- async uploads
- texture arrays

### Resize + device removed (often forgotten, design for it now)
- Resize handling:
  - recreate swapchain backbuffers + RTV heap
  - update viewport/scissor
  - renderer notified (or owns viewport state)
- Device removed seam:
  - log `GetDeviceRemovedReason()` and fail fast in Debug builds

### Engine growth targets (not implemented Day1, but enabled by this design)
- Multiple pipelines: more shaders/PSOs, materials
- Geometry expansion: procedural mesh generation, mesh loader
- Texture2DArray path: per-instance `texIndex` + descriptor tables
- CPU culling / compaction; later GPU culling + indirect draw
- Job system: build transforms/culling on worker threads; record command lists on render thread

