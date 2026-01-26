# E. Geometry Path Deep Dive

## Abstract

This module traces the complete path from geometry data to rendered pixels. After reading, you will be able to:
1. Compute vertex addresses from buffer and index
2. Explain instanced vs naive draw mechanics
3. Map any draw call to its exact resource bindings

---

## 1. Formal Model / Definitions

### 1.1 Vertex Buffer (Definition)

A **vertex buffer** is a contiguous memory region containing vertex data.

**Properties:**
- Base address (GPU virtual address)
- Stride (bytes per vertex)
- Size (total bytes)

**Address computation:**
```
addr(vertex_k) = VB_base + k * stride
```

### 1.2 Index Buffer (Definition)

An **index buffer** is a contiguous array of integer indices into the vertex buffer.

**Properties:**
- Base address (GPU virtual address)
- Format (R16_UINT or R32_UINT)
- Count (number of indices)

**Triangle list assembly:**
```
triangle_p uses indices [3p, 3p+1, 3p+2]
```

### 1.3 Winding Order (Definition)

**Winding** is the order vertices appear when viewed from a direction:
- **Clockwise (CW):** Vertices proceed clockwise
- **Counter-clockwise (CCW):** Vertices proceed counter-clockwise

Winding determines front-face vs back-face for culling.

---

## 2. Buffer Formats

### 2.1 Vertex Format

```cpp
struct Vertex {
    float x, y, z;  // 12 bytes total
};
```

**Source-of-Truth**
- EvidenceType: E4 (data layout proof)
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `CreateCubeGeometry()`
- WhatToInspect:
  - Lines 99-106: Vertex struct and array
  ```cpp
  struct Vertex { float x, y, z; };
  const Vertex vertices[] = {
      {-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1}, { 1, -1, -1},
      {-1, -1,  1}, {-1,  1,  1}, { 1,  1,  1}, { 1, -1,  1}
  };
  ```
- Claim: 8 vertices for unit cube, 12 bytes each.
- WhyItMattersHere: VBV stride must be 12.

### 2.2 Index Format

```cpp
const uint16_t indices[] = { ... };  // 2 bytes per index
```

Format: `DXGI_FORMAT_R16_UINT`

**Source-of-Truth**
- EvidenceType: E4 (data layout proof)
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `CreateCubeGeometry()`
- WhatToInspect:
  - Lines 108-121: Index array
- Claim: 36 indices (12 triangles x 3) as uint16.
- WhyItMattersHere: IBV format must match.

### 2.3 Cube Geometry

| Property | Value |
|----------|-------|
| Vertices | 8 |
| Indices | 36 |
| Triangles | 12 |
| Faces | 6 |
| Size | 2x2x2 units ([-1,1] each axis) |

**Winding:** CW when viewed from outside (matches `FrontCounterClockwise=FALSE`).

### 2.4 Floor Geometry

| Property | Value |
|----------|-------|
| Vertices | 4 |
| Indices | 6 |
| Triangles | 2 |
| Size | 400x400 units |
| Height | y = -0.01 |

**Source-of-Truth**
- EvidenceType: E4 (data layout proof)
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `CreateFloorGeometry()`
- WhatToInspect:
  - Lines 149-159: Floor vertex and index data
- Claim: Quad at y=-0.01 spanning -200 to +200 in X and Z.
- WhyItMattersHere: Slightly below y=0 prevents z-fighting with cube bases.

---

## 3. IA/Vertex Fetch Mechanics

### 3.1 VB Address Computation

**Definition:**
```
addr(k) = VB_base + k * stride
```

**Example (cube):**
- VB_base = (GPU virtual address from VBV)
- stride = 12 bytes
- Vertex 3: `addr(3) = VB_base + 36`

### 3.2 IB Triangle Assembly

**Definition:** For primitive topology TRIANGLELIST:
```
Triangle p = (index[3p], index[3p+1], index[3p+2])
```

**Example (cube, first triangle):**
```
indices[0..2] = {0, 2, 1}
Triangle 0 = vertices[0], vertices[2], vertices[1]
           = (-1,-1,-1), (1,1,-1), (-1,1,-1)
```

This is the -Z face (front), vertices in CW order when viewed from -Z.

### 3.3 Culling Decision

**Algorithm:**
1. Transform vertices to screen space
2. Compute signed area of triangle
3. Positive area = CCW, Negative = CW
4. If `FrontCounterClockwise=FALSE`: CW = front
5. If `CullMode=BACK`: cull back-faces

**Result:** CCW triangles culled; CW visible.

---

## 4. GeometryFactory Pattern

### 4.1 Synchronous Upload

GeometryFactory creates buffers with immediate GPU synchronization:

