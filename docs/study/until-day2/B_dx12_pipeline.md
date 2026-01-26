# B. DX12 Pipeline Mapping

## Abstract

This module formalizes the graphics pipeline as a triangle model of draw validity. After reading, you will be able to:
1. Trace data through each pipeline stage
2. Identify which state affects each stage
3. Explain why a specific PSO configuration produces specific visual results

---

## 1. Formal Model / Definitions

### 1.1 Triangle Model of Draw Validity (Definition)

A draw call is valid if three conditions are met:

```
         ┌─────────────────┐
         │      PSO        │  Fixed-function state + shader bytecode
         │  (immutable)    │
         └────────┬────────┘
                  │
    ┌─────────────┴─────────────┐
    │                           │
┌───┴───┐                   ┌───┴───┐
│ Root  │                   │ Cmd   │
│  Sig  │                   │ List  │
│ (ABI) │                   │(bind) │
└───────┘                   └───────┘
```

**PSO (Pipeline State Object):** Immutable compilation of shaders + fixed-function state.

**Root Signature:** ABI contract defining resource bindings.

**Command List:** Runtime bindings and draw commands.

A draw is valid iff:
1. PSO is set and matches root signature
2. All root parameters declared in root sig are bound
3. Resources are in valid states for their bindings

### 1.2 Pipeline Stages (Definition)

```
┌─────┐   ┌─────┐   ┌─────┐   ┌─────┐   ┌─────┐
│ IA  │ → │ VS  │ → │ RS  │ → │ PS  │ → │ OM  │
└─────┘   └─────┘   └─────┘   └─────┘   └─────┘
  │         │         │         │         │
  │         │         │         │         └─> Depth test, blend, RTV write
  │         │         │         └─> Fragment shading
  │         │         └─> Viewport, culling, scissor
  │         └─> Vertex transformation
  └─> Vertex/index fetch, topology
```

---

## 2. Per-Stage Analysis

### 2.1 Input Assembler (IA)

**Function:** Fetch vertices and indices, assemble primitives.

**This Repo's Configuration:**

| Setting | Value | Source |
|---------|-------|--------|
| Primitive Topology | TRIANGLELIST | `PassOrchestrator.cpp:81` |
| Vertex Format | POSITION (R32G32B32_FLOAT) | `ShaderLibrary.cpp:218-220` |
| Vertex Stride | 12 bytes | `RenderScene.cpp` (view creation) |
| Index Format | R16_UINT | `RenderScene.cpp:134` |

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `CreatePSO()`
- WhatToInspect:
  - Lines 218-220: Input layout definition
  ```cpp
  D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  };
  ```
- Claim: Single input element, position only.
- WhyItMattersHere: No normals/UVs in vertex data; derived in shader.

### 2.2 Vertex Shader (VS)

**Function:** Transform vertices, output clip-space positions.

**This Repo's Implementation:**

1. Fetch world matrix from `Transforms[iid + InstanceOffset]`
2. Transform position: `worldPos = mul(localPos, world)`
3. Apply ViewProj: `clipPos = mul(worldPos, ViewProj)`
4. Compute normal from position (cube-specific)
5. Pass instance ID to PS

**Source-of-Truth**
- EvidenceType: E3 (shader declaration)
- File: `shaders/cube_vs.hlsl`
- Symbol: `VSMain()`
- WhatToInspect:
  - Lines 16-38: Complete vertex shader
- Claim: VS performs world and view-projection transforms.
- WhyItMattersHere: Output SV_Position drives rasterization.

### 2.3 Rasterizer (RS)

**Function:** Viewport transform, triangle setup, culling, interpolation.

**This Repo's Configuration:**

| Setting | Value | Source |
|---------|-------|--------|
| Fill Mode | SOLID | `ShaderLibrary.cpp:225` |
| Cull Mode | BACK (cubes), NONE (floor) | `ShaderLibrary.cpp:226,283` |
| Front Face | Clockwise | `ShaderLibrary.cpp:227` |
| Depth Clip | Enabled | `ShaderLibrary.cpp:231` |

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `CreatePSO()`
- WhatToInspect:
  - Lines 224-231: Rasterizer state
  ```cpp
  rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
  rasterizer.CullMode = D3D12_CULL_MODE_BACK;
  rasterizer.FrontCounterClockwise = FALSE;
  rasterizer.DepthClipEnable = TRUE;
  ```
- Claim: Back-face culling with CW = front.
- WhyItMattersHere: CCW triangles are culled as back-faces.

### 2.4 Pixel Shader (PS)

**Function:** Compute fragment color.

