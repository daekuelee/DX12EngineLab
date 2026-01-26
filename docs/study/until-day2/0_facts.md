# 0. Canonical Facts

**Abstract:** This document establishes the ground truth for all numeric constants, ABI definitions, and structural invariants in the DX12EngineLab codebase through Day 2. Every fact includes a Source-of-Truth (SOT) block for repo hard-facts or Spec Evidence block for DX12 specification facts.

---

## 1. Core Constants

### 1.1 FrameCount = 3

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/FrameContextRing.h`
- Symbol: `FrameContextRing::FrameCount`
- WhatToInspect:
  - Line 38: `static constexpr uint32_t FrameCount = 3`
- Claim: Triple-buffering uses exactly 3 frame contexts.
- WhyItMattersHere: Frame index computed as `frameId % 3`; affects command allocator and upload buffer reuse timing.

**FailureSignature:** If wrong (e.g., assume 2), frame N would stomp frame N-2's allocator before GPU finishes, causing validation errors: "Reset called while command list is in flight".

**FirstPlaceToInspect:** `FrameContextRing.h:38`

---

### 1.2 InstanceCount = 10,000

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/FrameContextRing.h`
- Symbol: `Renderer::InstanceCount`
- WhatToInspect:
  - Line 12: `static constexpr uint32_t InstanceCount = 10000`
- Claim: The cube grid renders exactly 10,000 instances (100x100).
- WhyItMattersHere: Determines transforms buffer size and naive mode draw call count.

**FailureSignature:** If wrong, out-of-bounds SRV read in shader causes GPU hang or garbage transforms.

**FirstPlaceToInspect:** `FrameContextRing.h:12`

---

### 1.3 Upload Allocator Capacity = 1 MB

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/FrameContextRing.cpp`
- Symbol: `ALLOCATOR_CAPACITY`
- WhatToInspect:
  - Line 88: `static constexpr uint64_t ALLOCATOR_CAPACITY = 1 * 1024 * 1024`
- Claim: Each per-frame upload allocator has 1 MB capacity.
- WhyItMattersHere: Must fit frame CB (256 bytes) + transforms (640 KB) with headroom.

**FailureSignature:** If capacity too small, allocation fails or overwrites; debug layer: "Map failed".

**FirstPlaceToInspect:** `FrameContextRing.cpp:88`

---

### 1.4 Transforms Buffer Size = 640 KB

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant, derived)
- File: `Renderer/DX12/FrameContextRing.cpp`
- Symbol: `TRANSFORMS_SIZE`
- WhatToInspect:
  - Line 12: `static constexpr uint64_t TRANSFORMS_SIZE = InstanceCount * sizeof(float) * 16`
  - Calculation: 10,000 * 4 * 16 = 640,000 bytes
- Claim: Transforms buffer is exactly 640,000 bytes (10k matrices @ 64 bytes each).
- WhyItMattersHere: Must fit in upload allocator; defines SRV NumElements.

**FailureSignature:** If SRV NumElements wrong, shader reads garbage past buffer end.

**FirstPlaceToInspect:** `FrameContextRing.cpp:12`

---

### 1.5 Descriptor Heap Capacity = 1024

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `Dx12Context::InitFrameResources()`
- WhatToInspect:
  - Line 280: `m_descRing.Initialize(m_device.Get(), 1024, FrameCount)`
- Claim: Shader-visible CBV/SRV/UAV heap has 1024 descriptors total.
- WhyItMattersHere: Layout: [0,1,2] reserved for per-frame SRVs, [3..1023] dynamic ring.

**FailureSignature:** If capacity exceeded, Allocate() hard-fails; no visible geometry.

**FirstPlaceToInspect:** `Dx12Context.cpp:280`

---

### 1.6 Reserved SRV Slots = [0, 1, 2]

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/FrameContextRing.cpp`
- Symbol: `FrameContextRing::Initialize()`
- WhatToInspect:
  - Line 41-42: `m_frames[i].srvSlot = i` for i in [0, FrameCount)
- Claim: Slots 0, 1, 2 are reserved for per-frame transforms SRVs; one per frame context.
- WhyItMattersHere: Prevents descriptor stomping; each frame has dedicated SRV slot.

**FailureSignature:** If all frames share one slot, stomp_Lifetime bug causes flickering geometry (transforms from wrong frame).

**FirstPlaceToInspect:** `FrameContextRing.cpp:41-42`

---

## 2. Root Signature ABI

### 2.1 Root Parameter Layout

