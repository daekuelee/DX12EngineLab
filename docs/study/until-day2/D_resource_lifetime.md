# D. Resource Lifetime & Synchronization

## Abstract

This module formalizes resource lifetime as a happens-before analysis. After reading, you will be able to:
1. Identify when a resource can be safely reused
2. Trace fence synchronization through the frame ring
3. Predict resource stomp bugs from incorrect timing

---

## 1. Formal Model / Definitions

### 1.1 Happens-Before Relation (Definition)

Event A **happens-before** event B (written A -> B) if:
1. A completes before B starts, AND
2. There exists a synchronization mechanism guaranteeing this ordering

In DX12, fences provide happens-before guarantees.

### 1.2 GPU Timeline (Definition)

The GPU executes commands asynchronously:

```
CPU:    record_cmd → submit_cmd → ... (continues immediately)
                          ↓
GPU:                  execute_cmd → signal_fence
```

**Critical insight:** CPU `ExecuteCommandLists` returns immediately; GPU execution happens later.

### 1.3 Fence (Definition)

A **fence** is a GPU-CPU synchronization primitive with:
- A monotonically increasing value
- `Signal(fence, value)`: GPU sets fence to value when reached
- `GetCompletedValue()`: Returns highest value GPU has passed
- `WaitForFence(value)`: CPU blocks until `GetCompletedValue() >= value`

### 1.4 Safe Reuse Condition (Definition)

A resource can be safely reused when:
1. All commands referencing it have completed execution
2. The fence value signaled after those commands is <= `GetCompletedValue()`

---

## 2. Heap Types and Usage Patterns

### 2.1 Heap Type Properties

| Heap Type | CPU Access | GPU Access | Use Case |
|-----------|------------|------------|----------|
| DEFAULT | None | Fast R/W | Permanent GPU resources |
| UPLOAD | Write (persistent map) | Slow read | CPU → GPU transfer |
| READBACK | Read | Write | GPU → CPU transfer |

**This repo uses:** UPLOAD for frame constants and transforms staging; DEFAULT for geometry and transforms.

### 2.2 Upload → Default Copy Pattern

```
Frame N:
  1. CPU writes to UPLOAD buffer (linear allocator)
  2. Record CopyBufferRegion to command list
  3. Record barrier: COPY_DEST → NON_PIXEL_SHADER_RESOURCE
  4. Submit command list
  5. GPU executes copy and barrier
  6. Signal fence with value N
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `RecordBarriersAndCopy()`
- WhatToInspect:
  - Lines 504-522: Transition, copy, transition sequence
- Claim: Transforms copied from upload to default each frame.
- WhyItMattersHere: Default buffer holds stable data for GPU reads.

---

## 3. Frame Ring Architecture

### 3.1 Frame Context Structure

Each of the 3 frame contexts contains:

| Resource | Type | Lifecycle |
|----------|------|-----------|
| cmdAllocator | Command Allocator | Reset at frame start |
| uploadAllocator | Linear Allocator | Reset at frame start |
| transformsHandle | Resource Handle | Persistent (DEFAULT heap) |
| srvSlot | uint32_t | Fixed assignment [0,1,2] |
| fenceValue | uint64_t | Updated at frame end |

**Source-of-Truth**
- EvidenceType: E1 (struct definition)
- File: `Renderer/DX12/FrameContextRing.h`
- Symbol: `struct FrameContext`
- WhatToInspect:
  - Lines 15-31: Complete struct
- Claim: Per-frame resources grouped for fence-gated reuse.
- WhyItMattersHere: All resources in a context share the same fence condition.

### 3.2 Frame Ring Invariant (Theorem)

**Invariant:** Frame context selection uses `frameId % FrameCount`, NOT backbuffer index.

**Proof Type:** P2 (Repo Invariant)

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/FrameContextRing.cpp`
- Symbol: `BeginFrame()`
- WhatToInspect:
  - Line 177: `uint32_t frameIndex = static_cast<uint32_t>(frameId % FrameCount)`
- Claim: Frame index derived from monotonic counter, not swapchain.
- WhyItMattersHere: Backbuffer index can repeat non-monotonically; frame ID is stable.

### 3.3 Fence Gating (Theorem)

**Theorem:** A frame context is safe to reuse when its fenceValue has been passed by the GPU.