**This Repo's Implementation (3 Modes):**

| Mode | ColorMode Value | Behavior |
|------|-----------------|----------|
| Face Debug | 0 | Color by primitive ID (face index) |
| Instance ID | 1 | Color by instance ID (hue) |
| Lambert | 2 | Simple directional lighting |

**Source-of-Truth**
- EvidenceType: E3 (shader declaration)
- File: `shaders/cube_ps.hlsl`
- Symbol: `PSMain()`
- WhatToInspect:
  - Lines 19-48: Mode dispatch and coloring
- Claim: PS selects coloring based on DebugCB.ColorMode.
- WhyItMattersHere: Toggle via F3 key changes visual output.

### 2.5 Output Merger (OM)

**Function:** Depth test, stencil test, blending, RTV write.

**This Repo's Configuration:**

| Setting | Value | Source |
|---------|-------|--------|
| Depth Enabled | TRUE | `ShaderLibrary.cpp:250` |
| Depth Func | LESS | `ShaderLibrary.cpp:252` |
| Depth Write | ALL | `ShaderLibrary.cpp:251` |
| Stencil | Disabled | `ShaderLibrary.cpp:253` |
| Blending | Disabled | `ShaderLibrary.cpp:237` |

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `CreatePSO()`
- WhatToInspect:
  - Lines 249-253: Depth stencil state
  ```cpp
  depthStencil.DepthEnable = TRUE;
  depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
  depthStencil.StencilEnable = FALSE;
  ```
- Claim: Standard opaque depth test.
- WhyItMattersHere: Closer fragments overwrite farther.

---

## 3. PSO Configuration

### 3.1 PSO Types in This Repo

| PSO | VS | PS | Cull | Depth | Purpose |
|-----|----|----|------|-------|---------|
| Cube | cube_vs | cube_ps | BACK | LESS | 10k instanced cubes |
| Floor | floor_vs | floor_ps | NONE | LESS | Ground plane |
| Marker | marker_vs | marker_ps | NONE | Disabled | NDC corner markers |

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `CreatePSO()`
- WhatToInspect:
  - Lines 271-274: Cube PSO
  - Lines 281-288: Floor PSO
  - Lines 329-356: Marker PSO
- Claim: Three distinct PSOs for different geometry types.
- WhyItMattersHere: Floor needs CULL_NONE due to CCW winding from above.

### 3.2 PSO vs Root Signature

All three PSOs share the same root signature (except markers which use empty root sig).

**Why?** Same binding layout works for all; only shader behavior differs.

---

## 4. Derived Results

### Lemma 4.1: Culling Determination

For a triangle with vertices v0, v1, v2 in clip space:

1. Compute screen-space edge vectors
2. Cross product determines winding in screen space
3. Compare with `FrontCounterClockwise`:
   - If TRUE: CCW is front
   - If FALSE: CW is front
4. If triangle winding != front AND `CullMode` == BACK: cull

**This repo:** CW is front, BACK culled → CCW triangles invisible.

### Lemma 4.2: Depth Test Pass Condition

For fragment at depth z:
```
pass = (z LESS existing_depth)
     = (z < depth_buffer[x,y])
```

Since buffer cleared to 1.0 (far), any fragment with z < 1.0 passes initially.

### Lemma 4.3: SV_InstanceID Semantics

`SV_InstanceID` is:
- System-generated value
- Ranges from 0 to (InstanceCount - 1)
- Does NOT include `StartInstanceLocation` from draw call

**This repo's handling:**
```hlsl
float4x4 world = Transforms[iid + InstanceOffset].M;
```

**Source-of-Truth**
- EvidenceType: E3 (shader declaration)
- File: `shaders/cube_vs.hlsl`
- Symbol: `VSMain()`
- WhatToInspect:
  - Line 19: `Transforms[iid + InstanceOffset].M`
- Claim: InstanceOffset added to support naive mode.
- WhyItMattersHere: In instanced mode, InstanceOffset=0; in naive, varies 0-9999.

---

## 5. Canonical Pipeline Diagram