| RP Index | Register | Type | Visibility | Size | Purpose |
|----------|----------|------|------------|------|---------|
| 0 | b0 space0 | Root CBV | VERTEX | 2 DWORDs | ViewProj matrix address |
| 1 | t0 space0 | Descriptor Table | VERTEX | 1 DWORD | Transforms SRV |
| 2 | b1 space0 | Root Constants | VERTEX | 1 DWORD | InstanceOffset |
| 3 | b2 space0 | Root Constants | PIXEL | 4 DWORDs | ColorMode + pad |

**Source-of-Truth**
- EvidenceType: E3 (shader declaration) + E2 (API call site)
- File: `Renderer/DX12/ShaderLibrary.h` (enum) + `shaders/common.hlsli` (registers)
- Symbol: `enum RootParam`, cbuffer declarations
- WhatToInspect:
  - `ShaderLibrary.h:13-18`: enum values 0-3
  - `common.hlsli:17-33`: register bindings
  - `ShaderLibrary.cpp:154-180`: root param construction
- Claim: Root signature has exactly 4 parameters with the layout shown above.
- WhyItMattersHere: Mismatch between CPU binding and shader declaration causes missing data or validation errors.

**FailureSignature:** Wrong RP index causes "Root parameter index out of bounds" or shader reads zero/garbage.

**FirstPlaceToInspect:** `ShaderLibrary.h:13-18`

---

### 2.2 RP_FrameCB = 0

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/ShaderLibrary.h`
- Symbol: `RootParam::RP_FrameCB`
- WhatToInspect:
  - Line 14: `RP_FrameCB = 0`
- Claim: Frame constants (ViewProj) bound at root parameter index 0.
- WhyItMattersHere: `SetGraphicsRootConstantBufferView(0, frameCBAddress)` assumes this.

**FailureSignature:** If RP_FrameCB != 0, ViewProj bound to wrong slot; geometry appears at wrong position or invisible.

**FirstPlaceToInspect:** `ShaderLibrary.h:14`

---

### 2.3 RP_TransformsTable = 1

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/ShaderLibrary.h`
- Symbol: `RootParam::RP_TransformsTable`
- WhatToInspect:
  - Line 15: `RP_TransformsTable = 1`
- Claim: Transforms SRV descriptor table bound at root parameter index 1.
- WhyItMattersHere: `SetGraphicsRootDescriptorTable(1, srvHandle)` assumes this.

**FailureSignature:** If wrong, transforms not visible to shader; cubes render at origin stacked.

**FirstPlaceToInspect:** `ShaderLibrary.h:15`

---

### 2.4 RP_InstanceOffset = 2

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/ShaderLibrary.h`
- Symbol: `RootParam::RP_InstanceOffset`
- WhatToInspect:
  - Line 16: `RP_InstanceOffset = 2`
- Claim: Instance offset root constant bound at index 2.
- WhyItMattersHere: Naive draw mode uses `SetGraphicsRoot32BitConstants(2, 1, &i, 0)`.

**FailureSignature:** If wrong, naive mode reads wrong instance offset; all cubes render at same position.

**FirstPlaceToInspect:** `ShaderLibrary.h:16`

---

### 2.5 RP_DebugCB = 3

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/ShaderLibrary.h`
- Symbol: `RootParam::RP_DebugCB`
- WhatToInspect:
  - Line 17: `RP_DebugCB = 3`
- Claim: Debug constants (ColorMode) bound at root parameter index 3.
- WhyItMattersHere: Color mode toggle affects PS output; wrong slot = wrong coloring.

**FailureSignature:** ColorMode always reads 0; only face-debug coloring visible regardless of toggle.

**FirstPlaceToInspect:** `ShaderLibrary.h:17`

---

## 3. Coordinate System (Repo Hard-Facts)

### 3.1 Handedness = Right-Handed

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant / preprocessor)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `USE_RIGHT_HANDED`
- WhatToInspect:
  - Line 21: `#define USE_RIGHT_HANDED 1`
  - Lines 76-82: Conditional use of `XMMatrixLookAtRH` / `XMMatrixPerspectiveFovRH`
- Claim: View and projection matrices use right-handed coordinate system.
- WhyItMattersHere: Forward vector is +Z at yaw=0; affects camera movement and culling.

**FailureSignature:** If LH matrices used with RH assumptions, scene appears mirrored or inverted.

**FirstPlaceToInspect:** `Dx12Context.cpp:21`

---

### 3.2 Matrix Storage = Row-Major

