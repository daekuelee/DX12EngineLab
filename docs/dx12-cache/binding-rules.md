# DX12 Binding Rules

Core DX12 descriptor, heap, and root signature contracts. Reference before implementing bindings or debugging GPU crashes.

---

## Descriptor Heap Rules

### Shader-Visible Heaps
- **Only ONE** shader-visible `CBV_SRV_UAV` heap can be bound at a time
- **Only ONE** shader-visible `SAMPLER` heap can be bound at a time
- Call `SetDescriptorHeaps` **before** `SetGraphicsRootDescriptorTable`
- Heap switching is expensive; prefer single large heap with suballocation

### Non-Shader-Visible Heaps
- Used for RTV, DSV, and staging descriptors
- Can have multiple non-shader-visible heaps simultaneously
- Copy descriptors from staging to shader-visible heap as needed

### Descriptor Lifetime
- Descriptor must remain valid from `SetGraphicsRootDescriptorTable` until GPU execution completes
- Per-frame descriptor slots prevent stomp (see `patterns/triple-buffer-ring.md`)

---

## Root Signature Rules

### Parameter Types

| Type | Size (DWORDs) | Use Case |
|------|---------------|----------|
| `32BIT_CONSTANTS` | N (1-64 total) | Small per-draw data (instance offset) |
| `CBV` | 2 | Inline CBV address (no descriptor) |
| `SRV` | 2 | Inline SRV address (raw buffer only) |
| `UAV` | 2 | Inline UAV address (raw buffer only) |
| `DESCRIPTOR_TABLE` | 1 | Pointer to descriptor range in heap |

### Root Signature Limits
- **Max 64 DWORDs** total across all parameters
- Inline descriptors (CBV/SRV/UAV) = 2 DWORDs each
- Tables = 1 DWORD (index into heap)
- Root constants count against 64 DWORD limit

### Common Errors

| Error | Symptom | Fix |
|-------|---------|-----|
| SetGraphicsRootConstantBufferView on TABLE slot | TDR / nvwgf2umx.dll crash | Match call to parameter type |
| SetGraphicsRootDescriptorTable on CBV slot | TDR / GPU hang | Match call to parameter type |
| Wrong root parameter index | Corruption / crash | Use enum constants (RP_FrameCB, etc.) |
| Descriptor table with stale heap | Corruption / crash | SetDescriptorHeaps before table |

---

## Resource State Transitions

### Legacy Barriers (`ResourceBarrier`)

```cpp
D3D12_RESOURCE_BARRIER barrier = {};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barrier.Transition.pResource = resource;
barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
cmdList->ResourceBarrier(1, &barrier);
```

### Common State Transitions

| Before | After | Use Case |
|--------|-------|----------|
| `COPY_DEST` | `*_SHADER_RESOURCE` | After upload to default heap |
| `RENDER_TARGET` | `PRESENT` | Before Present |
| `PRESENT` | `RENDER_TARGET` | Frame start |
| `COMMON` | `COPY_DEST` | Pre-upload (if needed) |

### Enhanced Barriers (Agility SDK)
- More explicit sync points: `D3D12_BARRIER_SYNC`, `D3D12_BARRIER_ACCESS`
- Layout-based: `D3D12_BARRIER_LAYOUT` instead of monolithic states
- See `pinned-sources.md > Enhanced Barriers` for docs

---

## Alignment Requirements

| Resource Type | Alignment | Notes |
|---------------|-----------|-------|
| Constant Buffer | 256 bytes | GPU VA must be 256-byte aligned |
| Structured Buffer element | No strict alignment | But 16-byte elements preferred for perf |
| Texture | 512 bytes (small), 64KB (default) | Use `GetCopyableFootprints` |

### CBV Alignment
- `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT` = 256 bytes
- Buffer size must be multiple of 256 when used as CBV
- GPU virtual address in `SetGraphicsRootConstantBufferView` must be 256-aligned

---

## Descriptor Range Flags

### Recommended Flags

| Flag | When to Use |
|------|-------------|
| `DATA_STATIC` | Data never changes after SetDescriptorTable (textures) |
| `DATA_STATIC_WHILE_SET_AT_EXECUTE` | Data stable during GPU execution (per-frame resources) |
| `DATA_VOLATILE` | Data may change mid-draw (rare) |
| `DESCRIPTORS_VOLATILE` | Descriptors may change after SetDescriptorTable (rare) |

### Project Default
```cpp
srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
```
Per-frame data is stable only during GPU execution window.

---