**Proof:**
1. At frame N end: `Signal(fence, fenceValue)` is queued
2. At frame N+3 start (same context): `WaitForFence(fenceValue)` blocks until GPU signals
3. After wait returns: All frame N commands have completed
4. Therefore: All frame N resources are safe to reuse

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/FrameContextRing.cpp`
- Symbol: `BeginFrame()`, `EndFrame()`
- WhatToInspect:
  - Lines 181-184: Wait for fence if value non-zero
  - Lines 198-206: Signal fence after queue execution
  ```cpp
  // BeginFrame
  if (ctx.fenceValue != 0)
      WaitForFence(ctx.fenceValue);

  // EndFrame
  m_fenceCounter++;
  queue->Signal(m_fence.Get(), m_fenceCounter);
  ctx.fenceValue = m_fenceCounter;
  ```
- Claim: Fence wait guarantees GPU completion before reuse.
- WhyItMattersHere: Without this wait, allocator reset stomps in-flight data.

---

## 4. Timeline Diagrams

### 4.1 Normal Operation (3 Frames in Flight)

```
Time →

Frame 0:  [record]──[submit]────────────────────────────────────────────────
                        ↓
GPU 0:                  [execute]─[signal(1)]
                                       ↓
Frame 1:          [record]──[submit]───────────────────────────────────────
                                ↓
GPU 1:                          [execute]─[signal(2)]
                                               ↓
Frame 2:                  [record]──[submit]───────────────────────────────
                                        ↓
GPU 2:                                  [execute]─[signal(3)]
                                                       ↓
Frame 3:                          [wait(1)]──[record]──[submit]────────────
(reuses ctx 0)                        ↑
                            GPU passed signal(1), safe to reuse
```

### 4.2 Stomp Bug (If No Fence Wait)

```
Time →

Frame 0:  [record]──[submit]────────────────────────────────────────────────
                        ↓
GPU 0:                  [execute──────────────────────]
                                                    ↑
Frame 3:          [reset_allocator]──[record]──[submit]
(reuses ctx 0)         ↑
                  STOMP! GPU still reading allocator memory
```

**FailureSignature:** Flickering geometry, GPU hang, or validation error "command allocator in use".

---

## 5. Resource State Tracking

### 5.1 Resource States (Definition)

Resources have states that determine valid operations:

| State | Valid Operations |
|-------|------------------|
| COPY_DEST | CopyBufferRegion target |
| NON_PIXEL_SHADER_RESOURCE | VS/GS/DS/HS SRV read |
| VERTEX_AND_CONSTANT_BUFFER | IA vertex/index fetch, CBV |
| RENDER_TARGET | RTV write |
| PRESENT | Display to screen |

### 5.2 State Transition Barriers (Definition)

A **barrier** is a GPU command that:
1. Ensures all prior operations complete (synchronization)
2. Transitions resource state (if transition barrier)
3. Invalidates/flushes caches as needed

### 5.3 Barrier State Machine

For transforms buffer:

```
         COPY_DEST
            │
   [CopyBufferRegion]
            │
            ▼
    ┌──────────────┐
    │  Transition  │
    │   Barrier    │
    └──────────────┘
            │
            ▼
 NON_PIXEL_SHADER_RESOURCE
            │
      [VS reads SRV]
            │
            ▼
    ┌──────────────┐
    │  Transition  │  (next frame)
    │   Barrier    │
    └──────────────┘
            │
            ▼
         COPY_DEST
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `RecordBarriersAndCopy()`
- WhatToInspect:
  - Line 510-511: Transition to COPY_DEST
  - Line 520-521: Transition to NON_PIXEL_SHADER_RESOURCE
- Claim: State transitions bracket the copy operation.
- WhyItMattersHere: Reading during copy or copying during read causes corruption.

### 5.4 Backbuffer State Machine

```
       PRESENT
          │
    [BeginFrame]
          ▼
   RENDER_TARGET
          │
   [Draw commands]
          │
     [EndFrame]
          ▼
       PRESENT
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/PassOrchestrator.cpp`
- Symbol: `BackbufferScope` (RAII)
- WhatToInspect:
  - Line 18: `BackbufferScope bbScope(inputs.cmd, inputs.backBuffer)`
- Claim: RAII pattern manages backbuffer transitions.
- WhyItMattersHere: Present with wrong state causes debug layer error.

---

## 6. Descriptor Ring Retirement

### 6.1 Retirement Protocol

```
BeginFrame(completedFenceValue):
  1. Find oldest frame record
  2. If fenceValue <= completedFenceValue:
     - Retire those slots (can be reused)
     - Advance tail pointer

EndFrame(signaledFenceValue):
  1. Create frame record with current allocations
  2. Attach signaledFenceValue to record
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/DescriptorRingAllocator.h`
- Symbol: `BeginFrame()`, `EndFrame()`
- WhatToInspect:
  - Lines 71-78: Method signatures and documentation
