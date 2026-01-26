# Debug Playbook

This document provides techniques for diagnosing common issues.

---

## Debug Layer

### Enable

The debug layer is automatically enabled in Debug builds:

```cpp
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif
```

> **Source-of-Truth**: `Dx12Context::InitDevice()` in `Renderer/DX12/Dx12Context.cpp`

### View Output

1. Run Debug build (F5)
2. Open Output window (`View > Output` or `Ctrl+Alt+O`)
3. Select "Debug" in dropdown
4. Look for `D3D12 ERROR:` or `D3D12 WARNING:`

---

## Common Errors and Solutions

### 1. Resource State Mismatch

**Error Message**:
```
D3D12 ERROR: ID3D12CommandList::CopyBufferRegion: The resource state (0x0: D3D12_RESOURCE_STATE_COMMON)
is invalid for the subresource 0 in the transition source resource state (0x2: D3D12_RESOURCE_STATE_COPY_DEST).
```

**Cause**: Resource in wrong state for operation

**Debug Steps**:
1. Find which resource and operation in the error message
2. Check `ResourceStateTracker` - is the resource registered?
3. Verify `Transition()` and `FlushBarriers()` called before operation

**Fix**:
```cpp
// Before copy
m_stateTracker.Transition(resource, D3D12_RESOURCE_STATE_COPY_DEST);
m_stateTracker.FlushBarriers(cmdList);
// Then copy...
```

### 2. Descriptor Heap Not Set

**Error Message**:
```
D3D12 ERROR: ID3D12CommandList::SetGraphicsRootDescriptorTable:
Descriptor heap is not set. A shader-visible descriptor heap must be set.
```

**Cause**: `SetDescriptorHeaps()` not called before table binding

**Fix**:
```cpp
ID3D12DescriptorHeap* heaps[] = { m_descRing.GetHeap() };
cmdList->SetDescriptorHeaps(1, heaps);
// Then SetGraphicsRootDescriptorTable...
```

### 3. CBV Alignment

**Error Message**:
```
D3D12 ERROR: ID3D12CommandList::SetGraphicsRootConstantBufferView:
GPU virtual address 0x... is not 256-byte aligned.
```

**Cause**: CBV address not 256-byte aligned

**Fix**:
```cpp
Allocation cbAlloc = m_uploadArena.Allocate(size, 256, "MyCB");  // alignment = 256
```

### 4. Root Parameter Out of Bounds

**Error Message**:
```
D3D12 ERROR: ID3D12CommandList::SetGraphicsRoot32BitConstant:
Root parameter index is out of bounds.
```

**Cause**: Using RP index that doesn't exist in root signature

**Fix**: Check `enum RootParam` - ensure index < `RP_Count`

### 5. Missing Root Signature

**Error Message**:
```
D3D12 ERROR: ID3D12CommandList::DrawIndexedInstanced:
Root signature has not been set.
```

**Fix**:
```cpp
cmdList->SetGraphicsRootSignature(m_shaderLibrary.GetRootSignature());
```

---

## Diagnostic Toggles

### F1 - Sentinel Instance 0

Moves instance 0 to position (150, 50, 150).

**Use case**: Verify transforms are uploading correctly
- If instance 0 moves → transforms upload working
- If nothing moves → upload or binding issue

### F2 - Stomp Lifetime Test

Uses wrong frame's SRV (N+1 instead of N).

**Use case**: Verify per-frame isolation is working
- If flickering/corruption → proves SRV binding matters
- If no change → something else is wrong

### U - Upload Arena HUD

Shows allocation metrics.

**Expected values**:
- `allocCalls: 2` (FrameCB + Transforms)
- `allocBytes: ~655KB`
- `peakOffset` ≈ `allocBytes`

---

## Debug Output Prefixes

| Prefix | Meaning | Frequency |
|--------|---------|-----------|
| `DIAG:` | Viewport/scissor/draw diagnostics | Once per second |
| `PROOF:` | Frame/binding verification | Once per second |
| `PASS:` | Pass execution info | Once per second |
| `WARNING:` | Diagnostic mode active | When F-key pressed |

### Enable Additional Diagnostics

```cpp
// State tracker diagnostics
m_stateTracker.SetDiagnosticsEnabled(true);

// Upload arena diagnostics
m_uploadArena.Begin(&allocator, true);  // diagEnabled = true
```

---

## Visual Debugging

### Nothing Visible

Checklist:
1. Press G - toggle grid. Is floor visible?
2. Press M - enable markers. Are corner triangles visible?
3. Check camera position (WASD to move)
4. Check Output window for errors

### Wrong Colors

- Press C to cycle color modes
- `FaceDebug` → rainbow by face
- `InstanceID` → hue by instance
- `Lambert` → gray with lighting

### Flickering

- Check F2 toggle - is stomp test active?
- Verify correct frame index for SRV binding
- Check `PROOF:` output - does actual offset match expected?

---

## Memory Debugging

### PIX

1. Install PIX from Microsoft Store
2. Attach to running application
3. Capture a frame
4. Inspect resource contents, descriptor heaps, barrier history

### GPU Validation

For deeper validation (slower):
```cpp
ComPtr<ID3D12Debug1> debug1;
debugController->QueryInterface(IID_PPV_ARGS(&debug1));
debug1->SetEnableGPUBasedValidation(TRUE);
```

---

## Performance Debugging

### CPU Recording Time

Check `EVIDENCE:` log output:
```
EVIDENCE: mode=instanced draws=2 cpu_record_ms=0.XXX frameId=X
```

- Instanced mode: < 1ms typical
- Naive mode: several ms (10k draw calls)

### Toggle Comparison

1. Note visual appearance
2. Press T to toggle draw mode
3. Visual should be identical
4. CPU timing will differ significantly

---

## Fence Debugging

### Detect Fence Issues

Add logging to fence operations:
```cpp
// In WaitForFence
OutputDebugStringA("Waiting for fence value X\n");
// ...
OutputDebugStringA("Fence completed\n");
```

### Common Fence Problems

| Symptom | Likely Cause |
|---------|--------------|
| GPU hang | Missing signal or infinite wait |
| Corruption | Writing before fence completes |
| Slow frame rate | Waiting on wrong fence |

---

## Checklist: New Feature Not Working

1. **Build errors?** Check Output window for compile errors
2. **Debug layer errors?** Check for D3D12 ERROR messages
3. **Visual output?** Try moving camera, toggling visibility
4. **Bindings correct?** Verify root parameter indices
5. **States correct?** Check barrier transitions
6. **Sync correct?** Verify fence wait before resource reuse

---

## Further Reading

| Topic | Document | What to Look For |
|-------|----------|------------------|
| Frame sync | [20-frame-lifecycle.md](20-frame-lifecycle.md) | Fence timing |
| Bindings | [40-binding-abi.md](40-binding-abi.md) | RP indices |
| States | [30-resource-ownership.md](30-resource-ownership.md) | Barrier patterns |
| Facts | [_facts.md](_facts.md) | Magic numbers |

---

## Study Path

### Read Now
- [80-how-to-extend.md](80-how-to-extend.md) - Extension patterns

### Read When Broken
- This document (you're here)
- Search error message in codebase

### Read Later
- [90-exercises.md](90-exercises.md) - Practice debugging
