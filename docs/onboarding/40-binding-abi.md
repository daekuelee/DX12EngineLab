# Binding ABI

This document covers the root signature and descriptor binding mechanics.

---

## Root Signature Layout

The root signature defines the CPU/GPU ABI - how the CPU binds resources for shaders to read.

```cpp
enum RootParam : uint32_t {
    RP_FrameCB = 0,          // b0 space0 - Frame constants (ViewProj)
    RP_TransformsTable = 1,  // t0 space0 - Transforms SRV descriptor table
    RP_InstanceOffset = 2,   // b1 space0 - Instance offset (1 DWORD root constant)
    RP_DebugCB = 3,          // b2 space0 - Debug constants (ColorMode, 4 DWORDs)
    RP_Count
};
```

> **Source-of-Truth**: `enum RootParam` in `Renderer/DX12/ShaderLibrary.h`

### Parameter Details

| RP | Type | Size | GPU Access | Purpose |
|----|------|------|------------|---------|
| 0 | Root CBV | 1 DWORD (GPU VA) | `cbuffer FrameCB : register(b0)` | ViewProj matrix |
| 1 | Descriptor Table | 1 DWORD (offset) | `StructuredBuffer<float4x4> : register(t0)` | 10k transforms |
| 2 | Root Constants | 1 DWORD | `cbuffer InstanceCB : register(b1)` | Instance offset for naive mode |
| 3 | Root Constants | 4 DWORDs | `cbuffer DebugCB : register(b2)` | ColorMode enum |

### Root Signature Cost

Total root signature size: ~7 DWORDs (fits within 64 DWORD limit).

---

## Binding Sequence

In `PassOrchestrator::SetupRenderState()`:

```cpp
// 1. Set root signature
cmd->SetGraphicsRootSignature(shaders->GetRootSignature());

// 2. Set descriptor heap (required for table bindings)
ID3D12DescriptorHeap* heaps[] = { descRing->GetHeap() };
cmd->SetDescriptorHeaps(1, heaps);

// 3. Bind frame CB at RP0
cmd->SetGraphicsRootConstantBufferView(RP_FrameCB, frameCBAddress);

// 4. Bind transforms SRV table at RP1
cmd->SetGraphicsRootDescriptorTable(RP_TransformsTable, srvTableHandle);

// 5. Bind debug constants at RP3
cmd->SetGraphicsRoot32BitConstants(RP_DebugCB, 4, &debugCB, 0);
```

The instance offset (RP2) is set per-draw in naive mode:

```cpp
// For each instance in naive mode
cmd->SetGraphicsRoot32BitConstant(RP_InstanceOffset, instanceIndex, 0);
cmd->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
```

> **Source-of-Truth**: `GeometryPass::Execute()` and `PassOrchestrator` in `Renderer/DX12/`

---

## Descriptor Heap Math

### Reserved Slots (Per-Frame SRVs)

```
Heap Layout:
┌─────────────────────────────────────────────────────────────────┐
│ Slot 0 │ Slot 1 │ Slot 2 │ Slot 3 │ ... │ Slot 1023 │
│ Frame0 │ Frame1 │ Frame2 │ Dynamic ring...                     │
│  SRV   │  SRV   │  SRV   │                                     │
└─────────────────────────────────────────────────────────────────┘
```

Each frame's transforms SRV is at slot `frameIndex`:
- Frame 0 → Slot 0
- Frame 1 → Slot 1
- Frame 2 → Slot 2

> **Source-of-Truth**: `FrameContextRing::CreateSRV()` in `Renderer/DX12/FrameContextRing.cpp`

### GPU Handle Calculation

```cpp
D3D12_GPU_DESCRIPTOR_HANDLE FrameContextRing::GetSrvGpuHandle(uint32_t frameIndex) const
{
    return m_descRing->GetReservedGpuHandle(m_frames[frameIndex].srvSlot);
}
```

For binding:
```cpp
inputs.srvTableHandle = m_frameRing.GetSrvGpuHandle(srvFrameIndex);
```

### Why Per-Frame SRVs?

Each frame has its own transforms buffer (in DEFAULT heap). The SRV must point to the correct buffer. If all frames shared one SRV, we'd have to recreate it every frame.

With per-frame SRVs, creation happens once at init. Binding just uses the right slot.

---

## SRV Creation