```
                    ┌─────────────────────────────────────────────┐
                    │              ROOT SIGNATURE                 │
                    │  RP0: b0 (ViewProj)    RP1: t0 (Transforms) │
                    │  RP2: b1 (InstOff)     RP3: b2 (ColorMode)  │
                    └───────────────────┬─────────────────────────┘
                                        │
     ┌──────────────────────────────────┼──────────────────────────────────┐
     │                                  │                                  │
     ▼                                  ▼                                  ▼
┌─────────┐                       ┌──────────┐                       ┌──────────┐
│   IA    │                       │    VS    │                       │    PS    │
│         │                       │          │                       │          │
│ VB:cube │──vertices────────────→│ b0: VP   │                       │ b2: Mode │
│ IB:cube │──indices              │ t0: Xfrm │──SV_Position─────────→│          │
│ TRILIST │──topology             │ b1: Offs │──WorldPos/Normal/IID──│          │
└─────────┘                       └──────────┘                       └────┬─────┘
                                                                          │
     ┌────────────────────────────────────────────────────────────────────┘
     │
     ▼
┌─────────┐                       ┌──────────┐
│   RS    │                       │    OM    │
│         │                       │          │
│ Viewport│                       │ DepthTest│
│ Scissor │──fragments───────────→│ RTV      │──────→ Backbuffer
│ Cull:BK │                       │ DSV      │
└─────────┘                       └──────────┘
```

---

## 6. State Setting Order

### 6.1 Complete Binding Sequence

```cpp
// 1. Set root signature (defines ABI)
cmd->SetGraphicsRootSignature(rootSig);

// 2. Set viewport and scissor (RS state)
cmd->RSSetViewports(1, &viewport);
cmd->RSSetScissorRects(1, &scissor);

// 3. Set render targets (OM state)
cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

// 4. Set primitive topology (IA state)
cmd->IASetPrimitiveTopology(TRIANGLELIST);

// 5. Set descriptor heaps (required before table binding)
cmd->SetDescriptorHeaps(1, heaps);

// 6. Bind root parameters
cmd->SetGraphicsRootConstantBufferView(0, frameCBAddr);  // b0
cmd->SetGraphicsRootDescriptorTable(1, srvHandle);       // t0

// 7. Set vertex/index buffers (per-object)
cmd->IASetVertexBuffers(0, 1, &vbv);
cmd->IASetIndexBuffer(&ibv);

// 8. Set PSO (if changing)
cmd->SetPipelineState(pso);

// 9. Draw
cmd->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/PassOrchestrator.cpp`
- Symbol: `SetupRenderState()`
- WhatToInspect:
  - Lines 66-89: Binding order
- Claim: State set in dependency order.
- WhyItMattersHere: Violating order causes validation errors.

### 6.2 Why Order Matters

| Dependency | Reason |
|------------|--------|
| RootSig before params | Params reference sig slots |
| Heaps before tables | Tables index into bound heap |
| Viewport before draw | RS needs transform params |
| PSO before draw | Draw uses PSO shaders |

---

## 7. Failure Signatures

| Symptom | Stage | Likely Cause |
|---------|-------|--------------|
| All geometry at origin | VS | Transforms not bound |
| Holes in cubes | RS | Wrong cull mode |
| No depth occlusion | OM | DepthEnable FALSE |
| Solid color everywhere | PS | ColorMode wrong |
| Nothing visible | IA | Topology mismatch |

### Debug Layer Messages

| Message | Stage | Fix |
|---------|-------|-----|
| "Input layout doesn't match" | IA | Fix vertex format |
| "Root parameter not set" | VS/PS | Bind missing param |
| "Render target not set" | OM | Call OMSetRenderTargets |

---

## 8. Verification by Inspection

### Checklist: PSO Configuration

- [ ] Input layout matches vertex struct (12 bytes, POSITION only)
- [ ] Root signature matches shader register declarations
- [ ] Rasterizer cull mode matches geometry winding
- [ ] Depth format matches DSV format (D32_FLOAT)

### Checklist: Draw Call Requirements

- [ ] PSO set before draw
- [ ] All root params bound
- [ ] VB/IB set for current geometry
- [ ] Correct instance count

---

## 9. Further Reading / References

### (A) This Repo
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `CreatePSO()`
- What to look for: Complete PSO configuration
- Why it matters HERE: All fixed-function state

### (B) MS Learn
- Page: "Graphics Pipeline"
- What to look for: Stage responsibilities
- Why it matters HERE: Authoritative stage reference

### (C) DirectX-Graphics-Samples
- Repo: microsoft/DirectX-Graphics-Samples
- Sample: D3D12HelloTriangle
- What to look for: Minimal pipeline setup
- Why it matters HERE: Baseline comparison

### (D) GPU Gems
- Chapter: GPU Pipeline
- What to look for: Hardware implementation
- Why it matters HERE: Understanding performance

---

## 10. Study Path

**Read Now:**
- Section 1-2 for triangle model and stage analysis
- Section 6 for binding order

**Read When Broken:**
- Section 7 (Failure Signatures) when geometry doesn't render

**Read Later:**
- Section 4 (Derived Results) for formal lemmas