```
1. Create DEFAULT buffer (COPY_DEST state)
2. Create UPLOAD buffer
3. Map UPLOAD, copy data, unmap
4. Create command list
5. Record CopyBufferRegion + barrier
6. Close and execute
7. Wait for fence (synchronous)
8. Return VBV/IBV
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/GeometryFactory.cpp`
- Symbol: `UploadBuffer()`
- WhatToInspect:
  - Lines 128-214: Complete upload sequence
  - Lines 207-210: Fence wait
  ```cpp
  m_queue->Signal(m_uploadFence.Get(), m_uploadFenceValue);
  if (m_uploadFence->GetCompletedValue() < m_uploadFenceValue)
  {
      m_uploadFence->SetEventOnCompletion(m_uploadFenceValue, m_uploadFenceEvent);
      WaitForSingleObject(m_uploadFenceEvent, INFINITE);
  }
  ```
- Claim: Init-time upload with synchronous wait.
- WhyItMattersHere: Geometry ready immediately after Initialize().

### 4.2 Why Synchronous?

- Geometry created once at init
- No need for async (not per-frame)
- Simplifies resource lifetime
- Upload buffer can be released after copy

---

## 5. Instancing Mechanics

### 5.1 Instanced Mode

**Draw call:**
```cpp
cmdList->DrawIndexedInstanced(36, 10000, 0, 0, 0);
//                            ↑    ↑
//                         indices instances
```

**Shader behavior:**
- `SV_InstanceID` ranges [0, 9999]
- `InstanceOffset` = 0 (root constant)
- `Transforms[iid + 0]` = per-instance world matrix

**Characteristics:**
- 1 draw call
- 10k instances rendered
- GPU parallelizes across instances

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `RecordDraw()`
- WhatToInspect:
  - Line 62: `cmdList->DrawIndexedInstanced(m_indexCount, instanceCount, 0, 0, 0)`
- Claim: Single draw with instanceCount=10000.
- WhyItMattersHere: Minimal CPU overhead, GPU efficient.

### 5.2 Naive Mode

**Draw calls:**
```cpp
for (uint32_t i = 0; i < 10000; ++i) {
    cmdList->SetGraphicsRoot32BitConstants(2, 1, &i, 0);
    cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
```

**Shader behavior:**
- `SV_InstanceID` = 0 (always, since instanceCount=1)
- `InstanceOffset` = i (root constant)
- `Transforms[0 + i]` = world matrix for cube i

**Characteristics:**
- 10k draw calls
- 10k root constant updates
- High CPU overhead

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `RecordDrawNaive()`
- WhatToInspect:
  - Lines 65-78: Naive draw loop
  ```cpp
  for (uint32_t i = 0; i < instanceCount; ++i) {
      cmdList->SetGraphicsRoot32BitConstants(2, 1, &i, 0);
      cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
  }
  ```
- Claim: Loop issues 10k individual draws.
- WhyItMattersHere: Baseline for measuring instancing benefit.

### 5.3 Shader Abstraction

Both modes use identical shader code:
```hlsl
float4x4 world = Transforms[iid + InstanceOffset].M;
```

| Mode | iid | InstanceOffset | Effective Index |
|------|-----|----------------|-----------------|
| Instanced | 0-9999 | 0 | 0-9999 |
| Naive | 0 | 0-9999 | 0-9999 |

**Result:** Same transforms accessed, different CPU patterns.

---

## 6. Transforms Buffer

### 6.1 Grid Layout

100x100 grid, 2-unit spacing, centered at origin:

```
for z in [0, 100):
    for x in [0, 100):
        idx = z * 100 + x
        tx = x * 2 - 99  // [-99, +99]
        tz = z * 2 - 99  // [-99, +99]
        ty = 0
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `UpdateTransforms()`
- WhatToInspect:
  - Lines 471-478: Grid iteration
- Claim: 100x100 grid centered at origin.
- WhyItMattersHere: Camera at (0, 180, -220) looks at grid center.

### 6.2 Transform Matrix

Row-major 4x4 scale+translate:
```
| scaleXZ   0        0        0  |
| 0         scaleY   0        0  |
| 0         0        scaleXZ  0  |
| tx        ty       tz       1  |
```

Where:
- scaleXZ = 0.9 (gap between cubes)
- scaleY = 3.0 (tall boxes)

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `UpdateTransforms()`
- WhatToInspect:
  - Lines 492-495: Matrix construction
- Claim: Scale and translation only (no rotation).
- WhyItMattersHere: Tall boxes show side faces.

### 6.3 Upload Path

```
CPU:
  uploadArena.Allocate(640KB) → cpuPtr
  Write 10k matrices to cpuPtr

GPU (recorded):
  Barrier: COPY_DEST
  CopyBufferRegion(default, upload)
  Barrier: NON_PIXEL_SHADER_RESOURCE

GPU (execute):
  Copy completes → SRV reads valid data
