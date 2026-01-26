# A. Rendering Math & Coordinate Spaces

## Abstract

This module proves understanding of the coordinate space transformation pipeline from object space to screen pixels. After reading, you will be able to:
1. Trace any point through Object -> World -> View -> Clip -> NDC -> Screen
2. Derive why specific matrix conventions are used
3. Predict visual bugs from convention mismatches

---

## 1. Formal Model / Definitions

### 1.1 Coordinate Spaces (Definition)

| Space | Origin | Axes | Units |
|-------|--------|------|-------|
| **Object** | Mesh center | Local to mesh | Model units |
| **World** | Scene origin | Right-handed: +X right, +Y up, +Z forward | World units |
| **View** | Camera eye | +X right, +Y up, -Z into scene (RH) | World units |
| **Clip** | N/A | Homogeneous (x,y,z,w) | Pre-divide |
| **NDC** | Screen center | X,Y in [-1,1], Z in [0,1] (DX12) | Normalized |
| **Screen** | Top-left | +X right, +Y down | Pixels |

### 1.2 Homogeneous Coordinates (Definition)

A 3D point `p = (x, y, z)` is represented in homogeneous coordinates as `p_h = (x, y, z, 1)`.

A 4x4 transformation matrix `M` transforms homogeneous points via multiplication.

**Key property:** After perspective projection, `w != 1`. The perspective divide `(x/w, y/w, z/w)` produces NDC coordinates.

### 1.3 Matrix Multiplication Convention (Definition)

This repo uses **row-vector * matrix** (post-multiply) convention:

```
p_out = p_in * M
```

Where `p_in` is a 1x4 row vector and `M` is a 4x4 matrix.

**Source-of-Truth**
- EvidenceType: E3 (shader declaration)
- File: `shaders/cube_vs.hlsl`
- Symbol: `VSMain()`
- WhatToInspect:
  - Line 20: `float3 worldPos = mul(float4(vin.Pos, 1.0), world).xyz`
  - Line 21: `o.Pos = mul(float4(worldPos, 1.0), ViewProj)`
- Claim: `mul(vec, mat)` is used, consistent with row-vector convention.
- WhyItMattersHere: DirectXMath produces row-major matrices; shader must consume them the same way.

### 1.4 Storage Convention (Definition)

**Row-major storage:** Matrix element `M[row][col]` is stored at memory offset `row * 4 + col`.

In row-major storage, a 4x4 matrix looks like:
```
Memory: [m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33]
        [-----row 0------] [-----row 1------] [-----row 2------] [-----row 3------]
```

For a row-vector * matrix multiplication, the translation components are in the **last row** (row 3): `[tx, ty, tz, 1]`.

**Source-of-Truth**
- EvidenceType: E3 (shader declaration)
- File: `shaders/common.hlsli`
- Symbol: `ViewProj`, `TransformData::M`
- WhatToInspect:
  - Line 19: `row_major float4x4 ViewProj`
  - Line 49: `row_major float4x4 M`
- Claim: HLSL explicitly declares `row_major`, matching DirectXMath output.
- WhyItMattersHere: Without `row_major`, HLSL defaults to column-major, causing silent transposition bugs.

---

## 2. Transformation Pipeline

### 2.1 Object to World (Model Matrix)

**Definition:** The world matrix `W` transforms object-space positions to world-space.

For this repo, the world matrix encodes **scale + translation** (no rotation):

```
W = | sx   0    0    0  |
    | 0    sy   0    0  |
    | 0    0    sz   0  |
    | tx   ty   tz   1  |
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `Dx12Context::UpdateTransforms()`
- WhatToInspect:
  - Lines 492-495: Matrix construction
  ```cpp
  transforms[idx * 16 + 0] = scaleXZ;  // m00
  transforms[idx * 16 + 5] = scaleY;   // m11
  transforms[idx * 16 + 10] = scaleXZ; // m22
  transforms[idx * 16 + 12] = tx;      // m30
  transforms[idx * 16 + 13] = ty;      // m31
  transforms[idx * 16 + 14] = tz;      // m32
  transforms[idx * 16 + 15] = 1.0f;    // m33
  ```
- Claim: Translation in row 3 (indices 12,13,14) matches row-major, row-vector convention.
- WhyItMattersHere: Placing translation in wrong location causes cubes to appear at wrong positions.

### 2.2 World to View (View Matrix)

**Definition:** The view matrix `V` transforms world-space to camera-space (view-space).

**Construction:** `XMMatrixLookAtRH(eye, target, up)`

This produces a matrix that:
1. Translates world so camera is at origin
2. Rotates so camera looks along -Z (right-handed)

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `BuildFreeCameraViewProj()`
- WhatToInspect:
  - Line 77: `XMMATRIX view = XMMatrixLookAtRH(eye, target, up)`
- Claim: Right-handed view matrix; camera looks along -Z in view space.
- WhyItMattersHere: Matching handedness ensures correct forward direction.

### 2.3 View to Clip (Projection Matrix)

**Definition:** The projection matrix `P` transforms view-space to clip-space, encoding perspective.

**Construction:** `XMMatrixPerspectiveFovRH(fovY, aspect, near, far)`

For right-handed coordinate system:

```
P = | f/aspect   0       0              0       |
    | 0          f       0              0       |
    | 0          0       far/(near-far) -1      |
    | 0          0       near*far/(near-far) 0  |
