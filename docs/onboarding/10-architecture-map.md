# Architecture Map

This document shows component ownership and relationships.

---

## Ownership Hierarchy

```
Dx12Context (main orchestrator)
├── Core DX12 Objects
│   ├── IDXGIFactory6
│   ├── IDXGIAdapter1
│   ├── ID3D12Device
│   ├── ID3D12CommandQueue
│   ├── IDXGISwapChain3
│   └── ID3D12GraphicsCommandList (shared, reset per-frame)
│
├── Descriptor Heaps
│   ├── m_rtvHeap (RTV, non-shader-visible, 3 slots)
│   ├── m_dsvHeap (DSV, non-shader-visible, 1 slot)
│   └── m_descRing (CBV/SRV/UAV, shader-visible, 1024 slots)
│
├── Subsystems
│   ├── FrameContextRing m_frameRing
│   │   └── FrameContext[3] m_frames
│   │       ├── ID3D12CommandAllocator
│   │       ├── FrameLinearAllocator uploadAllocator (1 MB)
│   │       ├── ResourceHandle transformsHandle
│   │       └── uint32_t srvSlot
│   │
│   ├── ResourceRegistry m_resourceRegistry
│   │   └── Entry[] (resource + state + generation)
│   │
│   ├── ResourceStateTracker m_stateTracker
│   │   └── unordered_map<ID3D12Resource*, TrackedResource>
│   │
│   ├── ShaderLibrary m_shaderLibrary
│   │   ├── ID3D12RootSignature
│   │   ├── PSOCache (owns PSOs)
│   │   └── ID3DBlob[] (shader bytecode)
│   │
│   ├── GeometryFactory m_geometryFactory
│   │   └── Fence + event for sync uploads
│   │
│   ├── RenderScene m_scene
│   │   ├── Cube VB/IB
│   │   ├── Floor VB/IB
│   │   └── Marker VB
│   │
│   ├── ImGuiLayer m_imguiLayer
│   │   └── ID3D12DescriptorHeap (private SRV heap for fonts)
│   │
│   └── UploadArena m_uploadArena
│       └── (wraps FrameLinearAllocator with metrics)
│
└── Render Targets
    ├── ID3D12Resource[3] m_backBuffers
    └── ID3D12Resource m_depthBuffer
```

> **Source-of-Truth**: Member declarations in `Renderer/DX12/Dx12Context.h`

---

## Component Responsibilities

### Dx12Context
**Role**: Main orchestrator, owns all subsystems
**Key Methods**: `Initialize()`, `Render()`, `Shutdown()`
**File**: `Renderer/DX12/Dx12Context.h`

The single entry point for frame rendering. Coordinates all subsystems in correct order.

### FrameContextRing
**Role**: Per-frame resource management with fence-gated reuse
**Key Data**: Command allocator, upload allocator, transforms buffer per frame
**File**: `Renderer/DX12/FrameContextRing.h`

Critical invariant: Uses `frameId % FrameCount` for context selection, NOT backbuffer index.

### FrameLinearAllocator
**Role**: Per-frame bump allocator for upload heap
**Key Method**: `Allocate(size, alignment, tag)`
**File**: `Renderer/DX12/FrameLinearAllocator.h`

Reset each frame after fence wait. No deallocation - just reset offset to 0.

### UploadArena
**Role**: Unified front-door for upload allocations with metrics
**Key Data**: Allocation counts, bytes, peak offset
**File**: `Renderer/DX12/UploadArena.h`

Wraps FrameLinearAllocator, adds diagnostic instrumentation. Enable HUD with U key.

### ResourceRegistry
**Role**: Handle-based resource ownership with generation validation
**Key Concept**: 64-bit handle = generation + index + type
**File**: `Renderer/DX12/ResourceRegistry.h`

Prevents use-after-free bugs. Generation increments on slot reuse.

### ResourceStateTracker
**Role**: SOLE authority for resource state tracking and barrier emission
**Key Methods**: `Transition()`, `FlushBarriers()`
**File**: `Renderer/DX12/ResourceStateTracker.h`

All barrier calls should go through this tracker. Batches barriers for efficiency.

### DescriptorRingAllocator
**Role**: Fence-protected descriptor ring for shader-visible heap
**Key Concept**: Reserved slots + dynamic ring
**File**: `Renderer/DX12/DescriptorRingAllocator.h`

