# DX12EngineLab Verified Facts

> **Canonical Source of Truth** - All other onboarding docs reference this file.
> Every fact below is traceable to a specific symbol in the codebase.

---

## Frame Buffering

| Fact | Value | Source |
|------|-------|--------|
| FrameCount | 3 | `FrameContextRing::FrameCount` in `Renderer/DX12/FrameContextRing.h` |
| InstanceCount | 10,000 | `Renderer::InstanceCount` in `Renderer/DX12/FrameContextRing.h` |
| Frame Index Formula | `frameId % FrameCount` | `FrameContextRing::BeginFrame()` in `Renderer/DX12/FrameContextRing.cpp` |

**Key Invariant**: Frame resources use `frameId % FrameCount`, NOT backbuffer index.

---

## Upload Allocator (FrameLinearAllocator)

| Fact | Value | Source |
|------|-------|--------|
| Capacity | 1 MB (1,048,576 bytes) | `ALLOCATOR_CAPACITY` in `FrameContextRing.cpp` |
| Default Alignment | 256 bytes | `FrameLinearAllocator::Allocate()` default param in `FrameLinearAllocator.h` |
| CBV Alignment | 256 bytes | `CBV_ALIGNMENT` in `FrameContextRing.cpp` and `Dx12Context.cpp` |
| Frame CB Size | 256 bytes | `CB_SIZE` constant - ViewProj matrix (64 bytes) aligned to 256 |
| Transforms Size | 640 KB (655,360 bytes) | `TRANSFORMS_SIZE` = 10,000 × 64 bytes (float4x4) |
| Total Per-Frame Usage | ~640 KB | Frame CB (256) + Transforms (640 KB) |

**Allocation Pattern**: Bump pointer, reset to 0 on `BeginFrame()` after fence wait.

---

## Root Signature ABI

| RP Index | Enum | Register | Type | Purpose |
|----------|------|----------|------|---------|
| 0 | `RP_FrameCB` | b0 space0 | CBV | Frame constants (ViewProj 4x4) |
| 1 | `RP_TransformsTable` | t0 space0 | Descriptor Table (SRV) | Transforms StructuredBuffer |
| 2 | `RP_InstanceOffset` | b1 space0 | Root Constant (1 DWORD) | Instance offset for naive draws |
| 3 | `RP_DebugCB` | b2 space0 | Root Constants (4 DWORD) | Debug constants (ColorMode) |

**Source**: `enum RootParam` in `Renderer/DX12/ShaderLibrary.h`

**Critical Contract**: These indices are the CPU/GPU ABI. Shader register assignments must match.

---

## Descriptor Heap Layout

| Fact | Value | Source |
|------|-------|--------|
| Total Capacity | 1024 | `m_descRing.Initialize(device, 1024, FrameCount)` in `Dx12Context.cpp` |
| Reserved Slots | 0, 1, 2 | Per-frame transforms SRVs at `frameIndex` |
| Dynamic Ring Start | 3 | `reservedCount = FrameCount = 3` |
| Dynamic Ring Size | 1021 slots | `1024 - 3` |
| Descriptor Size | Device-dependent | `DescriptorRingAllocator::GetDescriptorSize()` |

**Source**: `DescriptorRingAllocator` in `Renderer/DX12/DescriptorRingAllocator.h`

**Heap Layout**:
```
[Reserved: 0,1,2] [Dynamic Ring: 3..1023]
     ↑                    ↑
  Per-frame SRVs    Transient allocations
```

---

## Runtime Toggles

| Key | Symbol | Type | Default | Purpose |
|-----|--------|------|---------|---------|
| T | `s_drawMode` | `DrawMode` enum | `Instanced` | Switch: Instanced (1 draw) ↔ Naive (10k draws) |
| C | `s_colorMode` | `ColorMode` enum | `FaceDebug` | Cycle: Face → Instance → Lambert |
| G | `s_gridEnabled` | `bool` | `true` | Toggle floor/cube visibility |
| U | `s_uploadDiagEnabled` | `bool` | `false` | Upload Arena HUD overlay |
| M | `s_markersEnabled` | `bool` | `false` | Corner marker triangles |
| F1 | `s_sentinelInstance0` | `bool` | `false` | Move instance 0 to (150, 50, 150) |
| F2 | `s_stompLifetime` | `bool` | `false` | Use wrong frame SRV (causes flicker) |

**Source**: `ToggleSystem` in `Renderer/DX12/ToggleSystem.h`

### DrawMode Enum
```cpp
enum class DrawMode : uint32_t {
    Instanced,  // 1 draw call with 10k instances
    Naive       // 10k draw calls with 1 instance each
};
```

### ColorMode Enum
```cpp
enum class ColorMode : uint32_t {
    FaceDebug = 0,   // Face colors by SV_PrimitiveID
    InstanceID = 1,  // Stable hue per instance (golden ratio hash)
    Lambert = 2      // Simple directional lighting (gray diffuse)
};
```

---

## UploadArena Metrics

| Field | Type | Purpose |
|-------|------|---------|
| `allocCalls` | `uint32_t` | Allocation calls this frame (expected: 2) |
| `allocBytes` | `uint64_t` | Bytes allocated (~640 KB typical) |
| `peakOffset` | `uint64_t` | High-water mark in allocator |
| `capacity` | `uint64_t` | Allocator capacity (1 MB) |
| `lastAllocTag` | `const char*` | Debug tag of last allocation |
| `lastAllocSize` | `uint64_t` | Size of last allocation |
| `lastAllocOffset` | `uint64_t` | Offset of last allocation |