```

Where `f = 1 / tan(fovY / 2)`.

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `BuildFreeCameraViewProj()`
- WhatToInspect:
  - Line 78: `XMMATRIX proj = XMMatrixPerspectiveFovRH(cam.fovY, aspect, cam.nearZ, cam.farZ)`
- Claim: Right-handed perspective projection with vertical FOV.
- WhyItMattersHere: Determines visible frustum and depth mapping.

### 2.4 ViewProj Combination

**Definition:** `ViewProj = V * P` (matrix multiplication, not element-wise).

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `BuildFreeCameraViewProj()`
- WhatToInspect:
  - Line 84: `return XMMatrixMultiply(view, proj)`
- Claim: View and projection are composed in that order.
- WhyItMattersHere: Order matters; V*P transforms world->view->clip.

### 2.5 Clip to NDC (Perspective Divide)

**Definition:** Given clip coordinates `(x_c, y_c, z_c, w_c)`, NDC coordinates are:

```
x_ndc = x_c / w_c
y_ndc = y_c / w_c
z_ndc = z_c / w_c
```

**Spec Evidence**
- Source: MS Learn "Coordinate Systems (Direct3D 12)"
- WhatToInspect:
  - Perspective divide operation
  - NDC ranges: X,Y in [-1,1], Z in [0,1]
- Claim: GPU performs perspective divide after vertex shader; z_ndc in [0,1] for DirectX.
- WhyItMattersHere: Z=0 is near plane, Z=1 is far plane; depth test uses this.

### 2.6 NDC to Screen (Viewport Transform)

**Definition:** Given NDC `(x_ndc, y_ndc, z_ndc)` and viewport `(X, Y, W, H, MinZ, MaxZ)`:

```
x_screen = X + (x_ndc + 1) * W / 2
y_screen = Y + (1 - y_ndc) * H / 2
z_depth = MinZ + z_ndc * (MaxZ - MinZ)
```

Note: Y is flipped (NDC +Y is up, screen +Y is down).

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `Dx12Context::Initialize()`
- WhatToInspect:
  - Line 406: `m_viewport = { 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f }`
- Claim: Viewport covers entire window, depth range [0,1].
- WhyItMattersHere: MinZ=0, MaxZ=1 means z_depth = z_ndc directly.

---

## 3. Derived Results

### Lemma 3.1: Transform Chain for Cube Vertex

Given:
- Object position `p_obj = (x, y, z, 1)`
- World matrix `W` (from transforms buffer)
- ViewProj matrix `VP` (from frame constant buffer)

The clip position is:
```
p_clip = p_obj * W * VP
```

Or equivalently (associativity):
```
p_world = p_obj * W
p_clip = p_world * VP
```

**Proof Type:** P3 (Dataflow)

**Shader Evidence:**
```hlsl
// cube_vs.hlsl lines 19-21
float4x4 world = Transforms[iid + InstanceOffset].M;
float3 worldPos = mul(float4(vin.Pos, 1.0), world).xyz;
o.Pos = mul(float4(worldPos, 1.0), ViewProj);
```

### Lemma 3.2: Clipping Preconditions

For a point to be visible after perspective divide:
1. `w_c > 0` (point is in front of camera)
2. `-w_c <= x_c <= w_c` (X within frustum)
3. `-w_c <= y_c <= w_c` (Y within frustum)
4. `0 <= z_c <= w_c` (Z within frustum, DirectX convention)

**Spec Evidence**
- Source: MS Learn "View Clipping"
- WhatToInspect:
  - Clip space bounds for DirectX
- Claim: Points outside these bounds are clipped by the rasterizer.
- WhyItMattersHere: Explains why geometry behind camera or outside frustum disappears.

### Lemma 3.3: Forward Vector Formula

For a camera with yaw (rotation around Y) and pitch (rotation around X):

```
forward = (sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch))
```

At yaw=0, pitch=0: forward = (0, 0, 1) = +Z direction.

**Proof Type:** P2 (Repo Invariant)

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `BuildFreeCameraViewProj()`
- WhatToInspect:
  - Lines 64-68: Forward vector construction
  ```cpp
  float cosP = cosf(cam.pitch);
  XMFLOAT3 forward = {
      sinf(cam.yaw) * cosP,
      sinf(cam.pitch),
      cosf(cam.yaw) * cosP
  };
  ```
- Claim: Forward vector derived from spherical coordinates.
- WhyItMattersHere: Camera movement in W/S keys uses this forward vector.

---

## 4. Repo Mapping

### 4.1 Matrix Flow Diagram

```
CPU Side:                          GPU Side:
-----------                        -----------