**Source-of-Truth**
- EvidenceType: E3 (shader declaration)
- File: `shaders/common.hlsli`
- Symbol: `ViewProj`, `TransformData::M`
- WhatToInspect:
  - Line 19: `row_major float4x4 ViewProj`
  - Line 49: `row_major float4x4 M`
- Claim: All matrices in shaders declared row-major, matching DirectXMath output.
- WhyItMattersHere: No transpose needed between CPU and GPU; `mul(vec, mat)` is post-multiply.

**FailureSignature:** If column-major assumed, transforms inverted; geometry appears inside-out or at wrong scale.

**FirstPlaceToInspect:** `common.hlsli:19`

---

### 3.3 Front Face Winding = Clockwise

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `ShaderLibrary::CreatePSO()`
- WhatToInspect:
  - Line 227: `rasterizer.FrontCounterClockwise = FALSE`
- Claim: Clockwise winding is front-facing; CCW triangles are back-faces.
- WhyItMattersHere: `CullMode = BACK` culls CCW triangles; cube face winding must be CW.

**FailureSignature:** If winding wrong, visible faces culled; cube appears hollow or invisible.

**FirstPlaceToInspect:** `ShaderLibrary.cpp:227`

---

## 4. Coordinate System (Spec Facts)

### 4.1 NDC Z Range = [0, 1]

**Spec Evidence**
- Source: MS Learn "Coordinate Systems (Direct3D 12)"
- WhatToInspect:
  - NDC z-coordinate definition for DirectX
  - Contrast with OpenGL [-1, 1] range
- Claim: DirectX 12 NDC z-coordinate ranges from 0 (near plane) to 1 (far plane).
- WhyItMattersHere: Depth buffer cleared to 1.0 (far); LESS comparison passes fragments closer than existing depth.

**FailureSignature:** If assume [-1,1], depth test fails unexpectedly; fragments rejected or z-fighting.

---

### 4.2 CBV Alignment = 256 Bytes

**Spec Evidence**
- Source: MS Learn "Creating a Constant Buffer View"
- WhatToInspect:
  - "Constant buffer views (CBVs) must be 256-byte aligned"
- Claim: CBV address passed to `SetGraphicsRootConstantBufferView` must be 256-byte aligned.
- WhyItMattersHere: Upload allocator enforces this alignment for frame constants.

**Repo Enforcement:**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `CBV_ALIGNMENT`
- WhatToInspect:
  - Line 422: `static constexpr uint64_t CBV_ALIGNMENT = 256`
  - Line 423: CB_SIZE calculation rounds up to 256-byte boundary

**FailureSignature:** Unaligned CBV causes debug layer error: "CBV address must be 256-byte aligned".

---

## 5. Camera Defaults

### 5.1 Initial Position = (0, 180, -220)

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant / initializer)
- File: `Renderer/DX12/Dx12Context.h`
- Symbol: `Dx12Context::FreeCamera::position`
- WhatToInspect:
  - Line 104: `float position[3] = {0.0f, 180.0f, -220.0f}`
- Claim: Camera starts at world position (0, 180, -220).
- WhyItMattersHere: High above and behind the cube grid center; looking toward +Z.

**FailureSignature:** N/A (visual preference), but helps reproduce screenshots/evidence.

**FirstPlaceToInspect:** `Dx12Context.h:104`

---

### 5.2 FOV = pi/4 (45 degrees)

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant / initializer)
- File: `Renderer/DX12/Dx12Context.h`
- Symbol: `Dx12Context::FreeCamera::fovY`
- WhatToInspect:
  - Line 107: `float fovY = 0.785398163f` (XM_PIDIV4)
- Claim: Vertical field of view is 45 degrees (pi/4 radians).
- WhyItMattersHere: Affects projection matrix aspect ratio and visible area.

**FailureSignature:** Wrong FOV causes geometry to appear too zoomed or too wide.

**FirstPlaceToInspect:** `Dx12Context.h:107`

---

### 5.3 Near/Far Planes = 1.0 / 1000.0

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant / initializer)
- File: `Renderer/DX12/Dx12Context.h`
- Symbol: `Dx12Context::FreeCamera::nearZ`, `farZ`
- WhatToInspect:
  - Line 108: `float nearZ = 1.0f`
  - Line 109: `float farZ = 1000.0f`
- Claim: Projection near plane at z=1, far plane at z=1000.
- WhyItMattersHere: Objects closer than 1 unit or farther than 1000 units are clipped.

**FailureSignature:** Near plane too close causes z-fighting; too far causes precision issues.

