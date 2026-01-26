# Frame Lifecycle

This document describes what happens each frame and common failure modes.

---

## Frame Timeline

```
┌────────────────────────────────────────────────────────────────────────────┐
│                           Dx12Context::Render()                             │
├────────────────────────────────────────────────────────────────────────────┤
│ PHASE 1: Setup                                                              │
│   UpdateDeltaTime()                                                         │
│   UpdateCamera(dt)                                                          │
│                                                                             │
│ PHASE 2: Resource Acquisition                                               │
│   FrameContextRing::BeginFrame(frameId)   ←── FENCE WAIT happens here       │
│     └─ uploadAllocator.Reset()            ←── Safe after fence wait         │
│   UploadArena::Begin()                                                      │
│   DescriptorRingAllocator::BeginFrame()   ←── Retires completed frames      │
│                                                                             │
│ PHASE 3: CPU Upload                                                         │
│   UpdateFrameConstants() → UploadArena::Allocate("FrameCB")                 │
│   UpdateTransforms()     → UploadArena::Allocate("Transforms")              │
│                                                                             │
│ PHASE 4: Command Recording                                                  │
│   commandList->Reset()                                                      │
│   ImGuiLayer::BeginFrame()                                                  │
│   RecordBarriersAndCopy()                                                   │
│     └─ Transition → CopyBufferRegion → Transition                          │
│   ImGuiLayer::RenderHUD()                                                   │
│   RecordPasses() via PassOrchestrator::Execute()                           │
│     └─ ClearPass → GeometryPass → ImGuiPass                                │
│   UploadArena::End()                                                        │
│                                                                             │
│ PHASE 5: Execution                                                          │
│   commandList->Close()                                                      │
│   ExecuteCommandLists()                                                     │
│   FrameContextRing::EndFrame()  ←── SIGNAL fence                           │
│   DescriptorRingAllocator::EndFrame()                                       │
│   Present()                                                                 │
│   ++m_frameId                                                               │
└────────────────────────────────────────────────────────────────────────────┘
```

> **Source-of-Truth**: `Dx12Context::Render()` in `Renderer/DX12/Dx12Context.cpp`

---

## Key Synchronization Points

### 1. Frame Context Selection

```cpp
uint32_t frameResourceIndex = static_cast<uint32_t>(m_frameId % FrameCount);
FrameContext& frameCtx = m_frameRing.BeginFrame(m_frameId);
```

**Critical Invariant**: Use `frameId % FrameCount`, NOT `swapChain->GetCurrentBackBufferIndex()`.

Why? Backbuffer index can skip values or repeat. Monotonic frameId gives predictable rotation.

> **Source-of-Truth**: `FrameContextRing::BeginFrame()` in `Renderer/DX12/FrameContextRing.cpp`

### 2. Fence Wait (BeginFrame)

```cpp
// Inside FrameContextRing::BeginFrame()
if (ctx.fenceValue != 0)
{
    WaitForFence(ctx.fenceValue);
}
ctx.cmdAllocator->Reset();
ctx.uploadAllocator.Reset();
```

The fence wait guarantees GPU has finished using this frame's resources. Only then is it safe to:
- Reset the command allocator
- Reset the upload allocator (reuse memory)
- Overwrite transforms buffer

### 3. Fence Signal (EndFrame)

```cpp
// Inside FrameContextRing::EndFrame()
m_fenceCounter++;
queue->Signal(m_fence.Get(), m_fenceCounter);
ctx.fenceValue = m_fenceCounter;
```

Each frame gets a unique fence value. When GPU completes this frame, the fence value becomes "completed."

### 4. Descriptor Ring Retirement

```cpp
uint64_t completedFence = m_frameRing.GetFence()->GetCompletedValue();
m_descRing.BeginFrame(completedFence);
```

The descriptor ring allocator retires frames based on completed fence value, freeing slots for reuse.

---

## Triple Buffering Details

