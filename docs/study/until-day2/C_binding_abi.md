# C. Binding ABI as Formal Contract

## Abstract

This module formalizes the root signature as a contract between CPU and GPU code. After reading, you will be able to:
1. Map any root parameter to its shader register
2. Explain why binding order matters
3. Compute descriptor handle arithmetic

---

## 1. Formal Model / Definitions

### 1.1 Root Signature (Definition)

A **root signature** is a formal ABI (Application Binary Interface) that defines:
1. What resources the shaders expect
2. How those resources are bound (root descriptor, table, or constants)
3. Which shader stages can access each resource

**Analogy:** The root signature is like a function signature for the GPU - it declares the parameters the shaders expect, and the CPU must fulfill this contract.

### 1.2 Shader Register Model (Definition)

HLSL uses registers to identify resources:

| Prefix | Type | Example |
|--------|------|---------|
| `b` | Constant Buffer (CBV) | `register(b0)` |
| `t` | Texture/Buffer (SRV) | `register(t0)` |
| `u` | Unordered Access (UAV) | `register(u0)` |
| `s` | Sampler | `register(s0)` |

**Spaces:** Registers are namespaced by `space`. `register(b0, space0)` and `register(b0, space1)` are different.

This repo uses only `space0`.

### 1.3 Root Parameter Types (Definition)

| Type | Size (DWORDs) | Description |
|------|---------------|-------------|
| Root Descriptor (CBV/SRV/UAV) | 2 | 64-bit GPU virtual address |
| Descriptor Table | 1 | Index into descriptor heap |
| Root Constants | N | Inline N x 32-bit values |

**64 DWORD Limit:** Total root signature size cannot exceed 64 DWORDs. This repo uses far less.

---

## 2. This Repo's Root Signature

### 2.1 Root Parameter Layout

| RP | Type | Register | Space | Visibility | Size | Purpose |
|----|------|----------|-------|------------|------|---------|
| 0 | Root CBV | b0 | space0 | VERTEX | 2 DWORD | ViewProj matrix (64B, but address only) |
| 1 | Desc Table | t0 | space0 | VERTEX | 1 DWORD | Transforms SRV |
| 2 | Constants | b1 | space0 | VERTEX | 1 DWORD | InstanceOffset |
| 3 | Constants | b2 | space0 | PIXEL | 4 DWORD | ColorMode + pad |

**Total:** 2 + 1 + 1 + 4 = **8 DWORDs** (well under 64 limit)

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `CreateRootSignature()`
- WhatToInspect:
  - Lines 153-180: Root parameter array construction
- Claim: Root signature has 4 parameters in the exact layout above.
- WhyItMattersHere: CPU binding calls must use correct indices.

### 2.2 HLSL Declarations (Shader Side)

**Source-of-Truth**
- EvidenceType: E3 (shader declaration)
- File: `shaders/common.hlsli`
- Symbol: cbuffer and SRV declarations
- WhatToInspect:
  - Line 17-20: `cbuffer FrameCB : register(b0, space0) { row_major float4x4 ViewProj; }`
  - Line 23-26: `cbuffer InstanceCB : register(b1, space0) { uint InstanceOffset; }`
  - Line 29-33: `cbuffer DebugCB : register(b2, space0) { uint ColorMode; ... }`
  - `cube_vs.hlsl:6`: `StructuredBuffer<TransformData> Transforms : register(t0, space0)`
- Claim: Shader register declarations match root signature exactly.
- WhyItMattersHere: Mismatch causes shader to read wrong data or zero.

### 2.3 Enum Correspondence

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/ShaderLibrary.h`
- Symbol: `enum RootParam`
- WhatToInspect:
  - Lines 13-18: Enum values 0-3
  ```cpp
  enum RootParam : uint32_t {
      RP_FrameCB = 0,         // b0
      RP_TransformsTable = 1, // t0
      RP_InstanceOffset = 2,  // b1
      RP_DebugCB = 3,         // b2
      RP_Count
  };
  ```
- Claim: Enum provides symbolic names for root param indices.
- WhyItMattersHere: Code uses enum values for `SetGraphicsRoot*` calls.

---

## 3. Descriptor Heap Architecture

### 3.1 Heap Types (Definition)

| Type | Shader Visible | Usage |
|------|----------------|-------|
| CBV_SRV_UAV | Yes | Resources for shaders |
| RTV | No | Render target binding |
| DSV | No | Depth stencil binding |
| Sampler | Yes | Texture samplers |

**Constraint:** Only ONE shader-visible CBV_SRV_UAV heap can be bound at a time.

### 3.2 This Repo's Descriptor Ring

**Layout:**
```
[0]   [1]   [2]   [3]  [4]  ...  [1023]
|--reserved--|   |-----dynamic ring------|
frame0 frame1 frame2
 SRV   SRV   SRV
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `InitFrameResources()`
- WhatToInspect:
  - Line 280: `m_descRing.Initialize(m_device.Get(), 1024, FrameCount)`
  - Parameters: capacity=1024, reservedCount=3
