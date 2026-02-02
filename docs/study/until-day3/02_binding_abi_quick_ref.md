# Binding ABI Quick Reference

## SSOT

Deep details: [until-day2/C_binding_abi.md](../until-day2/C_binding_abi.md)

---

## Contract Checklist (Root Signature & Descriptors)

### A. Root Parameter Layout

| RP | Type | Register | HLSL Usage | Size (DWORD) |
|----|------|----------|------------|--------------|
| 0 | Root CBV | b0 | FrameCB (ViewProj) | 2 |
| 1 | Desc Table | t0 | Transforms SRV | 1 |
| 2 | Root Const | b1 | InstanceOffset | 1 |
| 3 | Root Const | b2 | ColorMode | 4 |

**Total**: 8 DWORDs (well under 64 DWORD limit)

### B. Binding Order (MUST follow)

1. `SetDescriptorHeaps` (required before any table binding)
2. `SetGraphicsRootSignature`
3. `SetGraphicsRootConstantBufferView(0, frameCBAddr)`
4. `SetGraphicsRootDescriptorTable(1, srvHandle)`
5. `SetGraphicsRoot32BitConstants` (as needed for RP2, RP3)

### C. Descriptor Ring Layout

| Slot Range | Usage | Lifetime |
|------------|-------|----------|
| [0, 1, 2] | Per-frame transforms SRV | Persistent (slot = frameIndex) |
| [3] | Character transforms SRV | Persistent |
| [4..1023] | Dynamic ring | Fence-retired |

### D. Per-Draw Rebinding Requirements

| Draw Type | RP0 (FrameCB) | RP1 (Table) | RP2 (Offset) | RP3 (Color) |
|-----------|---------------|-------------|--------------|-------------|
| Cube instanced | ✓ once | ✓ once | 0 | colorMode |
| Cube naive (i) | ✓ once | ✓ once | i | colorMode |
| Floor | ✓ once | - | - | - |
| Character | ✓ once | slot3 | 0 | colorMode |

---

## Code Anchors

| Contract | Anchor |
|----------|--------|
| Root sig creation | `ShaderLibrary.cpp::CreateRootSignature()` |
| RootParam enum | `ShaderLibrary.h::RootParam` |
| Binding sequence | `PassOrchestrator.cpp::SetupRenderState():66-90` |
| SRV slot selection | `Dx12Context.cpp::RecordPasses():674` |
| HLSL registers | `shaders/common.hlsli:17-33` |
| Descriptor ring | `DescriptorRingAllocator.h` |

---

## Failure Linkage

### → Cookbook #10: HUD Shows Wrong Data

**Symptom**: Collision stats don't match visual state.

**Causal chain**: If binding uses wrong SRV slot → reads stale transforms → positions wrong.

**Binding contract violated**: `srvFrameIndex` must match `frameResourceIndex`.

**Check**: Verify slot index passed to `SetGraphicsRootDescriptorTable`.

### → Debug Layer: Root Table Not Set

**Symptom**: D3D12 ERROR in output window.

**Causal chain**: `SetDescriptorHeaps` missing before `SetGraphicsRootDescriptorTable`.

**Binding contract violated**: Heaps must be bound before table bindings.

**Check**: Verify `SetDescriptorHeaps` call precedes table binding.

### → Visual: Objects Disappear or Wrong Position

**Symptom**: Some objects not visible or in wrong place.

**Causal chain**: Wrong descriptor table bound → wrong transform data.

**Binding contract violated**: SRV slot mismatch between upload and bind.

**Check**: Trace srvSlot from upload through to draw call.

---

## Proof Steps

### Docs-Only Verification

```bash
# Verify SetDescriptorHeaps precedes table binding
grep -rn "SetDescriptorHeaps" Renderer/DX12/

# Verify table binding calls
grep -rn "SetGraphicsRootDescriptorTable" Renderer/DX12/

# Verify shader register declarations match
grep -rn "register(b0" shaders/
grep -rn "register(t0" shaders/
```

### Runtime Verification

1. **Debug layer ON**: 0 errors expected in happy path
2. **T key**: Toggle instanced↔naive (RP2 changes, no visual diff expected)
3. **C key**: Cycle color mode (RP3 changes, visual diff expected)
4. **VS Output**: Check for PROOF logs showing `srvOffset=N expected=N`

### PIX Verification (Optional)

1. Take GPU Capture → open Event list
2. Find `SetGraphicsRootDescriptorTable` calls
3. Verify `SetDescriptorHeaps` was called earlier in same command list
4. Inspect descriptor at bound GPU handle → verify points to correct SRV

---

## Quick Debug Decision Tree

```
Symptom: Objects in wrong position / invisible
  │
  ├─ Debug layer errors?
  │   └─ Yes → Read error message (often points to binding issue)
  │
  ├─ T key changes visual?
  │   └─ Yes (shouldn't) → RP2 instanceOffset binding wrong
  │
  ├─ C key changes color?
  │   └─ No (should) → RP3 colorMode binding broken
  │
  └─ Only some objects wrong?
      └─ Yes → Check per-draw rebinding sequence
```

---

## HLSL ↔ C++ Cross-Reference

```hlsl
// shaders/common.hlsli
cbuffer FrameCB : register(b0) { ... }      // ← RP0
StructuredBuffer<float4x4> : register(t0)   // ← RP1
cbuffer InstanceCB : register(b1) { ... }   // ← RP2
cbuffer DebugCB : register(b2) { ... }      // ← RP3
```

```cpp
// ShaderLibrary.h
enum RootParam {
    RP_FrameCB = 0,        // b0
    RP_TransformsTable = 1, // t0
    RP_InstanceOffset = 2,  // b1
    RP_DebugCB = 3          // b2
};
```

---

## See Also

- [C_binding_abi.md](../until-day2/C_binding_abi.md) - Full root signature model
- [04_day3_failure_cookbook.md](../../onboarding/pass/04_day3_failure_cookbook.md) - Failure patterns