```

---

## 7. Mechanical Mapping Table

### 7.1 Floor Draw

| Binding | Value | Source |
|---------|-------|--------|
| PSO | floor_pso | `ShaderLibrary.cpp:286` |
| VB | m_floorVbv | `RenderScene.cpp:82` |
| IB | m_floorIbv | `RenderScene.cpp:83` |
| RP0 (b0) | frameCBAddress | `PassOrchestrator.cpp:88` |
| RP1 (t0) | unused by floor_vs | N/A |
| RP2 (b1) | 0 | Not bound for floor |
| RP3 (b2) | ColorMode | For floor_ps |
| IndexCount | 6 | `RenderScene.cpp:161` |
| InstanceCount | 1 | `RenderScene.cpp:86` |

### 7.2 Cubes Draw (Instanced)

| Binding | Value | Source |
|---------|-------|--------|
| PSO | cube_pso | `ShaderLibrary.cpp:272` |
| VB | m_vbv (cube) | `RenderScene.cpp:58` |
| IB | m_ibv (cube) | `RenderScene.cpp:59` |
| RP0 (b0) | frameCBAddress | `PassOrchestrator.cpp:88` |
| RP1 (t0) | srvTableHandle | `PassOrchestrator.cpp:89` |
| RP2 (b1) | 0 | For SV_InstanceID mode |
| RP3 (b2) | ColorMode | `GeometryPass.cpp` |
| IndexCount | 36 | `RenderScene.cpp:123` |
| InstanceCount | 10000 | `GeometryPass.cpp` |

### 7.3 Cubes Draw (Naive, per draw i)

| Binding | Value | Source |
|---------|-------|--------|
| RP2 (b1) | i | `RenderScene.cpp:75` |
| InstanceCount | 1 | `RenderScene.cpp:76` |
| (others same as instanced) | | |

---

## 8. Derived Results

### Lemma 8.1: Instance Index Computation

For any cube at grid position (x, z):
```
instance_index = z * 100 + x
```

For cube drawn in naive mode at iteration i:
```
Transforms[0 + i] = Transforms[i]
```

For cube drawn in instanced mode with SV_InstanceID = iid:
```
Transforms[iid + 0] = Transforms[iid]
```

Both access the same transform.

### Lemma 8.2: Vertex Fetch Latency

Each vertex fetch requires:
1. Index buffer read (2 bytes for R16_UINT)
2. Vertex buffer read (12 bytes at computed address)

For 10k cubes:
- Instanced: 10k * 36 = 360k index fetches
- Naive: Same, but spread across 10k draw calls

GPU schedules fetches efficiently in instanced mode.

---

## 9. Failure Signatures

| Symptom | Likely Cause | Violated Definition |
|---------|--------------|---------------------|
| All cubes at origin | InstanceOffset not set | 5.2 Naive mode |
| Wrong cube colors | Instance ID offset wrong | 5.3 Shader abstraction |
| Missing faces | Winding incorrect | 3.3 Culling |
| Floor z-fighting | y != -0.01 | 2.4 Floor geometry |
| Gaps between cubes wrong | Scale wrong | 6.2 Transform matrix |

---

## 10. Verification by Inspection

### Checklist: Geometry Creation

- [ ] `RenderScene.cpp` creates cube with 8 vertices, 36 indices
- [ ] Vertex struct is 12 bytes (3 floats)
- [ ] Index format is R16_UINT
- [ ] Floor at y = -0.01

### Checklist: Instancing

- [ ] `RecordDraw` uses instanceCount parameter
- [ ] `RecordDrawNaive` sets RP2 in loop
- [ ] Shader uses `iid + InstanceOffset`

### Checklist: Transforms

- [ ] Grid is 100x100
- [ ] Spacing is 2 units
- [ ] Scale is (0.9, 3.0, 0.9)

---

## 11. Further Reading / References

### (A) This Repo
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `RecordDraw()`, `RecordDrawNaive()`
- What to look for: Draw call patterns
- Why it matters HERE: Core instancing comparison

### (B) MS Learn
- Page: "Drawing Primitives"
- What to look for: DrawIndexedInstanced parameters
- Why it matters HERE: API reference

### (C) DirectX-Graphics-Samples
- Repo: microsoft/DirectX-Graphics-Samples
- Sample: D3D12ExecuteIndirect
- What to look for: Advanced instancing
- Why it matters HERE: Next-level optimization

### (D) Real-Time Rendering
- Chapter: Geometry Processing
- What to look for: Vertex cache optimization
- Why it matters HERE: Understanding GPU efficiency

---

## 12. Study Path

**Read Now:**
- Section 2-3 for buffer formats and vertex fetch
- Section 5 for instancing mechanics

**Read When Broken:**
- Section 9 (Failure Signatures) when geometry looks wrong

**Read Later:**
- Section 7 (Mechanical Mapping Table) for complete binding reference
