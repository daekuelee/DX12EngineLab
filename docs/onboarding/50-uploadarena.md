# UploadArena

This document covers the unified upload allocation system.

---

## Overview

`UploadArena` is the front-door for all per-frame upload allocations. It wraps `FrameLinearAllocator` and adds diagnostic instrumentation.

```
Application Code
       │
       ▼
  UploadArena        ← Metrics + diagnostics
       │
       ▼
FrameLinearAllocator ← Actual allocation
       │
       ▼
  Upload Buffer      ← 1 MB per frame, persistently mapped
```

---

## FrameLinearAllocator

### Allocation Pattern

```cpp
Allocation Allocate(uint64_t size, uint64_t alignment = 256, const char* tag = nullptr);
```

Simple bump pointer allocation:
1. Align current offset to requested alignment
2. Check if fits in capacity
3. Return pointer at offset, advance offset

**No deallocation** - entire allocator resets at frame start after fence wait.

> **Source-of-Truth**: `FrameLinearAllocator` in `Renderer/DX12/FrameLinearAllocator.h`

### Capacity and Usage

| Fact | Value | Source |
|------|-------|--------|
| Capacity | 1 MB | `ALLOCATOR_CAPACITY` in `FrameContextRing.cpp` |
| Frame CB | 256 bytes | ViewProj matrix, 256-byte aligned |
| Transforms | 640 KB | 10,000 × 64 bytes |
| Total used | ~640 KB | Headroom of ~360 KB |

### Allocation Struct

```cpp
struct Allocation {
    void* cpuPtr;               // Mapped CPU pointer for writing
    D3D12_GPU_VIRTUAL_ADDRESS gpuVA;  // For CBV binding
    uint64_t offset;            // For CopyBufferRegion source offset
};
```

---

## UploadArena

### Purpose

Wraps `FrameLinearAllocator` with:
1. Per-frame allocation counting
2. Byte tracking
3. Peak offset tracking
4. Debug tags for diagnostics
5. HUD display support

### Lifecycle

```cpp
// At frame start (after fence wait)
m_uploadArena.Begin(&frameCtx.uploadAllocator, diagEnabled);

// During frame
Allocation frameCB = m_uploadArena.Allocate(CB_SIZE, 256, "FrameCB");
Allocation transforms = m_uploadArena.Allocate(TRANSFORMS_SIZE, 256, "Transforms");

// At frame end (before execute)
m_uploadArena.End();
m_imguiLayer.SetUploadArenaMetrics(m_uploadArena.GetLastSnapshot());
```

> **Source-of-Truth**: `UploadArena` in `Renderer/DX12/UploadArena.h`

### Metrics

```cpp
struct UploadArenaMetrics {
    uint32_t allocCalls;      // Expected: 2 (FrameCB + Transforms)
    uint64_t allocBytes;      // Expected: ~655KB
    uint64_t peakOffset;      // High-water mark
    uint64_t capacity;        // 1 MB

    const char* lastAllocTag; // Debug: last allocation name
    uint64_t lastAllocSize;
    uint64_t lastAllocOffset;
};
```

### HUD Display

Press **U** to toggle Upload Arena diagnostics in HUD.

Shows:
- Allocation calls this frame
- Bytes allocated
- Peak offset
- Capacity utilization
- Last allocation details

---

## Upload Flow

### Step 1: CPU Write

```cpp
Allocation transformsAlloc = m_uploadArena.Allocate(TRANSFORMS_SIZE, 256, "Transforms");
float* transforms = static_cast<float*>(transformsAlloc.cpuPtr);

// CPU writes directly to mapped memory
for (uint32_t i = 0; i < InstanceCount; ++i)
{
    // Write float4x4 for instance i
    transforms[i * 16 + 0] = ...;
    // ...
}
```

No explicit flush needed - upload heap has write-combine memory.

### Step 2: Copy Command

```cpp
// Transition default buffer to COPY_DEST
m_stateTracker.Transition(transformsResource, D3D12_RESOURCE_STATE_COPY_DEST);
m_stateTracker.FlushBarriers(m_commandList.Get());

// Copy from upload to default
m_commandList->CopyBufferRegion(
    transformsResource, 0,                                    // dest, destOffset
    ctx.uploadAllocator.GetBuffer(), transformsAlloc.offset,  // src, srcOffset
    TRANSFORMS_SIZE);                                         // numBytes

// Transition to SRV state
m_stateTracker.Transition(transformsResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
m_stateTracker.FlushBarriers(m_commandList.Get());
```