The transforms SRV is a StructuredBuffer:

```cpp
D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srvDesc.Format = DXGI_FORMAT_UNKNOWN;  // StructuredBuffer
srvDesc.Buffer.NumElements = InstanceCount;  // 10,000
srvDesc.Buffer.StructureByteStride = sizeof(float) * 16;  // 64 bytes per float4x4
srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
```

> **Source-of-Truth**: `FrameContextRing::CreateSRV()` in `Renderer/DX12/FrameContextRing.cpp`

---

## CBV Binding

Frame constants use inline CBV (root CBV, not descriptor table):

```cpp
cmd->SetGraphicsRootConstantBufferView(RP_FrameCB, frameCBAlloc.gpuVA);
```

Benefits:
- No descriptor allocation needed
- Immediate binding (no table lookup)
- Works well for small, frequently-changing data

Requirements:
- Buffer must be 256-byte aligned (D3D12 requirement)
- GPU virtual address must be valid for the frame

> **Source-of-Truth**: `CBV_ALIGNMENT = 256` in `Renderer/DX12/Dx12Context.cpp`

---

## Shader Side

### Vertex Shader

```hlsl
cbuffer FrameCB : register(b0)
{
    row_major float4x4 ViewProj;
};

StructuredBuffer<float4x4> Transforms : register(t0);

cbuffer InstanceCB : register(b1)
{
    uint InstanceOffset;
};
```

### Usage in VS

```hlsl
uint instanceId = instanceOffset + SV_InstanceID;
float4x4 world = Transforms[instanceId];
float4 worldPos = mul(float4(pos, 1.0), world);
output.pos = mul(worldPos, ViewProj);
```

---

## Common Binding Errors

### 1. Descriptor Heap Not Set

**Error**: `D3D12 ERROR: ... descriptor table ... requires a descriptor heap`
**Cause**: `SetDescriptorHeaps()` not called before `SetGraphicsRootDescriptorTable()`
**Fix**: Ensure heap is bound in `SetupRenderState()`

### 2. Root Parameter Mismatch

**Error**: `D3D12 ERROR: ... root parameter index out of bounds`
**Cause**: Using wrong RP index
**Fix**: Check `enum RootParam` - indices must match root signature creation

### 3. Wrong GPU Handle

**Error**: Renders wrong transforms (visual corruption)
**Cause**: Using wrong frame's SRV slot
**Fix**: Use `frameId % FrameCount` for frame selection, NOT backbuffer index

### 4. CBV Alignment

**Error**: `D3D12 ERROR: ... must be 256-byte aligned`
**Cause**: GPU VA not aligned
**Fix**: Use alignment parameter in `UploadArena::Allocate(size, 256, tag)`

---

## Instanced vs. Naive Mode

### Instanced (1 draw call)

```cpp
// InstanceOffset = 0 for all instances
cmd->SetGraphicsRoot32BitConstant(RP_InstanceOffset, 0, 0);
cmd->DrawIndexedInstanced(indexCount, 10000, 0, 0, 0);
```

Shader uses `SV_InstanceID` directly:
```hlsl
uint instanceId = 0 + SV_InstanceID;  // 0..9999
```

### Naive (10k draw calls)

```cpp
for (uint32_t i = 0; i < 10000; ++i)
{
    cmd->SetGraphicsRoot32BitConstant(RP_InstanceOffset, i, 0);
    cmd->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}
```

Shader uses offset:
```hlsl
uint instanceId = InstanceOffset + SV_InstanceID;  // i + 0 = i
```

Both produce identical visual output; naive mode is much slower.

---

## Further Reading

| Topic | Document | What to Look For |
|-------|----------|------------------|
| Frame sync | [20-frame-lifecycle.md](20-frame-lifecycle.md) | When bindings become valid |
| Upload | [50-uploadarena.md](50-uploadarena.md) | How CB data gets to GPU |
| Resources | [30-resource-ownership.md](30-resource-ownership.md) | SRV resource state requirements |
| Facts | [_facts.md](_facts.md) | Complete RP table |

---

## Study Path

### Read Now
- [50-uploadarena.md](50-uploadarena.md) - Understand upload path

### Read When Broken
- [70-debug-playbook.md](70-debug-playbook.md) - Binding errors

### Read Later
- [80-how-to-extend.md](80-how-to-extend.md) - Adding new root parameters