UpdateTransforms()                 cube_vs.hlsl
  |                                  |
  | world[10000]                     | Transforms[iid]
  | (row-major)                      | (StructuredBuffer)
  v                                  v
Upload Arena  -->  CopyBufferRegion  -->  DEFAULT Buffer
  |                                        |
  |                                        |
  |                                        v
BuildFreeCameraViewProj()          mul(pos, world)
  |                                  |
  | ViewProj                         | worldPos
  | (row-major)                      |
  v                                  v
Frame CB (256B)  -->  Root CBV  -->  mul(worldPos, ViewProj)
                                       |
                                       v
                                     SV_Position (clip space)
                                       |
                                       v
                                     GPU: Perspective Divide
                                       |
                                       v
                                     GPU: Viewport Transform
                                       |
                                       v
                                     Rasterizer -> PS
```

### 4.2 Coordinate System Truth Table

| Property | CPU | Shader | Consistent? |
|----------|-----|--------|-------------|
| Handedness | RH (USE_RIGHT_HANDED=1) | RH (LookAtRH) | Yes |
| Storage | Row-major (DirectXMath) | `row_major` keyword | Yes |
| Mul order | V * P (XMMatrixMultiply) | mul(vec, mat) | Yes |
| Translation row | Row 3 (indices 12-14) | Row 3 | Yes |
| Forward at yaw=0 | (0, 0, 1) = +Z | +Z | Yes |

---

## 5. Worked Numerical Examples

### Example 5.1: Floor Center to Screen

**Goal:** Trace floor center point from world to screen coordinates.

**Given (from `0_facts.md`):**
- Floor center: `p_world = (0, -0.01, 0)`
- Camera: eye=(0, 180, -220), yaw=0, pitch=-0.5
- FOV=pi/4, aspect=16/9 (assumed 1280x720), near=1, far=1000
- Viewport: (0, 0, 1280, 720, 0, 1)

**Note:** Viewport dimensions assumed. To verify actual values:
- File: `Dx12Context.cpp:406`
- Symbol: `m_viewport` initialization

**Step 1: World Position**
Floor has no world transform (uses identity in floor_vs.hlsl):
```
p_world = (0, -0.01, 0, 1)
```

**Step 2: Compute View Matrix Components**

Forward vector (yaw=0, pitch=-0.5):
```
cosP = cos(-0.5) = 0.8776
sinP = sin(-0.5) = -0.4794
sinY = sin(0) = 0
cosY = cos(0) = 1

forward = (0 * 0.8776, -0.4794, 1 * 0.8776) = (0, -0.4794, 0.8776)
```

Target point:
```
target = eye + forward = (0, 180, -220) + (0, -0.4794, 0.8776)
       = (0, 179.52, -219.12)
```

**Step 3: View Space Position**

The view matrix transforms p_world such that:
- Camera at origin
- Looking along -Z

Vector from eye to floor center:
```
p_local = p_world - eye = (0, -0.01, 0) - (0, 180, -220) = (0, -180.01, 220)
```

In view space (simplified, actual matrix is more complex):
- X stays same (camera not rotated around Y from forward)
- Need to account for pitch rotation

Approximate view-space Z (distance along view direction):
```
distance = dot((0, -180.01, 220), normalize(forward))
forward_norm = (0, -0.4794, 0.8776) / |forward| = (0, -0.4794, 0.8776) / 1.0
distance = 0*0 + (-180.01)*(-0.4794) + 220*(0.8776)
         = 86.32 + 193.07 = 279.39
```

So z_view ~ -279.39 (negative because in front of camera in RH system).

**Step 4: Clip Coordinates**

Using projection matrix with:
- f = 1/tan(pi/8) = 2.414
- aspect = 1.778

For a point at (0, y_view, z_view, 1):
```
x_clip = 0 * (f/aspect) = 0
y_clip = y_view * f
z_clip = z_view * (far/(near-far)) + near*far/(near-far)
w_clip = -z_view (for RH projection)
```

With z_view ~ -279.39:
```
w_clip = 279.39
z_clip = (-279.39) * (1000/-999) + (1*1000/-999)
       = 279.67 - 1.001 = 278.67
