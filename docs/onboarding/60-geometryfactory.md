# GeometryFactory

This document covers init-time synchronized buffer uploads.

---

## Overview

`GeometryFactory` handles **initialization-time** buffer uploads. This is distinct from the per-frame `UploadArena` pattern:

| System | When | Sync | Memory |
|--------|------|------|--------|
| UploadArena | Every frame | Async (fence after frame) | Reused per frame |
| GeometryFactory | Init only | Sync (CPU waits for GPU) | Temporary upload buffer |

---

## Purpose

Creates vertex and index buffers in DEFAULT heap with proper synchronization:

1. Create temporary upload buffer
2. Copy CPU data to upload buffer
3. Record copy command to default buffer
4. Execute and **wait** for completion
5. Release upload buffer

The CPU wait at init is acceptable - it only happens once at startup.

> **Source-of-Truth**: `GeometryFactory` in `Renderer/DX12/GeometryFactory.h`

---

## API

```cpp
// Create vertex buffer in DEFAULT heap
VertexBufferResult CreateVertexBuffer(const void* data, uint64_t sizeBytes, uint32_t strideBytes);

// Create index buffer in DEFAULT heap
IndexBufferResult CreateIndexBuffer(const void* data, uint64_t sizeBytes, DXGI_FORMAT format);
```

### Result Structs

```cpp
struct VertexBufferResult {
    ComPtr<ID3D12Resource> resource;
    D3D12_VERTEX_BUFFER_VIEW view;
};

struct IndexBufferResult {
    ComPtr<ID3D12Resource> resource;
    D3D12_INDEX_BUFFER_VIEW view;
};
```

---

## Internal Flow

```cpp
bool GeometryFactory::UploadBuffer(ID3D12Resource* dstDefault, const void* srcData,
                                    uint64_t numBytes, D3D12_RESOURCE_STATES afterState)
{
    // 1. Create temporary upload buffer
    ComPtr<ID3D12Resource> uploadBuffer;
    device->CreateCommittedResource(..., D3D12_HEAP_TYPE_UPLOAD, ...);

    // 2. Copy CPU data to upload buffer
    void* mapped;
    uploadBuffer->Map(0, nullptr, &mapped);
    memcpy(mapped, srcData, numBytes);
    uploadBuffer->Unmap(0, nullptr);

    // 3. Create temporary command allocator + list
    ComPtr<ID3D12CommandAllocator> cmdAlloc;
    ComPtr<ID3D12GraphicsCommandList> cmdList;
    // ...

    // 4. Record copy command
    cmdList->CopyBufferRegion(dstDefault, 0, uploadBuffer.Get(), 0, numBytes);

    // 5. Transition to final state
    D3D12_RESOURCE_BARRIER barrier = {...};
    cmdList->ResourceBarrier(1, &barrier);

    // 6. Close and execute
    cmdList->Close();
    ID3D12CommandList* lists[] = { cmdList.Get() };
    queue->ExecuteCommandLists(1, lists);

    // 7. Signal and wait (BLOCKING)
    m_uploadFenceValue++;
    queue->Signal(m_uploadFence.Get(), m_uploadFenceValue);
    m_uploadFence->SetEventOnCompletion(m_uploadFenceValue, m_uploadFenceEvent);
    WaitForSingleObject(m_uploadFenceEvent, INFINITE);

    // 8. Upload buffer released when function returns (ComPtr destructor)
    return true;
}
```

---

## Usage in RenderScene

```cpp
bool RenderScene::Initialize(GeometryFactory* factory)
{
    if (!CreateCubeGeometry(factory)) return false;
    if (!CreateFloorGeometry(factory)) return false;
    if (!CreateMarkerGeometry(factory)) return false;
    return true;
}

bool RenderScene::CreateCubeGeometry(GeometryFactory* factory)
{
    // Define vertex data
    struct Vertex { float x, y, z, nx, ny, nz; };
    Vertex vertices[] = { ... };

    // Define index data
    uint16_t indices[] = { ... };

    // Create buffers via factory
    auto vbResult = factory->CreateVertexBuffer(vertices, sizeof(vertices), sizeof(Vertex));
    if (!vbResult.resource) return false;

    auto ibResult = factory->CreateIndexBuffer(indices, sizeof(indices), DXGI_FORMAT_R16_UINT);
    if (!ibResult.resource) return false;

    // Store results
    m_vertexBuffer = vbResult.resource;
    m_vbv = vbResult.view;
    m_indexBuffer = ibResult.resource;
    m_ibv = ibResult.view;
    m_indexCount = ARRAYSIZE(indices);

    return true;
}
```