- Claim: 1024 total slots; slots 0-2 reserved for per-frame SRVs.
- WhyItMattersHere: Reserved slots prevent descriptor stomping across frames.

### 3.3 Handle Arithmetic (Definition)

Given:
- `heapStart`: CPU or GPU handle for slot 0
- `descriptorSize`: Device-specific increment (typically 32 bytes on most GPUs)
- `slot`: Target slot index

Handle calculation:
```
handle = heapStart + (slot * descriptorSize)
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/DescriptorRingAllocator.h`
- Symbol: `GetReservedCpuHandle()`, `GetReservedGpuHandle()`
- WhatToInspect:
  - Implementation computes `base + slot * m_descriptorSize`
- Claim: Standard handle arithmetic for slot indexing.
- WhyItMattersHere: Wrong math causes SRV to point to wrong descriptor.

---

## 4. Binding Sequence

### 4.1 Required Order (Theorem)

**Theorem:** `SetDescriptorHeaps` must be called before any `SetGraphicsRootDescriptorTable` call.

**Proof Type:** P1 (API Contract)

**Spec Evidence**
- Source: MS Learn "SetDescriptorHeaps method"
- WhatToInspect:
  - "The heaps being set must have shader visibility"
  - Table bindings reference the currently bound heap
- Claim: GPU handle in descriptor table is validated against bound heap.
- WhyItMattersHere: Table binding before heap binding causes debug layer error.

### 4.2 SetupRenderState Order

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/PassOrchestrator.cpp`
- Symbol: `SetupRenderState()`
- WhatToInspect:
  - Lines 66-89: Complete binding sequence
  ```cpp
  cmd->SetGraphicsRootSignature(...)        // Line 71
  cmd->RSSetViewports(...)                  // Line 74
  cmd->RSSetScissorRects(...)               // Line 75
  cmd->OMSetRenderTargets(...)              // Line 78
  cmd->IASetPrimitiveTopology(...)          // Line 81
  cmd->SetDescriptorHeaps(...)              // Line 84-85
  cmd->SetGraphicsRootConstantBufferView(0, ...) // Line 88
  cmd->SetGraphicsRootDescriptorTable(1, ...)    // Line 89
  ```
- Claim: Heap set before table binding; root sig set first.
- WhyItMattersHere: This order satisfies API requirements.

### 4.3 Per-Draw Bindings

Not all root params need rebinding for every draw:

| Root Param | Rebind per draw? | Reason |
|------------|------------------|--------|
| RP_FrameCB (0) | No | Same ViewProj for all draws |
| RP_TransformsTable (1) | No | Same SRV for all cubes |
| RP_InstanceOffset (2) | Naive: Yes | Different offset per draw |
| RP_DebugCB (3) | No | Same color mode for all |

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `RecordDrawNaive()`
- WhatToInspect:
  - Line 75: `cmdList->SetGraphicsRoot32BitConstants(2, 1, &i, 0)` in loop
- Claim: Only RP_InstanceOffset changes per draw in naive mode.
- WhyItMattersHere: This is why naive mode has 10k API calls.

---

## 5. Derived Results

### Lemma 5.1: Root CBV Address Requirements

**Spec Evidence**
- Source: MS Learn "Constant Buffer View"
- WhatToInspect:
  - CBV address must be 256-byte aligned
- Claim: GPU virtual address for RP_FrameCB must be 256-aligned.
- WhyItMattersHere: Upload allocator enforces this.

**Repo Enforcement:**
- File: `Dx12Context.cpp:422`
- Symbol: `CBV_ALIGNMENT = 256`
- Allocation: `m_uploadArena.Allocate(CB_SIZE, CBV_ALIGNMENT, "FrameCB")` at line 451

### Lemma 5.2: Descriptor Table GPU Handle Validity

**Theorem:** A GPU descriptor handle is valid iff:
1. The heap containing it is currently bound via `SetDescriptorHeaps`
2. The descriptor at that slot has been created via `CreateShaderResourceView` (or similar)
3. The resource backing the view is in a valid state for the view type

**Proof Type:** P1 (API Contract)

**Repo Mapping:**
- Heap binding: `PassOrchestrator.cpp:84-85`
- SRV creation: `FrameContextRing.cpp:107-136` (`CreateSRV`)
- Resource state: Transition to `NON_PIXEL_SHADER_RESOURCE` before draw

### Lemma 5.3: Root Constants Size

Root constants are limited by declaration:
- RP_InstanceOffset: 1 DWORD
- RP_DebugCB: 4 DWORDs

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `ShaderLibrary.cpp`
- Symbol: `CreateRootSignature()`
- WhatToInspect:
  - Line 172: `rootParams[RP_InstanceOffset].Constants.Num32BitValues = 1`
  - Line 179: `rootParams[RP_DebugCB].Constants.Num32BitValues = 4`
- Claim: Constants counts match shader cbuffer sizes.
- WhyItMattersHere: Mismatch causes partial data or validation error.

---

## 6. Repo Mapping

### 6.1 Binding Flow Diagram

```
CPU Side:                                GPU Side:
-----------                              -----------