**Source**: `UploadArenaMetrics` struct in `Renderer/DX12/UploadArena.h`

---

## ResourceHandle Layout

```
| 32-bit generation | 24-bit index | 8-bit type |
```

| Component | Bits | Accessor | Purpose |
|-----------|------|----------|---------|
| Generation | 63-32 | `GetGeneration()` | Use-after-free detection |
| Index | 31-8 | `GetIndex()` | Slot in registry |
| Type | 7-0 | `GetType()` | `ResourceType` enum |

**Source**: `ResourceHandle` struct in `Renderer/DX12/ResourceRegistry.h`

### ResourceType Enum
```cpp
enum class ResourceType : uint8_t {
    None = 0,
    Buffer,
    Texture2D,
    RenderTarget,
    DepthStencil
};
```

---

## Frame Lifecycle (Dx12Context::Render)

| Phase | Operation | Key Functions |
|-------|-----------|---------------|
| 1 | Delta time + Camera | `UpdateDeltaTime()`, `UpdateCamera(dt)` |
| 2 | Begin Frame | `m_frameRing.BeginFrame(frameId)` - fence wait + allocator reset |
| 3 | Upload Arena Begin | `m_uploadArena.Begin()` - set active allocator |
| 4 | Descriptor Ring Begin | `m_descRing.BeginFrame(completedFence)` - retire completed frames |
| 5 | CB Allocation | `UpdateFrameConstants()` - allocate via UploadArena |
| 6 | Transforms Allocation | `UpdateTransforms()` - allocate via UploadArena |
| 7 | Command List Reset | `m_commandList->Reset()` |
| 8 | ImGui Begin | `m_imguiLayer.BeginFrame()` |
| 9 | Barriers + Copy | `RecordBarriersAndCopy()` - COPY_DEST → copy → NON_PIXEL_SHADER_RESOURCE |
| 10 | ImGui Build | `m_imguiLayer.RenderHUD()` |
| 11 | Record Passes | `RecordPasses()` via `PassOrchestrator::Execute()` |
| 12 | Upload Arena End | `m_uploadArena.End()` - snapshot metrics |
| 13 | Execute + Present | `ExecuteAndPresent()` - Close + Execute + Signal + Present |
| 14 | Increment Frame | `++m_frameId` |

**Source**: `Dx12Context::Render()` in `Renderer/DX12/Dx12Context.cpp`

---

## Pass Execution Order

| Order | Pass | Condition |
|-------|------|-----------|
| 1 | ClearPass | Always (clears RT + depth) |
| 2 | GeometryPass | Grid + cubes based on toggles |
| 3 | ImGuiPass | Always (HUD overlay) |

**Source**: `PassOrchestrator::Execute()` in `Renderer/DX12/PassOrchestrator.h`

---

## Key File Paths

| Component | Header | Implementation |
|-----------|--------|----------------|
| Dx12Context | `Renderer/DX12/Dx12Context.h` | `Renderer/DX12/Dx12Context.cpp` |
| FrameContextRing | `Renderer/DX12/FrameContextRing.h` | `Renderer/DX12/FrameContextRing.cpp` |
| FrameLinearAllocator | `Renderer/DX12/FrameLinearAllocator.h` | `Renderer/DX12/FrameLinearAllocator.cpp` |
| UploadArena | `Renderer/DX12/UploadArena.h` | `Renderer/DX12/UploadArena.cpp` |
| ResourceRegistry | `Renderer/DX12/ResourceRegistry.h` | `Renderer/DX12/ResourceRegistry.cpp` |
| ResourceStateTracker | `Renderer/DX12/ResourceStateTracker.h` | `Renderer/DX12/ResourceStateTracker.cpp` |
| ShaderLibrary | `Renderer/DX12/ShaderLibrary.h` | `Renderer/DX12/ShaderLibrary.cpp` |
| DescriptorRingAllocator | `Renderer/DX12/DescriptorRingAllocator.h` | `Renderer/DX12/DescriptorRingAllocator.cpp` |
| PassOrchestrator | `Renderer/DX12/PassOrchestrator.h` | `Renderer/DX12/PassOrchestrator.cpp` |
| GeometryFactory | `Renderer/DX12/GeometryFactory.h` | `Renderer/DX12/GeometryFactory.cpp` |
| RenderScene | `Renderer/DX12/RenderScene.h` | `Renderer/DX12/RenderScene.cpp` |
| ImGuiLayer | `Renderer/DX12/ImGuiLayer.h` | `Renderer/DX12/ImGuiLayer.cpp` |
| ToggleSystem | `Renderer/DX12/ToggleSystem.h` | (header-only) |
| BarrierScope | `Renderer/DX12/BarrierScope.h` | (header-only) |

---

## Camera Defaults

| Property | Value | Source |
|----------|-------|--------|
| Initial Position | (0, 180, -220) | `FreeCamera` struct in `Dx12Context.h` |
| Initial Yaw | 0.0 rad | Looking along +Z |
| Initial Pitch | -0.5 rad | Looking down |
| FOV | π/4 (45°) | `XM_PIDIV4` |
| Near/Far | 1.0 / 1000.0 | Clip planes |
| Move Speed | 100 units/sec | WASD movement |
| Look Speed | 1.5 rad/sec | Q/E rotation |

**Source**: `Dx12Context::FreeCamera` in `Renderer/DX12/Dx12Context.h`

---

## Build Configuration

| Config | Platform | Debug Layer |
|--------|----------|-------------|
| Debug | x64 | ON |
| Release | x64 | OFF |

**Solution**: `DX12EngineLab.sln` (VS2022)

---

*Last updated from codebase exploration. All values verified against source.*