- Claim: Descriptor ring uses fence-based retirement.
- WhyItMattersHere: Dynamic descriptors can't be reused until GPU is done.

### 6.2 Reserved vs Dynamic Slots

| Slot Type | Indices | Retirement | Usage |
|-----------|---------|------------|-------|
| Reserved | [0, 1, 2] | Never | Per-frame SRVs |
| Dynamic | [3, 1023] | Fence-based | Transient allocations |

**Why reserved slots don't retire:** Each frame always uses its own dedicated slot; no reuse needed.

---

## 7. Happens-Before Analysis

### 7.1 Safe Reuse: Command Allocator

**Happens-before chain:**
```
Frame N:
  ctx.cmdAllocator->Reset()  → Record commands → Submit → Signal(N)

Frame N+3 (same context):
  WaitForFence(N)  → ctx.cmdAllocator->Reset()
```

**Condition:** `fenceValue >= N` before Reset.

**FailureSignature:** "Cannot reset command allocator while command list is executing".

### 7.2 Safe Reuse: Upload Allocator

**Happens-before chain:**
```
Frame N:
  uploadAllocator.Reset() → Write data → CopyBufferRegion → Signal(N)

Frame N+3:
  WaitForFence(N) → uploadAllocator.Reset()
```

**Condition:** Same as command allocator (shared fence).

**FailureSignature:** GPU reads partial/garbage data during copy.

### 7.3 Safe Reuse: Transforms Buffer

**Note:** Transforms buffer is in DEFAULT heap and persists. Only the DATA changes each frame.

**Happens-before chain:**
```
Frame N:
  CopyBufferRegion (upload → transforms[N%3]) → Barrier → Draw → Signal(N)

Frame N+3:
  WaitForFence(N) → Barrier (SRV → COPY_DEST) → CopyBufferRegion
```

**Condition:** Frame N draw complete before frame N+3 copy starts.

---

## 8. Failure Signatures

| Symptom | Likely Cause | Happens-Before Violation |
|---------|--------------|--------------------------|
| Flickering geometry | SRV stomp | Drawing with stale SRV |
| GPU hang | Barrier missing | Read/write race |
| Validation error | State mismatch | Wrong resource state |
| Corrupted vertices | Upload stomp | Allocator reset too early |
| Black screen | Copy not complete | Draw before copy |

### Debug Layer Messages

| Message | Meaning | Fix |
|---------|---------|-----|
| "Resource in wrong state" | Barrier missing | Add transition barrier |
| "Command allocator in use" | Fence wait missing | Wait before Reset() |
| "Descriptor heap not set" | Binding order wrong | Set heap before table |

---

## 9. Verification by Inspection

### Checklist: Fence Safety

- [ ] `BeginFrame()` waits on `ctx.fenceValue` before any reuse
- [ ] `EndFrame()` signals fence after queue submit
- [ ] Fence value stored in context for next wait
- [ ] `FrameCount = 3` allows 2 frames in flight + 1 recording

### Checklist: State Transitions

- [ ] Transforms: COPY_DEST before copy, SRV after copy
- [ ] Backbuffer: PRESENT at start, RENDER_TARGET for draw, PRESENT at end
- [ ] All barriers use `ResourceBarrier` API

### Checklist: Allocator Reset

- [ ] Command allocator reset only after fence wait
- [ ] Upload allocator reset only after fence wait
- [ ] Both happen at frame start, not end

---

## 10. Further Reading / References

### (A) This Repo
- File: `Renderer/DX12/FrameContextRing.cpp`
- Symbol: `BeginFrame()`, `EndFrame()`
- What to look for: Fence wait and signal
- Why it matters HERE: Core synchronization pattern

### (B) MS Learn
- Page: "Synchronization and Multi-Engine"
- What to look for: Fence semantics, GPU timelines
- Why it matters HERE: Authoritative fence documentation

### (C) DirectX-Graphics-Samples
- Repo: microsoft/DirectX-Graphics-Samples
- Sample: D3D12Multithreading
- What to look for: Multi-frame resource management
- Why it matters HERE: More complex fence patterns

### (D) GPU Gems
- Chapter: Frame Buffering
- What to look for: N-buffering patterns
- Why it matters HERE: Theory behind triple buffering

---

## 11. Study Path

**Read Now:**
- Section 1-3 for formal definitions
- Section 4 for timeline diagrams

**Read When Broken:**
- Section 8 (Failure Signatures) when sync bugs appear

**Read Later:**
- Section 7 (Happens-Before Analysis) for formal reasoning