**FirstPlaceToInspect:** `Dx12Context.h:108-109`

---

### 5.4 Initial Pitch = -0.5 radians (looking down)

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant / initializer)
- File: `Renderer/DX12/Dx12Context.h`
- Symbol: `Dx12Context::FreeCamera::pitch`
- WhatToInspect:
  - Line 106: `float pitch = -0.5f`
- Claim: Camera initially pitched down by 0.5 radians (~28.6 degrees).
- WhyItMattersHere: Looking down toward the cube grid from above.

**FailureSignature:** N/A (visual preference).

**FirstPlaceToInspect:** `Dx12Context.h:106`

---

## 6. Geometry Metrics

### 6.1 Cube = 8 Vertices, 36 Indices

**Source-of-Truth**
- EvidenceType: E4 (data layout proof)
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `RenderScene::CreateCubeGeometry()`
- WhatToInspect:
  - Lines 101-106: 8 vertices defined (front 4, back 4)
  - Lines 108-121: 36 indices defined (6 faces x 2 triangles x 3 indices)
- Claim: Unit cube geometry has 8 vertices and 36 indices (12 triangles).
- WhyItMattersHere: `m_indexCount = 36` determines DrawIndexedInstanced call.

**FailureSignature:** Wrong index count causes incomplete cube or debug layer warning.

**FirstPlaceToInspect:** `RenderScene.cpp:123`

---

### 6.2 Floor = 4 Vertices, 6 Indices

**Source-of-Truth**
- EvidenceType: E4 (data layout proof)
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `RenderScene::CreateFloorGeometry()`
- WhatToInspect:
  - Lines 149-154: 4 vertices at y=-0.01, spanning -200 to +200 in X and Z
  - Lines 156-159: 6 indices (2 triangles)
- Claim: Floor quad is 400x400 units centered at origin, slightly below y=0.
- WhyItMattersHere: Floor provides reference plane beneath cubes.

**FailureSignature:** Wrong floor height causes z-fighting with cube bases.

**FirstPlaceToInspect:** `RenderScene.cpp:161`

---

### 6.3 Vertex Format = R32G32B32_FLOAT (12 bytes)

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `ShaderLibrary::CreatePSO()` input layout
- WhatToInspect:
  - Lines 218-220: Input element desc with `DXGI_FORMAT_R32G32B32_FLOAT`
- Claim: Vertices have only POSITION semantic, 12 bytes per vertex.
- WhyItMattersHere: VBV stride must match; no normals/UVs in vertex data.

**FailureSignature:** Wrong stride causes IA to read garbage positions.

**FirstPlaceToInspect:** `ShaderLibrary.cpp:219`

---

### 6.4 Index Format = R16_UINT (2 bytes)

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/RenderScene.cpp`
- Symbol: `CreateCubeGeometry()` index buffer creation
- WhatToInspect:
  - Line 134: `factory->CreateIndexBuffer(indices, sizeof(indices), DXGI_FORMAT_R16_UINT)`
- Claim: Indices are 16-bit unsigned integers.
- WhyItMattersHere: IBV format must match; max vertex index = 65535.

**FailureSignature:** Wrong format causes triangles to connect wrong vertices.

**FirstPlaceToInspect:** `RenderScene.cpp:134`

---

## 7. Transform Grid Layout

### 7.1 Grid = 100x100, 2-Unit Spacing

**Source-of-Truth**
- EvidenceType: E2 (API call site / loop structure)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `Dx12Context::UpdateTransforms()`
- WhatToInspect:
  - Lines 471-478: Nested loop z in [0,100), x in [0,100)
  - Lines 476-478: Translation `tx = x*2 - 99`, `tz = z*2 - 99`
- Claim: 100x100 grid of cubes, 2 units apart, centered at origin (spanning -99 to +99).
- WhyItMattersHere: Grid center at world origin; camera should look at (0,0,0).

**FailureSignature:** Wrong spacing causes cubes to overlap or have large gaps.

**FirstPlaceToInspect:** `Dx12Context.cpp:471-478`

---

### 7.2 Cube Scale = (0.9, 3.0, 0.9)

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `Dx12Context::UpdateTransforms()`
- WhatToInspect:
  - Lines 490-491: `scaleXZ = 0.9f`, `scaleY = 3.0f`
  - Lines 492-495: Matrix construction with scale on diagonal
- Claim: Cubes are scaled to 0.9 units in X/Z (gap between cubes) and 3.0 in Y (tall boxes).
- WhyItMattersHere: Tall boxes show side faces; scale < spacing creates visible gaps.

**FailureSignature:** If scaleY = 1, cubes are flat and side faces barely visible.

**FirstPlaceToInspect:** `Dx12Context.cpp:490-491`

---

## 8. Depth Buffer

### 8.1 Format = D32_FLOAT

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `Dx12Context::InitDepthBuffer()`
- WhatToInspect:
  - Line 244: `depthDesc.Format = DXGI_FORMAT_D32_FLOAT`
- Claim: Depth buffer uses 32-bit floating point format.
- WhyItMattersHere: High precision reduces z-fighting; matches PSO DSVFormat.

**FailureSignature:** If PSO DSVFormat != depth buffer format, debug layer error.

**FirstPlaceToInspect:** `Dx12Context.cpp:244`

---

### 8.2 Clear Value = 1.0 (Far Plane)

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `Dx12Context::InitDepthBuffer()`
- WhatToInspect:
  - Line 250: `clearValue.DepthStencil.Depth = 1.0f`
- Claim: Depth buffer cleared to 1.0 (far plane in NDC [0,1] convention).
- WhyItMattersHere: LESS comparison passes fragments with depth < 1.0 (closer than far).

**FailureSignature:** If cleared to 0.0, all fragments fail depth test; nothing visible.

**FirstPlaceToInspect:** `Dx12Context.cpp:250`

---

### 8.3 Depth Test = LESS

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/ShaderLibrary.cpp`
- Symbol: `ShaderLibrary::CreatePSO()`
- WhatToInspect:
  - Line 252: `depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS`