| Frame N | CPU Writing | GPU Reading |
|---------|-------------|-------------|
| 0 | Context 0 | Context 2 (from frame N-1) |
| 1 | Context 1 | Context 0 |
| 2 | Context 2 | Context 1 |
| 3 | Context 0 (after fence wait) | Context 2 |

With 3 frame contexts, CPU can be 2 frames ahead of GPU before blocking.

> **Source-of-Truth**: `FrameContextRing::FrameCount = 3` in `Renderer/DX12/FrameContextRing.h`

---

## Per-Frame Resource Lifecycle

### Upload Allocator

```
Frame 0: [FrameCB: 256B][Transforms: 640KB][............unused............]
         ^offset=0                         ^offset≈656KB

Frame 1: Reset to 0, reuse same 1MB buffer (after fence wait)
```

The allocator is a simple bump pointer. Reset clears the offset, allowing memory reuse.

> **Source-of-Truth**: `FrameLinearAllocator::Reset()` in `Renderer/DX12/FrameLinearAllocator.h`

### Transforms Buffer

```
State transitions per frame:
  COPY_DEST → (copy) → NON_PIXEL_SHADER_RESOURCE → (draw) → [next frame: COPY_DEST]
```

Each frame context has its own transforms buffer in DEFAULT heap. The SRV at reserved slot `frameIndex` always points to that frame's buffer.

---

## Failure Modes

### 1. Use-Before-Wait

**Symptom**: Random corruption, flickering, GPU hang
**Cause**: Writing to upload buffer before fence wait completes
**Debug**: Add assertion in allocator that fence has passed

### 2. Wrong Frame Index

**Symptom**: Cubes render with wrong transforms
**Cause**: Using backbuffer index instead of `frameId % FrameCount`
**Debug**: Press F2 to enable `stomp_Lifetime` toggle - intentionally uses wrong frame to demonstrate

### 3. Barrier Mismatch

**Symptom**: D3D12 ERROR in debug output about resource state
**Cause**: Resource in wrong state for operation
**Debug**: Check `ResourceStateTracker` - is the resource registered? Is the transition correct?

### 4. Descriptor Heap Not Bound

**Symptom**: D3D12 ERROR about descriptor heap
**Cause**: `SetDescriptorHeaps()` not called before draw
**Debug**: Verify heap is bound in `PassOrchestrator::SetupRenderState()`

### 5. Fence Overflow

**Symptom**: GPU hang after many frames
**Cause**: Fence value wrapped (extremely rare with uint64)
**Debug**: Log fence values, check for wrap

---

## Diagnostic Toggles

| Toggle | How to Trigger | What It Tests |
|--------|---------------|---------------|
| F1 (sentinel) | Press F1 | Moves instance 0 - verify transform upload works |
| F2 (stomp) | Press F2 | Uses wrong frame SRV - causes visible corruption |
| U (upload diag) | Press U | Shows allocation metrics in HUD |

---

## Timing Metrics

The engine logs timing once per second (throttled):

```
EVIDENCE: mode=instanced draws=2 cpu_record_ms=0.XXX frameId=X
```

- `cpu_record_ms`: Time spent in command recording (excluding upload)
- Expected: < 1ms for instanced mode, several ms for naive mode

---

## Further Reading

| Topic | Document | What to Look For |
|-------|----------|------------------|
| Resource states | [30-resource-ownership.md](30-resource-ownership.md) | StateTracker barrier batching |
| Upload details | [50-uploadarena.md](50-uploadarena.md) | Allocator capacity, alignment |
| Binding mechanics | [40-binding-abi.md](40-binding-abi.md) | Root parameter binding sequence |
| Debug techniques | [70-debug-playbook.md](70-debug-playbook.md) | Interpreting validation errors |

---

## Study Path

### Read Now
- [30-resource-ownership.md](30-resource-ownership.md) - Understand barrier management

### Read When Broken
- [70-debug-playbook.md](70-debug-playbook.md) - Debug validation errors
- Check `_facts.md` for exact constants

### Read Later
- [50-uploadarena.md](50-uploadarena.md) - Deep dive into upload system