> **Source-of-Truth**: `RenderScene::Initialize()` in `Renderer/DX12/RenderScene.cpp`

---

## Why Not Use UploadArena?

| Aspect | UploadArena | GeometryFactory |
|--------|-------------|-----------------|
| Timing | Per-frame | Init-only |
| Sync | Async (fence at end of frame) | Sync (CPU wait) |
| Upload buffer | Reused each frame | Temporary, released after copy |
| Usage | Dynamic data (transforms) | Static geometry |

GeometryFactory is simpler for static data because:
1. No need to track upload buffer lifetime
2. After init, only the DEFAULT heap buffer exists
3. No per-frame overhead

---

## Why Excluded from UploadArena?

The plan explicitly excludes `GeometryFactory` from the `UploadArena` because:

1. **Different lifecycle**: Init-time vs. per-frame
2. **Different sync model**: Blocking wait vs. fence-gated reuse
3. **Temporary upload buffer**: Released after use, not reused
4. **No metrics needed**: Doesn't contribute to per-frame upload budgeting

Including it would complicate `UploadArena` without benefit.

---

## Initialization Order

```cpp
// In Dx12Context::Initialize()
bool Dx12Context::InitScene()
{
    // Initialize factory first (needs device + queue)
    if (!m_geometryFactory.Initialize(m_device.Get(), m_commandQueue.Get()))
        return false;

    // Scene uses factory for geometry creation
    if (!m_scene.Initialize(&m_geometryFactory))
        return false;

    return true;
}
```

The factory is initialized before scene, then scene creates all geometry during its init.

---

## Buffer Views

### Vertex Buffer View

```cpp
D3D12_VERTEX_BUFFER_VIEW view;
view.BufferLocation = resource->GetGPUVirtualAddress();
view.SizeInBytes = static_cast<UINT>(sizeBytes);
view.StrideInBytes = strideBytes;
```

### Index Buffer View

```cpp
D3D12_INDEX_BUFFER_VIEW view;
view.BufferLocation = resource->GetGPUVirtualAddress();
view.SizeInBytes = static_cast<UINT>(sizeBytes);
view.Format = format;  // DXGI_FORMAT_R16_UINT or R32_UINT
```

These views are stored and used directly in draw calls:
```cpp
cmdList->IASetVertexBuffers(0, 1, &m_vbv);
cmdList->IASetIndexBuffer(&m_ibv);
```

---

## Shutdown

```cpp
void GeometryFactory::Shutdown()
{
    if (m_uploadFenceEvent)
    {
        CloseHandle(m_uploadFenceEvent);
        m_uploadFenceEvent = nullptr;
    }
    m_uploadFence.Reset();
    m_device = nullptr;
    m_queue = nullptr;
}
```

The fence and event are the only persistent resources. Upload buffers and command lists are temporary.

---

## Failure Modes

### 1. Queue Mismatch

**Symptom**: GPU hang or corruption
**Cause**: Factory using different queue than renderer
**Fix**: Pass same command queue to both

### 2. Missing Wait

**Symptom**: Geometry renders garbage
**Cause**: Using buffer before copy completes
**Fix**: Ensure `WaitForSingleObject` completes before returning

### 3. Upload Buffer Too Small

**Symptom**: Crash in memcpy
**Cause**: Source data larger than upload buffer
**Fix**: Create upload buffer with correct size

---

## Further Reading

| Topic | Document | What to Look For |
|-------|----------|------------------|
| Per-frame uploads | [50-uploadarena.md](50-uploadarena.md) | Contrast with arena pattern |
| Resource states | [30-resource-ownership.md](30-resource-ownership.md) | Final state after copy |
| Architecture | [10-architecture-map.md](10-architecture-map.md) | Where factory fits |

---

## Study Path

### Read Now
- [70-debug-playbook.md](70-debug-playbook.md) - Debug techniques

### Read When Broken
- Check init order if geometry not visible
- Verify factory init before scene init

### Read Later
- [80-how-to-extend.md](80-how-to-extend.md) - Adding new geometry types