- Claim: Fragments pass if their depth is strictly less than existing buffer value.
- WhyItMattersHere: Standard opaque rendering; closer fragments overwrite farther.

**FailureSignature:** Wrong comparison (e.g., GREATER) causes back-to-front rendering or invisible geometry.

**FirstPlaceToInspect:** `ShaderLibrary.cpp:252`

---

## 9. Summary Table

| Fact | Value | File:Line | Evidence Type |
|------|-------|-----------|---------------|
| FrameCount | 3 | FrameContextRing.h:38 | E1 |
| InstanceCount | 10,000 | FrameContextRing.h:12 | E1 |
| Upload Capacity | 1 MB | FrameContextRing.cpp:88 | E1 |
| Transforms Size | 640 KB | FrameContextRing.cpp:12 | E1 |
| Heap Capacity | 1024 | Dx12Context.cpp:280 | E2 |
| Reserved Slots | [0,1,2] | FrameContextRing.cpp:41-42 | E2 |
| RP_FrameCB | 0 | ShaderLibrary.h:14 | E1 |
| RP_TransformsTable | 1 | ShaderLibrary.h:15 | E1 |
| RP_InstanceOffset | 2 | ShaderLibrary.h:16 | E1 |
| RP_DebugCB | 3 | ShaderLibrary.h:17 | E1 |
| Handedness | Right | Dx12Context.cpp:21 | E1 |
| Matrix Storage | Row-Major | common.hlsli:19,49 | E3 |
| Front Winding | CW | ShaderLibrary.cpp:227 | E2 |
| NDC Z Range | [0,1] | DX12 Spec | Spec |
| CBV Alignment | 256 | Dx12Context.cpp:422 + Spec | E1+Spec |
| Camera Position | (0,180,-220) | Dx12Context.h:104 | E1 |
| FOV | pi/4 | Dx12Context.h:107 | E1 |
| Near/Far | 1/1000 | Dx12Context.h:108-109 | E1 |
| Cube Vertices | 8 | RenderScene.cpp:101-106 | E4 |
| Cube Indices | 36 | RenderScene.cpp:108-121 | E4 |
| Floor Vertices | 4 | RenderScene.cpp:149-154 | E4 |
| Floor Indices | 6 | RenderScene.cpp:156-159 | E4 |
| Vertex Stride | 12 | ShaderLibrary.cpp:219 | E2 |
| Index Format | R16_UINT | RenderScene.cpp:134 | E2 |
| Grid Size | 100x100 | Dx12Context.cpp:471-472 | E2 |
| Grid Spacing | 2 units | Dx12Context.cpp:476-478 | E2 |
| Cube Scale | (0.9,3,0.9) | Dx12Context.cpp:490-491 | E2 |
| Depth Format | D32_FLOAT | Dx12Context.cpp:244 | E2 |
| Depth Clear | 1.0 | Dx12Context.cpp:250 | E2 |
| Depth Test | LESS | ShaderLibrary.cpp:252 | E2 |
