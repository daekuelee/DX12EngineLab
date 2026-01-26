# Glossary

Repo-specific terminology and DX12 concepts as used in this codebase.

---

## A

### Allocation
A result from `FrameLinearAllocator::Allocate()` or `UploadArena::Allocate()`. Contains CPU pointer, GPU virtual address, and offset. See `Allocation` struct in `FrameLinearAllocator.h`.

---

## B

### BackbufferScope
RAII helper for PRESENT ↔ RENDER_TARGET transitions. Defined in `BarrierScope.h`. Constructor transitions to RENDER_TARGET, destructor transitions back to PRESENT.

### BarrierScope
RAII helper for symmetric resource state transitions. Ensures cleanup barrier is emitted even on early return. Defined in `BarrierScope.h`.

### Bump Allocator
Memory allocation strategy where a single offset advances forward ("bumps"). No individual deallocation - entire allocator resets at once. Used by `FrameLinearAllocator`.

---

## C

### CBV (Constant Buffer View)
DX12 descriptor type for shader-readable constant buffers. Must be 256-byte aligned. In this codebase, bound via `SetGraphicsRootConstantBufferView()` at RP0.

### ColorMode
Enum for shader debug visualization. Values: `FaceDebug` (0), `InstanceID` (1), `Lambert` (2). Toggle with C key. See `ToggleSystem.h`.

### Copy Queue
DX12 queue type for data transfers. This codebase uses the direct queue for all operations including copies.

---

## D

### DEFAULT Heap
GPU-optimized memory (`D3D12_HEAP_TYPE_DEFAULT`). Fast GPU access but not CPU-accessible. Used for transforms buffer and static geometry.

### DescriptorRingAllocator
Fence-protected ring allocator for shader-visible CBV/SRV/UAV heap. Layout: reserved slots at front (0,1,2), dynamic ring for transient allocations. See `DescriptorRingAllocator.h`.

### DrawMode
Enum for draw strategy. `Instanced` = 1 draw call with 10k instances. `Naive` = 10k draw calls with 1 instance each. Toggle with T key. See `ToggleSystem.h`.

---

## F

### Fence
GPU/CPU synchronization primitive. Signal after GPU work, wait before reusing resources. Each frame context has its own fence value. See `FrameContextRing.h`.

### FrameContext
Per-frame resources: command allocator, upload allocator, transforms handle, SRV slot. Three contexts rotate via `frameId % 3`. See `FrameContextRing.h`.

### FrameContextRing
Manager for triple-buffered frame contexts. Handles fence wait before reuse, allocator reset. See `FrameContextRing.h`.

### FrameCount
Constant = 3. Number of frame contexts in flight. Defined in `FrameContextRing.h`.

### FrameId
Monotonic frame counter in `Dx12Context`. Used for frame context selection via `frameId % FrameCount`.

### FrameLinearAllocator
Per-frame bump allocator for upload heap. 1 MB capacity. Reset after fence wait. See `FrameLinearAllocator.h`.

---

## G

### Generation
32-bit counter in `ResourceHandle` that increments when slot is reused. Detects use-after-free bugs.

### GeometryFactory
Init-time buffer creation with synchronized upload. Uses blocking wait - only for startup. See `GeometryFactory.h`.

### Grid
The 10k cube instances. Toggle visibility with G key.

---

## H

### Handle
See `ResourceHandle`.

---

## I

### InstanceCount
Constant = 10,000. Number of cube instances. Defined in `FrameContextRing.h`.

### Instanced Drawing
Drawing multiple instances with one draw call. Used by default (DrawMode::Instanced).

---

## N

### Naive Drawing
Drawing one instance per draw call. 10k draw calls total. For comparison with instanced mode.

---

## P

### PassOrchestrator
Coordinates render pass execution: Clear → Geometry → ImGui. See `PassOrchestrator.h`.

### Persistent Map
Upload heap is mapped once at creation and stays mapped. No Map/Unmap per frame.

### PSO (Pipeline State Object)
DX12 object containing shader bytecode and fixed-function state. Cached in `PSOCache`.

---

## R

### Reserved Slots
First 3 descriptor slots (0,1,2) in the shader-visible heap. Hold per-frame transforms SRVs. Not managed by ring allocator.

### ResourceHandle
64-bit handle with generation validation. Layout: 32-bit generation, 24-bit index, 8-bit type. See `ResourceRegistry.h`.

### ResourceRegistry
Handle-based resource ownership. Creates resources, validates handles, prevents use-after-free. See `ResourceRegistry.h`.

### ResourceStateTracker
SOLE authority for resource state tracking and barrier emission. All barriers should go through this. See `ResourceStateTracker.h`.

### Root Parameter (RP)
Index into root signature. RP0 = FrameCB, RP1 = TransformsTable, RP2 = InstanceOffset, RP3 = DebugCB. See `ShaderLibrary.h`.

### Root Signature
CPU/GPU ABI defining how resources are bound. Created once, used for all draws.

---

## S

### Sentinel Instance
Instance 0 moved to (150, 50, 150) when F1 pressed. Used to verify transform upload.

### Shader-Visible Heap
Descriptor heap that can be bound for shader access. Only one CBV/SRV/UAV heap can be bound at a time.

### SRV (Shader Resource View)
Descriptor for shader-readable resources. Transforms buffer uses StructuredBuffer SRV.

### StateTracker
Shorthand for `ResourceStateTracker`.

### Stomp
Incorrectly using another frame's resources. F2 toggle intentionally causes this.

### StructuredBuffer
HLSL buffer type with typed elements. Transforms buffer is `StructuredBuffer<float4x4>`.

---

## T

### Toggle
Runtime mode switch. Managed by `ToggleSystem`. Keys: T, C, G, U, M, F1, F2.

### Transforms Buffer
Per-frame DEFAULT heap buffer holding 10k float4x4 matrices. Uploaded each frame, read by vertex shader.

### Triple Buffering
Using 3 frame contexts to allow CPU to run ahead of GPU. Reduces stalls.

---

## U

### UPLOAD Heap
CPU-accessible memory (`D3D12_HEAP_TYPE_UPLOAD`). Slow GPU reads. Used for staging data before copy to DEFAULT heap.

### UploadArena
Unified front-door for per-frame upload allocations. Adds metrics for diagnostics. See `UploadArena.h`.

---

## V

### ViewProj
Combined view and projection matrix. Stored in FrameCB, bound at b0.

---

## Key Constants Reference

| Name | Value | Location |
|------|-------|----------|
| FrameCount | 3 | `FrameContextRing.h` |
| InstanceCount | 10,000 | `FrameContextRing.h` |
| ALLOCATOR_CAPACITY | 1 MB | `FrameContextRing.cpp` |
| CBV_ALIGNMENT | 256 | `Dx12Context.cpp` |
| Descriptor Capacity | 1024 | `Dx12Context.cpp` |
| Reserved Slots | 3 | `Dx12Context.cpp` |

For complete facts, see [_facts.md](_facts.md).

---

## File Path Reference

| Component | Path |
|-----------|------|
| Main orchestrator | `Renderer/DX12/Dx12Context.cpp` |
| Frame resources | `Renderer/DX12/FrameContextRing.cpp` |
| Upload allocator | `Renderer/DX12/FrameLinearAllocator.cpp` |
| Root signature | `Renderer/DX12/ShaderLibrary.cpp` |
| Toggles | `Renderer/DX12/ToggleSystem.h` |
| State tracking | `Renderer/DX12/ResourceStateTracker.cpp` |

---

*Terms are defined as used in this specific codebase, not general DX12 usage.*