## Debugging Checklist

When debugging GPU crashes or incorrect rendering:

### 1. Root Parameter Binding
- [ ] Parameter type matches call (CBV vs TABLE vs Constants)
- [ ] Root parameter index matches enum (RP_FrameCB, RP_TransformsTable, RP_InstanceOffset)
- [ ] SetDescriptorHeaps called before SetGraphicsRootDescriptorTable

### 2. Descriptor Validity
- [ ] SRV/CBV slot is per-frame (no stomp across frames)
- [ ] Descriptor created before use (CreateShaderResourceView completed)
- [ ] Heap is shader-visible for tables

### 3. Resource States
- [ ] Transition barriers before read/write
- [ ] StateBefore matches actual current state
- [ ] No missing barriers (use Debug Layer + GBV)

### 4. Fence Synchronization
- [ ] WaitForFence before reusing frame resources
- [ ] Frame index = `frameId % FrameCount`, NOT backbuffer index

### Debug Layer Logging Points
Log these values when debugging:
```cpp
// Log descriptor heap base
OutputDebugStringA("SRV Heap GPU Base: 0x...");

// Log per-frame slot
OutputDebugStringA("Frame SRV Slot: ...");

// Log GPU handle being set
OutputDebugStringA("SetGraphicsRootDescriptorTable GPU Handle: 0x...");

// Log fence values
OutputDebugStringA("Fence Value: ..., Current: ...");
```

---

## Quick Reference: Root Parameter Calls

| Parameter Type | Set Call |
|----------------|----------|
| `CBV` | `SetGraphicsRootConstantBufferView(index, gpuVA)` |
| `SRV` | `SetGraphicsRootShaderResourceView(index, gpuVA)` |
| `UAV` | `SetGraphicsRootUnorderedAccessView(index, gpuVA)` |
| `DESCRIPTOR_TABLE` | `SetGraphicsRootDescriptorTable(index, gpuHandle)` |
| `32BIT_CONSTANTS` | `SetGraphicsRoot32BitConstants(index, count, data, offset)` |

---

## Debug Layer Message Reference

Common message IDs and what they mean. Full list in MS docs.

| Message ID Pattern | Meaning | Fix |
|--------------------|---------|-----|
| `COMMAND_LIST_DRAW_ROOT_SIGNATURE_NOT_SET` | Draw without SetGraphicsRootSignature | Set root sig before draw |
| `COMMAND_LIST_DRAW_VERTEX_BUFFER_NOT_SET` | Draw without vertex buffer bound | IASetVertexBuffers before draw |
| `CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE` | Clear color differs from optimized clear | Use resource's optimized clear value |
| `CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET` | PSO RTV format mismatch | Match PSO RTVFormats to actual RTVs |
| `RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH` | StateBefore doesn't match actual state | Track states correctly or use enhanced barriers |
| `RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS` | Same subresource transitioned twice | Remove duplicate barrier |
| `GPU_BASED_VALIDATION_DESCRIPTOR_HEAP_INDEX_OUT_OF_BOUNDS` | Shader accessed invalid heap index | Check descriptor table offset |
| `GPU_BASED_VALIDATION_DESCRIPTOR_UNINITIALIZED` | Shader accessed uninitialized descriptor | CreateSRV/CBV/UAV before use |
| `GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE` | Resource in wrong state for access | Add barrier before access |

---

## Verification Microtests

### Verify Descriptor is Valid
```cpp
// Create descriptor and immediately verify heap index is in range
UINT descriptorIndex = ...; // your computed index
UINT heapSize = m_srvHeap->GetDesc().NumDescriptors;
assert(descriptorIndex < heapSize && "Descriptor index out of heap bounds");

// Log for debugging
OutputDebugStringA(std::format("Descriptor {} / {} in heap\n",
    descriptorIndex, heapSize).c_str());
```

### Verify Barrier Was Applied
```cpp
// Before draw, log the expected state
OutputDebugStringA(std::format("Resource {} expected in state {}\n",
    resourceName, expectedStateString).c_str());

// After barrier, enable GBV to catch mismatches at runtime
// GBV will report GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE if wrong
```

### Verify Root Parameter Binding
```cpp
// Before SetGraphicsRootDescriptorTable, verify match
// Root param at index N must be DESCRIPTOR_TABLE type
// Log GPU handle being set
D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = ...;
OutputDebugStringA(std::format("SetGraphicsRootDescriptorTable({}, 0x{:X})\n",
    paramIndex, gpuHandle.ptr).c_str());
```
