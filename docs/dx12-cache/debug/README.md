# Debug Index

Compact symptom lookup with links to detailed day packets.

---

## Quick Symptom Lookup

| Symptom | First Check | Day Packet |
|---------|-------------|------------|
| Exploding triangles | `row_major` declaration, `mul(v,M)` order | [Day1 IP-ExplodingTriangles](../../contracts/day1/Day1_Debug_IssuePacket_ExplodingTriangles.md) |
| Only top face renders | Index buffer winding order | [Day1 Debug Plans](../../contracts/day1/) |
| Cube shifted by floor offset | Floor VS reading Transforms[iid] | [Day1 VisualSpec](../../contracts/day1/Day1_Debug_GroundTruth_VisualSpec.md) |
| Descriptor stomp / flicker | Per-frame SRV slots, fence gating | See `binding-rules.md` |
| GPU hang / TDR | Root param type mismatch | See `binding-rules.md` |
| nvwgf2umx.dll crash | SetGraphicsRootConstantBufferView on TABLE slot | See `binding-rules.md` |
| Wrong face colors | Index buffer face order vs color array | [Day1 fixcube plans](../../contracts/day1/Day1_Debug_plan13-fixcube.md) |
| Z-fighting floor/cubes | Floor VS should NOT read transforms | Fixed in ShaderLibrary.cpp |
| row_major compile fail | Old HLSL syntax issue | [Day1 RuntimeLog](../../contracts/day1/Day1_Debug_RuntimeLog_RowMajorCompileFail.md) |

---

## Proof Toggle Reference

| Toggle | Purpose | Location |
|--------|---------|----------|
| `MICROTEST_MODE` | ByteAddressBuffer + color sentinel diagnostic | `RenderConfig.h` |

---

## Debug Layer Quick Enable

```cpp
#if defined(_DEBUG)
ComPtr<ID3D12Debug> debugController;
if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
{
    debugController->EnableDebugLayer();

    // Optional: GPU-based validation (slower but catches more)
    ComPtr<ID3D12Debug1> debugController1;
    if (SUCCEEDED(debugController.As(&debugController1)))
    {
        debugController1->SetEnableGPUBasedValidation(TRUE);
    }
}
#endif
```

See `pinned-sources.md > Diagnostics` for Debug Layer, GBV, DRED documentation.

---

## Interpreting DRED Output

DRED (Device Removed Extended Data) helps identify GPU crashes.

### Auto-Breadcrumbs
- Shows last N GPU operations before TDR
- Each op is either "completed" or "in-progress" at crash time
- **Focus on**: Last completed op, first in-progress op

```
Breadcrumbs:
  [0] DrawIndexedInstanced     - COMPLETED
  [1] ResourceBarrier          - COMPLETED
  [2] DrawIndexedInstanced     - IN-PROGRESS  <-- Crash here
  [3] Present                  - NOT-STARTED
```

**Interpretation**: Op [2] was executing when GPU hung. Check that draw's:
- Descriptor bindings
- Resource states
- Shader accessing out-of-bounds

### Page Fault Address
- DRED reports the GPU VA that faulted
- Cross-reference with your resources:
```cpp
// Log resource VAs at creation time
OutputDebugStringA(std::format("Resource '{}' GPU VA: 0x{:X}\n",
    name, resource->GetGPUVirtualAddress()).c_str());
```
- Match fault address to resource → likely cause is:
  - Resource was deleted while GPU was reading
  - Descriptor points to freed resource
  - Buffer overrun

---

## GBV Message Patterns

GPU-Based Validation messages indicate shader-time issues.

| Pattern | Meaning | Fix |
|---------|---------|-----|
| `DESCRIPTOR_HEAP_INDEX_OUT_OF_BOUNDS` | Shader indexed past heap end | Check table offset + index < heap size |
| `DESCRIPTOR_UNINITIALIZED` | Shader read slot with no descriptor | Create descriptor before use |
| `RESOURCE_STATE_IMPRECISE` | Resource in wrong state | Add barrier before access |
| `INCOMPATIBLE_RESOURCE_STATE` | Shader access conflicts with state | Transition to correct state |
| `RESOURCE_DATA_STATIC_VIOLATION` | Wrote to DATA_STATIC resource | Use DATA_VOLATILE or update before bind |

### GBV Enable Pattern
```cpp
ComPtr<ID3D12Debug1> debug1;
if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug1)))) {
    debug1->SetEnableGPUBasedValidation(TRUE);
}
// NOTE: GBV has significant perf cost, enable only for debugging
```

### When GBV Doesn't Catch It
GBV misses:
- CPU-side race conditions (fence timing)
- Shader logic bugs
- Cross-queue sync issues
Use PIX GPU capture for these cases.

---

## Day Packet Index

| Day | Focus | Key Packets |
|-----|-------|-------------|
| Day1 | Cube rendering, transforms | [Explore](../../contracts/day1/Day1_Debug_explore.md), [VisualSpec](../../contracts/day1/Day1_Debug_GroundTruth_VisualSpec.md), [ExplodingTriangles](../../contracts/day1/Day1_Debug_IssuePacket_ExplodingTriangles.md) |

---

## Adding New Issues

1. **Create detailed packet**: `docs/contracts/dayX/DayX_Debug_IssuePacket_<Name>.md`
   - Symptom description
   - Investigation steps
   - Root cause analysis
   - Fix applied
   - Verification evidence

2. **Add one-line entry**: Update the symptom table above
   - Symptom (brief)
   - First check (what to look at)
   - Link to packet

3. **Update binding-rules.md**: If issue reveals new DX12 rule
