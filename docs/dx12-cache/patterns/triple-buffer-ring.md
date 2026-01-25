# Triple-Buffer Ring Pattern

Frame context isolation pattern for stomp-free per-frame resources.

---

## Overview

The `FrameContextRing` manages per-frame resources with fence-gated reuse. This ensures that CPU writes to frame N resources don't corrupt GPU reads of previous frames.

**Source**: `Renderer/DX12/FrameContextRing.h`

---

## Key Invariant

```cpp
// CORRECT: Use monotonic frame counter modulo FrameCount
uint32_t frameIndex = frameId % FrameContextRing::FrameCount;
FrameContext& ctx = m_frames[frameIndex];

// WRONG: Do NOT use backbuffer index
uint32_t backbufferIndex = m_swapChain->GetCurrentBackBufferIndex(); // NO!
```

### Why Not Backbuffer Index?

| Aspect | Frame Index | Backbuffer Index |
|--------|-------------|------------------|
| Source | Monotonic counter | Swap chain state |
| Control | Application-controlled | Driver-controlled |
| Predictable | Yes | No (may skip indices) |
| Purpose | Frame resource isolation | Present target selection |

The swap chain may not cycle backbuffers predictably (depends on present mode, driver behavior). Using backbuffer index for frame resources can cause:
- Resource stomp (CPU overwrites data GPU is reading)
- Fence value mismatch (waiting on wrong fence)
- Flicker or corruption

---

## Frame Context Structure

```cpp
struct FrameContext
{
    ComPtr<ID3D12CommandAllocator> allocator;  // Per-frame command allocator
    uint64_t fenceValue = 0;                    // Fence value when submitted

    // Frame constant buffer (ViewProj)
    ComPtr<ID3D12Resource> frameCB;
    void* frameCBMapped = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS frameCBGpuVA = 0;

    // Transforms buffer (upload + default heap)
    ComPtr<ID3D12Resource> transformsUpload;
    void* transformsUploadMapped = nullptr;
    ComPtr<ID3D12Resource> transformsDefault;
    D3D12_RESOURCE_STATES transformsState = D3D12_RESOURCE_STATE_COPY_DEST;

    // Per-frame SRV slot in shader-visible heap
    uint32_t srvSlot = 0;
};
```

---

## Ring Buffer Lifecycle

### Initialization
```
Frame 0: Create resources, assign SRV slot 0
Frame 1: Create resources, assign SRV slot 1
Frame 2: Create resources, assign SRV slot 2
```

### Per-Frame Flow
```
BeginFrame(frameId):
  1. Compute frameIndex = frameId % FrameCount
  2. Wait for ctx.fenceValue if GPU still using
  3. Reset command allocator
  4. Return ctx reference

[Render loop uses ctx resources]

EndFrame(queue, ctx):
  1. Signal fence with new value
  2. Store fence value in ctx.fenceValue
```

### Shutdown
```
WaitForAll():
  Wait for all frames to complete before destroying resources
```

---

## Per-Frame SRV Slot Allocation

To prevent descriptor stomp, each frame has its own SRV slot:

```cpp
// During initialization
for (uint32_t i = 0; i < FrameCount; ++i)
{
    m_frames[i].srvSlot = baseSrvSlot + i;  // Unique slot per frame
}

// During render
D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = GetSrvGpuHandle(frameIndex);
cmdList->SetGraphicsRootDescriptorTable(RP_TransformsTable, srvGpuHandle);
```

### Why Per-Frame Slots?

| Approach | Risk |
|----------|------|
| Single shared slot | Frame N+1 overwrites descriptor while GPU reads frame N |
| Per-frame slots | Each frame reads its own descriptor, no stomp |

---

## Fence Synchronization

```cpp
void FrameContextRing::WaitForFence(uint64_t value)
{
    if (m_fence->GetCompletedValue() < value)
    {
        m_fence->SetEventOnCompletion(value, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}
```

### Fence Value Sequence
```
Frame 0: Submit, signal fence=1 → ctx[0].fenceValue = 1
Frame 1: Submit, signal fence=2 → ctx[1].fenceValue = 2
Frame 2: Submit, signal fence=3 → ctx[2].fenceValue = 3
Frame 3: Wait for fence >= 1 (ctx[0]), reset, submit, signal fence=4
```

---

## Resource State Tracking

Each frame context tracks its own resource states:

```cpp
// Per-frame state to fix barrier mismatch
D3D12_RESOURCE_STATES transformsState = D3D12_RESOURCE_STATE_COPY_DEST;

// Transition only when needed
if (ctx.transformsState != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
{
    // Barrier from current state to SRV
    ctx.transformsState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}
```

---

## Integration Points

### Adding New Per-Frame Resource

1. Add resource member to `FrameContext`
2. Create resource in `CreatePerFrameBuffers()`
3. Create descriptor in `CreateSRV()` (if applicable)
4. Access via `ctx.resourceName` in render loop
5. Update `project-abi.md` if affects root signature

### Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Using backbuffer index for frame resources | Corruption, TDR | Use `frameId % FrameCount` |
| Single SRV slot for all frames | Flicker, wrong transforms | Per-frame SRV slots |
| Missing fence wait before reset | `COMMAND_ALLOCATOR_SYNC` error | Wait in `BeginFrame` |
| Wrong barrier state tracking | `RESOURCE_STATE` debug error | Track per-frame state |

---

## Constants

```cpp
static constexpr uint32_t FrameCount = 3;      // Triple buffering
static constexpr uint32_t InstanceCount = 10000; // Transform count
```