Layout: `[Reserved: 0,1,2][Dynamic: 3..1023]`

### ShaderLibrary
**Role**: Root signature, PSO creation, shader management
**Key Data**: Root param enum defines CPU/GPU ABI
**File**: `Renderer/DX12/ShaderLibrary.h`

### GeometryFactory
**Role**: Init-time synchronized buffer uploads
**Key Method**: `CreateVertexBuffer()`, `CreateIndexBuffer()`
**File**: `Renderer/DX12/GeometryFactory.h`

Uses its own fence for blocking uploads. NOT used at runtime.

### RenderScene
**Role**: Scene geometry (VB/IB) and draw recording
**Key Methods**: `RecordDraw()`, `RecordDrawNaive()`, `RecordDrawFloor()`
**File**: `Renderer/DX12/RenderScene.h`

### PassOrchestrator
**Role**: Pass execution sequencing
**Key Method**: `Execute()` - runs Clear → Geometry → ImGui
**File**: `Renderer/DX12/PassOrchestrator.h`

### ImGuiLayer
**Role**: HUD overlay and input handling
**Key Methods**: `BeginFrame()`, `RenderHUD()`, `RecordCommands()`
**File**: `Renderer/DX12/ImGuiLayer.h`

### ToggleSystem
**Role**: Runtime mode switching (draw mode, color mode, diagnostics)
**Key Methods**: Static getters/setters for all toggles
**File**: `Renderer/DX12/ToggleSystem.h` (header-only)

---

## Data Flow

```
[CPU] UpdateFrameConstants() → UploadArena → FrameLinearAllocator → Upload Buffer
                                                     ↓
[GPU]                              Copy cmd → Default Buffer → Shader Read
```

### Upload Path
1. CPU writes to mapped upload buffer via `UploadArena::Allocate()`
2. Command list records `CopyBufferRegion()` to default heap
3. Barrier transitions to `NON_PIXEL_SHADER_RESOURCE`
4. Shader reads via SRV

### Binding Path
1. Root signature defines parameter layout
2. `SetGraphicsRootConstantBufferView()` binds frame CB at RP0
3. `SetGraphicsRootDescriptorTable()` binds transforms SRV at RP1
4. Draw call uses bound resources

---

## Initialization Order

```cpp
// In Dx12Context::Initialize()
InitDevice();           // Device, queue
InitSwapChain();        // Swap chain, format
InitRenderTargets();    // RTV heap, backbuffers
InitDepthBuffer();      // DSV heap, depth buffer
InitFrameResources();   // DescRing, Registry, FrameRing, StateTracker
InitShaders();          // ShaderLibrary (root sig + PSO)
InitScene();            // GeometryFactory, RenderScene (uploads geometry)
InitImGui();            // ImGuiLayer
```

> **Source-of-Truth**: `Dx12Context::Initialize()` in `Renderer/DX12/Dx12Context.cpp`

---

## Shutdown Order

Reverse of initialization, with fence wait first:
```cpp
m_frameRing.WaitForAll();  // GPU drain
m_scene.Shutdown();
m_geometryFactory.Shutdown();
m_imguiLayer.Shutdown();
m_shaderLibrary.Shutdown();
m_resourceRegistry.Shutdown();
m_frameRing.Shutdown();
// ... release COM objects
```

---

## Further Reading

| Topic | Document | What to Look For |
|-------|----------|------------------|
| Frame timeline | [20-frame-lifecycle.md](20-frame-lifecycle.md) | Per-frame operations sequence |
| Resource tracking | [30-resource-ownership.md](30-resource-ownership.md) | StateTracker + Registry details |
| Binding ABI | [40-binding-abi.md](40-binding-abi.md) | Root parameter layout |
| Upload system | [50-uploadarena.md](50-uploadarena.md) | Arena + allocator details |
| Init uploads | [60-geometryfactory.md](60-geometryfactory.md) | GeometryFactory deep dive |

---

## Study Path

### Read Now
- This document (you're here)
- [20-frame-lifecycle.md](20-frame-lifecycle.md) - What happens each frame

### Read When Broken
- [30-resource-ownership.md](30-resource-ownership.md) - Barrier/state issues
- [70-debug-playbook.md](70-debug-playbook.md) - Validation errors

### Read Later
- [80-how-to-extend.md](80-how-to-extend.md) - Adding new components
