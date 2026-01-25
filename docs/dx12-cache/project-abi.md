# Project ABI - Root Signature Bindings

**SINGLE SOURCE OF TRUTH** for the project's CPU/GPU binding contract.

**Change Protocol**: Update this document FIRST, then code, then verify.

---

## Current Root Signature Layout

Source: `Renderer/DX12/ShaderLibrary.h` (`RootParam` enum)

| Index | Enum | Type | Register | Description |
|-------|------|------|----------|-------------|
| 0 | `RP_FrameCB` | CBV (inline) | b0 space0 | Frame constants (ViewProj matrix) |
| 1 | `RP_TransformsTable` | Descriptor Table | t0 space0 | Transforms SRV (StructuredBuffer) |
| 2 | `RP_InstanceOffset` | 32-bit Constants (1 DWORD) | b1 space0 | Instance offset for naive draw mode |

Total Parameters: 3 (`RP_Count`)

---

## HLSL Register Mapping

### Cube Vertex Shader (`kVertexShader`)
```hlsl
cbuffer FrameCB : register(b0, space0)       // RP_FrameCB
{
    row_major float4x4 ViewProj;
};

cbuffer InstanceCB : register(b1, space0)    // RP_InstanceOffset
{
    uint InstanceOffset;
};

StructuredBuffer<TransformData> Transforms : register(t0, space0);  // RP_TransformsTable
```

### Floor Vertex Shader (`kFloorVertexShader`)
```hlsl
cbuffer FrameCB : register(b0, space0)       // RP_FrameCB only
{
    row_major float4x4 ViewProj;
};
// Does NOT use RP_TransformsTable or RP_InstanceOffset
```

### Marker Shaders
- Use separate root signature (`m_markerRootSignature`)
- No parameters (empty root sig)
- Pass-through VS with NDC vertices

---

## Frame Context Resources

Source: `Renderer/DX12/FrameContextRing.h` (`FrameContext` struct)

| Resource | Type | Heap | Purpose |
|----------|------|------|---------|
| `frameCB` | ID3D12Resource | Upload | ViewProj matrix (256-byte aligned) |
| `frameCBMapped` | void* | - | Persistent CPU mapping |
| `frameCBGpuVA` | D3D12_GPU_VIRTUAL_ADDRESS | - | For SetGraphicsRootConstantBufferView |
| `transformsUpload` | ID3D12Resource | Upload | Instance transforms staging |
| `transformsDefault` | ID3D12Resource | Default | GPU-side transforms (SRV target) |
| `transformsState` | D3D12_RESOURCE_STATES | - | Current barrier state |
| `srvSlot` | uint32_t | - | Per-frame SRV slot in shader-visible heap |

### Frame Constants Layout (b0)
```cpp
struct FrameConstants
{
    DirectX::XMFLOAT4X4 ViewProj;  // 64 bytes, row-major
    // Padding to 256 bytes (CBV alignment)
};
```

### Instance Count
`InstanceCount = 10000` (defined in `FrameContextRing.h`)

---

## Draw Calls

### Cube Instanced Draw
```cpp
cmdList->SetGraphicsRootConstantBufferView(RP_FrameCB, ctx.frameCBGpuVA);
cmdList->SetGraphicsRootDescriptorTable(RP_TransformsTable, srvGpuHandle);
cmdList->SetGraphicsRoot32BitConstants(RP_InstanceOffset, 1, &offset, 0);
cmdList->DrawIndexedInstanced(36, InstanceCount, 0, 0, 0);
```

### Floor Draw
```cpp
// Uses same root signature but only RP_FrameCB
cmdList->SetGraphicsRootConstantBufferView(RP_FrameCB, ctx.frameCBGpuVA);
// RP_TransformsTable and RP_InstanceOffset not used by floor VS
cmdList->DrawIndexed(6, 1, 0, 0, 0);
```

---

## Descriptor Range Flags

```cpp
srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
```

Rationale: Per-frame data is stable only during GPU execution window.

---

## Change Checklist

When modifying the root signature:

1. **Update this document first**
2. Update `RootParam` enum in `ShaderLibrary.h`
3. Update `CreateRootSignature()` in `ShaderLibrary.cpp`
4. Update embedded HLSL register assignments
5. Update all `SetGraphicsRoot*` calls in `Dx12Context.cpp`
6. Verify Debug Layer shows 0 errors
7. Add entry to History table below

---

## History

| Commit | Change | Date |
|--------|--------|------|
| Initial | 3-param layout: FrameCB + TransformsTable + InstanceOffset | Day1 |