> **Source-of-Truth**: `Dx12Context::RecordBarriersAndCopy()` in `Renderer/DX12/Dx12Context.cpp`

### Step 3: GPU Read

After execution, GPU reads from default heap buffer via SRV.

---

## Why This Architecture?

### Upload Heap Limitations

Upload heaps (`D3D12_HEAP_TYPE_UPLOAD`) have restrictions:
- CPU-visible, GPU-readable
- Cannot be used as render targets or UAVs
- Slow GPU reads (PCIe bandwidth)

### Default Heap Benefits

Default heaps (`D3D12_HEAP_TYPE_DEFAULT`) are:
- GPU-optimized memory
- Fast GPU access
- Required for high-bandwidth reads (transforms accessed per-vertex)

### The Copy Pattern

```
Upload Heap (1MB)          Default Heap (640KB per frame)
┌─────────────────┐        ┌─────────────────┐
│ FrameCB (256B)  │        │                 │
├─────────────────┤   ───► │ Transforms      │
│ Transforms      │  Copy  │ (SRV read)      │
│ (640KB)         │        │                 │
└─────────────────┘        └─────────────────┘
```

Frame CB is small, bound directly via GPU VA (root CBV). Transforms are large, copied to default heap.

---

## Alignment Rules

| Resource Type | Alignment | Why |
|--------------|-----------|-----|
| CBV | 256 bytes | D3D12 requirement |
| SRV buffer | 256 bytes | Conservative, avoids issues |
| Texture | Variable | Row pitch alignment |

Default alignment in `FrameLinearAllocator::Allocate()` is 256 bytes.

---

## Memory Layout Per Frame

```
Frame 0 Upload Buffer (1 MB):
┌────────────────────────────────────────────────────────────────┐
│ offset=0    │ offset=256  │                   │                │
│ FrameCB     │ Transforms  │     (unused)      │  (headroom)    │
│ (256B)      │ (640KB)     │                   │                │
└────────────────────────────────────────────────────────────────┘
              │             │
              │             └── peakOffset ≈ 656KB
              └────────────────── allocBytes ≈ 656KB
```

---

## Failure Modes

### 1. Out of Memory

**Symptom**: Assertion/crash in Debug build
**Cause**: Allocations exceed 1 MB
**Fix**: Increase `ALLOCATOR_CAPACITY` or optimize allocations

### 2. Alignment Violation

**Symptom**: D3D12 error about alignment
**Cause**: CBV bound without 256-byte alignment
**Fix**: Pass correct alignment to `Allocate()`

### 3. Write After Reset

**Symptom**: Data corruption
**Cause**: Writing to upload buffer after it's been reset
**Fix**: Ensure all writes happen between `Begin()` and copy command

### 4. Use Before Copy

**Symptom**: Shader reads stale/zero data
**Cause**: Drawing before copy completes
**Fix**: Ensure `CopyBufferRegion` and barrier before draw

---

## Diagnostics

### HUD (U key)

Shows real-time metrics:
```
Upload Arena
  allocCalls: 2
  allocBytes: 655616 (640 KB)
  peakOffset: 655616
  capacity: 1048576 (1 MB)
  lastAlloc: Transforms @ 256 (640000 bytes)
```

### Debug Output

Enable diagnostic mode for detailed logs:
```cpp
m_uploadArena.Begin(&allocator, true);  // diagEnabled = true
```

---

## Further Reading

| Topic | Document | What to Look For |
|-------|----------|------------------|
| Frame sync | [20-frame-lifecycle.md](20-frame-lifecycle.md) | When reset happens |
| Binding | [40-binding-abi.md](40-binding-abi.md) | How CBV/SRV are bound |
| Resources | [30-resource-ownership.md](30-resource-ownership.md) | Default heap resources |
| Facts | [_facts.md](_facts.md) | Exact capacity values |

---

## Study Path

### Read Now
- [60-geometryfactory.md](60-geometryfactory.md) - Init-time uploads (different pattern)

### Read When Broken
- [70-debug-playbook.md](70-debug-playbook.md) - Upload-related errors

### Read Later
- [80-how-to-extend.md](80-how-to-extend.md) - Adding new upload data
