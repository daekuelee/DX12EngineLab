# Resource Ownership

This document covers the ResourceRegistry and ResourceStateTracker.

---

## Overview

Two systems work together for resource management:

| System | Responsibility |
|--------|---------------|
| **ResourceRegistry** | Ownership (create/destroy) + handle validation |
| **ResourceStateTracker** | State tracking + barrier emission |

These are separate by design: ownership and state tracking have different lifecycles.

---

## ResourceRegistry

### Purpose

Handle-based resource ownership with generation validation.

### Handle Layout

```
64-bit ResourceHandle:
┌──────────────────────────┬───────────────────────────┬────────────┐
│     Generation (32)      │       Index (24)          │  Type (8)  │
└──────────────────────────┴───────────────────────────┴────────────┘
```

| Component | Purpose |
|-----------|---------|
| Generation | Increments when slot is reused - detects use-after-free |
| Index | Slot in the registry array |
| Type | `ResourceType` enum for validation |

> **Source-of-Truth**: `ResourceHandle` struct in `Renderer/DX12/ResourceRegistry.h`

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

### Usage Pattern

```cpp
// Create
ResourceDesc desc = ResourceDesc::Buffer(size, heap, initialState, "MyBuffer");
ResourceHandle handle = m_registry.Create(desc);

// Use
ID3D12Resource* resource = m_registry.Get(handle);  // Returns nullptr if invalid

// Destroy
m_registry.Destroy(handle);  // Safe to call with invalid handle
```

### Generation Validation

When you call `Get(handle)`:
1. Extract index from handle
2. Compare generation in handle vs. generation in slot
3. If mismatch → return nullptr (handle is stale)

This catches use-after-free bugs at runtime.

> **Source-of-Truth**: `ResourceRegistry::Get()` in `Renderer/DX12/ResourceRegistry.cpp`

---

## ResourceStateTracker

### Purpose

SOLE authority for resource state tracking. All barriers go through this system.

### Key Constraint

> **Rule**: Never emit barriers directly. Always use `ResourceStateTracker::Transition()`.

### API

```cpp
// Register owned resources with known initial state
void Register(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, const char* debugName);

// Track external resources (e.g., swapchain backbuffers)
void AssumeState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);

// Queue a transition (batched)
void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES targetState);

// Emit all queued barriers
void FlushBarriers(ID3D12GraphicsCommandList* cmdList);
```

> **Source-of-Truth**: `ResourceStateTracker` class in `Renderer/DX12/ResourceStateTracker.h`

### Batching Pattern

```cpp
// Queue multiple transitions
m_stateTracker.Transition(resourceA, D3D12_RESOURCE_STATE_COPY_DEST);
m_stateTracker.Transition(resourceB, D3D12_RESOURCE_STATE_COPY_DEST);

// Emit them all at once (single ResourceBarrier call)
m_stateTracker.FlushBarriers(cmdList);
```

Batching is more efficient than individual `ResourceBarrier()` calls.

### Scope

Current implementation: Whole-resource tracking only (no per-subresource).

---

## Transforms Buffer Lifecycle

The transforms buffer demonstrates both systems working together:

### Creation (in FrameContextRing)

```cpp
// Create via ResourceRegistry
ResourceDesc transformsDesc = ResourceDesc::Buffer(
    TRANSFORMS_SIZE,
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_RESOURCE_STATE_COPY_DEST,  // Initial state
    debugName);
ctx.transformsHandle = m_registry->Create(transformsDesc);
```

### Registration (in Dx12Context)

```cpp
// Register with StateTracker
ID3D12Resource* transformsResource = m_resourceRegistry.Get(ctx.transformsHandle);
m_stateTracker.Register(transformsResource, D3D12_RESOURCE_STATE_COPY_DEST, debugName);
```

### Per-Frame Transitions

```cpp
// Before copy
m_stateTracker.Transition(transformsResource, D3D12_RESOURCE_STATE_COPY_DEST);
m_stateTracker.FlushBarriers(cmdList);

// Copy happens here...

// After copy (before shader read)
m_stateTracker.Transition(transformsResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
m_stateTracker.FlushBarriers(cmdList);
```

> **Source-of-Truth**: `Dx12Context::RecordBarriersAndCopy()` in `Renderer/DX12/Dx12Context.cpp`

---

## Backbuffer Handling

Backbuffers are external (swapchain-owned), so we use `AssumeState`:

```cpp
// During initialization
for (UINT i = 0; i < FrameCount; ++i)
{
    m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
    m_stateTracker.AssumeState(m_backBuffers[i].Get(), D3D12_RESOURCE_STATE_PRESENT);
}
```

The `BarrierScope` RAII helper handles backbuffer transitions:

```cpp
// In PassOrchestrator - wraps PRESENT ↔ RENDER_TARGET
BackbufferScope bbScope(cmd, backBuffer);
// ... render commands ...
// Destructor emits RENDER_TARGET → PRESENT
```

> **Source-of-Truth**: `BarrierScope` in `Renderer/DX12/BarrierScope.h`

---

## Common Patterns

### Pattern 1: Create + Register

```cpp
ResourceHandle handle = registry.Create(desc);
ID3D12Resource* raw = registry.Get(handle);
stateTracker.Register(raw, initialState, debugName);
```

### Pattern 2: Transition Before Use

```cpp
stateTracker.Transition(resource, neededState);
stateTracker.FlushBarriers(cmdList);
// Now safe to use resource in neededState
```

### Pattern 3: RAII Scope

```cpp
{
    BarrierScope scope(cmdList, resource, beforeState, afterState);
    // Resource is in afterState here
} // Destructor transitions back to beforeState
```

---

## Failure Modes

### 1. Unregistered Resource

**Symptom**: Assertion or wrong barrier emitted
**Cause**: Calling `Transition()` on unregistered resource
**Fix**: Call `Register()` or `AssumeState()` first

### 2. Stale Handle

**Symptom**: `Get()` returns nullptr unexpectedly
**Cause**: Resource was destroyed, handle is stale
**Fix**: Check `IsValid()` before use, or don't cache handles

### 3. State Mismatch

**Symptom**: D3D12 ERROR about state transition
**Cause**: StateTracker's tracked state doesn't match actual GPU state
**Fix**: Ensure ALL transitions go through StateTracker

### 4. Missing Flush

**Symptom**: Operations happen in wrong state
**Cause**: `Transition()` queues but doesn't emit
**Fix**: Call `FlushBarriers()` before the operation that needs the state

---

## Debug Diagnostics

Enable state tracker diagnostics:

```cpp
m_stateTracker.SetDiagnosticsEnabled(true);
```

This logs every transition to debug output.

---

## Further Reading

| Topic | Document | What to Look For |
|-------|----------|------------------|
| Frame sync | [20-frame-lifecycle.md](20-frame-lifecycle.md) | When resources can be reused |
| Binding | [40-binding-abi.md](40-binding-abi.md) | SRV creation for transforms |
| Debug | [70-debug-playbook.md](70-debug-playbook.md) | Interpreting barrier errors |
| Facts | [_facts.md](_facts.md) | ResourceHandle bit layout |

---

## Study Path

### Read Now
- [40-binding-abi.md](40-binding-abi.md) - How resources are bound

### Read When Broken
- [70-debug-playbook.md](70-debug-playbook.md) - State mismatch errors

### Read Later
- [80-how-to-extend.md](80-how-to-extend.md) - Adding new resource types