```

**Step 5: NDC**
```
x_ndc = 0 / 279.39 = 0
y_ndc = y_clip / w_clip (need y_view calculation)
z_ndc = 278.67 / 279.39 = 0.997
```

The floor center appears near horizontal center (x_ndc=0) and almost at far plane depth.

**Step 6: Screen Coordinates**
```
x_screen = 0 + (0 + 1) * 1280 / 2 = 640
y_screen = 0 + (1 - y_ndc) * 720 / 2  // depends on y_ndc
z_depth = 0 + 0.997 * 1 = 0.997
```

Floor center is at horizontal center of screen (pixel 640).

---

### Example 5.2: Cube Corner Transformation

**Goal:** Trace cube corner from object space to clip space.

**Given:**
- Object vertex: `p_obj = (-1, -1, -1)` (front-bottom-left of unit cube)
- Instance 0 (grid position after centering): tx=-99, ty=0, tz=-99
- Scale: (0.9, 3.0, 0.9)

**Step 1: World Matrix for Instance 0**
```
W = | 0.9   0     0     0   |
    | 0     3.0   0     0   |
    | 0     0     0.9   0   |
    | -99   0     -99   1   |
```

**Step 2: Object to World**
```
p_world = p_obj * W
        = (-1, -1, -1, 1) * W

x_w = -1*0.9 + 0 + 0 + 1*(-99) = -0.9 - 99 = -99.9
y_w = 0 + (-1)*3.0 + 0 + 0 = -3.0
z_w = 0 + 0 + (-1)*0.9 + 1*(-99) = -0.9 - 99 = -99.9
w_w = 1

p_world = (-99.9, -3.0, -99.9, 1)
```

**Step 3: Apply ViewProj**

This corner is at the far-left-back of the grid, well within the camera's view frustum.
The exact clip coordinates depend on the full ViewProj matrix, but we can verify:
- x_w is very negative -> will be on left side of screen
- y_w is slightly negative -> will be below center
- z_w is very negative -> will be in front of camera (visible)

**Verification:** Instance 0 is visible in the rendered scene at the back-left corner of the grid.

---

## 6. Failure Signatures

| Symptom | Likely Cause | Violated Definition |
|---------|--------------|---------------------|
| All geometry at origin | Translation in wrong matrix row | 2.1 World Matrix |
| Scene mirrored | LH/RH mismatch | 1.1 Handedness |
| Geometry inside-out | Column-major without `row_major` | 1.4 Storage |
| Nothing visible | w_c <= 0 (behind camera) | 3.2 Clipping |
| Geometry clipped unexpectedly | Near/far planes wrong | 2.3 Projection |
| Distorted aspect ratio | Wrong aspect calculation | 2.3 Projection |
| Y axis inverted | Missing Y flip in viewport | 2.6 Viewport |

---

## 7. Verification by Inspection

### Checklist: Convention Consistency

- [ ] `Dx12Context.cpp:21` has `USE_RIGHT_HANDED 1`
- [ ] `Dx12Context.cpp:77-78` uses `LookAtRH` and `PerspectiveFovRH`
- [ ] `common.hlsli:19,49` has `row_major` qualifier
- [ ] `cube_vs.hlsl:20-21` uses `mul(float4(...), matrix)`
- [ ] `Dx12Context.cpp:492-495` puts translation in indices 12,13,14

### Checklist: Aspect Ratio

- [ ] `Dx12Context.cpp:453` computes `aspect = width / height`
- [ ] Width and height come from window client rect

---

## 8. Further Reading / References

### (A) This Repo
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `BuildFreeCameraViewProj()`
- What to look for: Complete view/proj construction
- Why it matters HERE: Single source of truth for camera math

### (B) MS Learn
- Page: "Coordinate Systems (Direct3D 12)"
- What to look for: NDC ranges, perspective divide
- Why it matters HERE: Confirms Z in [0,1] not [-1,1]

### (C) DirectX-Graphics-Samples
- Repo: microsoft/DirectX-Graphics-Samples
- Sample: D3D12HelloTriangle
- File: `D3D12HelloTriangle.cpp`
- What to look for: Basic transformation setup
- Why it matters HERE: Reference for minimal pipeline

### (D) DirectXMath Documentation
- Page: MS Learn "DirectXMath"
- What to look for: Row-major storage, function naming (RH/LH suffix)
- Why it matters HERE: Explains why no transpose needed

---

## 9. Study Path

**Read Now:** Sections 1-3 for formal definitions and lemmas.

**Read When Broken:** Section 6 (Failure Signatures) when geometry doesn't appear or appears wrong.

**Read Later:** Section 5 (Worked Examples) for deep understanding of the math.