ShaderLibrary::Initialize()
  |
  +-> CreateRootSignature()              Defines ABI contract
  |     |
  |     +-> D3D12_ROOT_PARAMETER1 array
  |     +-> D3D12SerializeVersionedRootSignature()
  |     +-> device->CreateRootSignature()
  |
  +-> CreatePSO()
        |
        +-> psoDesc.pRootSignature = m_rootSignature
        |
        v
      PSO created with bound root sig


Render() each frame:                     Per-draw state:
  |
  +-> UpdateFrameConstants()             FrameCB in upload heap
  |     |
  |     +-> m_uploadArena.Allocate(CB_SIZE, 256, "FrameCB")
  |     +-> memcpy ViewProj matrix
  |
  +-> SetupRenderState()                 Bindings made to command list
        |
        +-> SetGraphicsRootSignature()    -> Command processor knows ABI
        +-> SetDescriptorHeaps([heap])    -> GPU can resolve table indices
        +-> SetGraphicsRootConstantBufferView(0, gpuVA)
        |     -> b0 now points to ViewProj
        +-> SetGraphicsRootDescriptorTable(1, gpuHandle)
        |     -> t0 now points to Transforms SRV
        |
        v
      Draw calls use bound resources
```

### 6.2 SRV Slot Selection

| Frame ID | frameId % 3 | SRV Slot | GPU Handle Offset |
|----------|-------------|----------|-------------------|
| 0 | 0 | 0 | 0 |
| 1 | 1 | 1 | descriptorSize |
| 2 | 2 | 2 | 2 * descriptorSize |
| 3 | 0 | 0 | 0 |
| ... | ... | ... | ... |

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/FrameContextRing.cpp`
- Symbol: `BeginFrame()`
- WhatToInspect:
  - Line 177: `uint32_t frameIndex = static_cast<uint32_t>(frameId % FrameCount)`
- Claim: SRV slot cycles with frame ID modulo 3.
- WhyItMattersHere: Each frame uses its own SRV to avoid stomping.

---

## 7. Failure Signatures

| Symptom | Likely Cause | Debug Layer Message |
|---------|--------------|---------------------|
| Black screen | Root sig mismatch | "Root signature doesn't match" |
| Geometry at origin | Wrong SRV bound | N/A (reads zeros) |
| Validation error | Table before heap | "Descriptor table not set" |
| Garbage colors | Wrong RP for ColorMode | N/A (reads garbage) |
| Crash on draw | Null root sig | "Pipeline state not set" |

### Specific Debug Layer Messages

| Message | Cause | Fix |
|---------|-------|-----|
| "CBV not 256-byte aligned" | Allocation alignment wrong | Check CBV_ALIGNMENT |
| "Root parameter index out of bounds" | Wrong RP index | Check enum values |
| "Descriptor heap type mismatch" | Wrong heap bound | Check heap creation type |

---

## 8. Verification by Inspection

### Checklist: Root Signature Consistency

- [ ] `ShaderLibrary.h` enum has 4 values (0-3)
- [ ] `ShaderLibrary.cpp` creates 4 root params
- [ ] `common.hlsli` has matching register declarations
- [ ] `PassOrchestrator.cpp` binds all required params
- [ ] `RenderScene.cpp` uses correct RP indices

### Checklist: Descriptor Heap

- [ ] Heap created with `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE`
- [ ] Heap type is `D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV`
- [ ] `SetDescriptorHeaps` called before `SetGraphicsRootDescriptorTable`

---

## 9. Further Reading / References

### (A) This Repo
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `CreateRootSignature()`
- What to look for: Full root param construction
- Why it matters HERE: Canonical root sig definition

### (B) MS Learn
- Page: "Root Signatures Overview"
- What to look for: Root param types, size limits
- Why it matters HERE: Reference for API constraints

### (C) DirectX-Graphics-Samples
- Repo: microsoft/DirectX-Graphics-Samples
- Sample: D3D12DynamicIndexing
- What to look for: Descriptor table usage patterns
- Why it matters HERE: More complex binding scenarios

### (D) PIX Documentation
- Tool: PIX for Windows
- What to look for: Root signature visualization
- Why it matters HERE: Debug binding issues visually

---

## 10. Study Path

**Read Now:**
- Section 1-2 for definitions and this repo's layout
- Section 4 for binding order requirements

**Read When Broken:**
- Section 7 (Failure Signatures) when debug layer errors appear

**Read Later:**
- Section 3 (Descriptor Heap) for allocation details
