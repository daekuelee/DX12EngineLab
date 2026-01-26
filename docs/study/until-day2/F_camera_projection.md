# F. Camera & Projection

## Abstract

This module provides detailed analysis of the camera system implementation, including view matrix construction, projection parameters, and input handling. After reading, you will be able to:
1. Derive camera orientation from yaw/pitch values
2. Construct view and projection matrices from parameters
3. Verify camera behavior matches expected coordinate system

---

## 1. Formal Model / Definitions

### 1.1 Camera State (Definition)

The `FreeCamera` struct encodes all camera parameters:

| Field | Type | Default | Meaning |
|-------|------|---------|---------|
| position[3] | float | (0, 180, -220) | Eye position in world space |
| yaw | float | 0 | Horizontal rotation (radians) |
| pitch | float | -0.5 | Vertical rotation (radians) |
| fovY | float | pi/4 | Vertical field of view (radians) |
| nearZ | float | 1.0 | Near clipping plane |
| farZ | float | 1000.0 | Far clipping plane |
| moveSpeed | float | 100.0 | Units per second |
| lookSpeed | float | 1.5 | Radians per second |

**Source-of-Truth**
- EvidenceType: E1 (compile-time constant)
- File: `Renderer/DX12/Dx12Context.h`
- Symbol: `Dx12Context::FreeCamera`
- WhatToInspect:
  - Lines 102-112: Complete struct definition
- Claim: Camera state is a simple struct with sensible defaults.
- WhyItMattersHere: Default position is above and behind the grid center.

### 1.2 Spherical Coordinate Conventions (Definition)

Yaw and pitch define the forward direction in spherical coordinates:

```
forward.x = sin(yaw) * cos(pitch)
forward.y = sin(pitch)
forward.z = cos(yaw) * cos(pitch)
```

**Convention:**
- yaw = 0: Looking along +Z
- yaw = pi/2: Looking along +X
- pitch = 0: Looking horizontally
- pitch > 0: Looking up
- pitch < 0: Looking down

### 1.3 Right-Handed Coordinate System (Definition)

In a right-handed coordinate system:
- +X points right
- +Y points up
- +Z points forward (out of screen)
- Cross product: X x Y = Z

The camera looks along -Z in view space (objects in front have negative view-space Z).

---

## 2. View Matrix Construction

### 2.1 LookAt Matrix (Theorem)

**Theorem:** `XMMatrixLookAtRH(eye, target, up)` produces a view matrix V such that:
- Eye position is transformed to origin
- Target direction is transformed to -Z
- Up direction is transformed to +Y

**Construction:**

1. Compute forward: `f = normalize(target - eye)`
2. Compute right: `r = normalize(cross(up, f))`
3. Compute new up: `u = cross(f, r)`

The view matrix is:
```
V = | r.x   u.x  -f.x   0 |
    | r.y   u.y  -f.y   0 |
    | r.z   u.z  -f.z   0 |
    | -dot(r,eye)  -dot(u,eye)  dot(f,eye)  1 |
```

(Row-major storage, column vectors in rows)

### 2.2 Forward Vector Derivation (Proof)

**Goal:** Derive the forward vector from yaw and pitch.

**Derivation:**

Starting from +Z axis (0, 0, 1), apply rotations:

1. Rotate around Y by yaw:
   ```
   (0, 0, 1) -> (sin(yaw), 0, cos(yaw))
   ```

2. Rotate around X by pitch (in the local frame):
   ```
   The Y component becomes: sin(pitch)
   The horizontal component is scaled by cos(pitch)
   ```

Result:
```
forward = (sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch))
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `BuildFreeCameraViewProj()`
- WhatToInspect:
  - Lines 63-68: Forward vector computation
  ```cpp
  float cosP = cosf(cam.pitch);
  XMFLOAT3 forward = {
      sinf(cam.yaw) * cosP,
      sinf(cam.pitch),
      cosf(cam.yaw) * cosP
  };
  ```
- Claim: Forward vector matches spherical coordinate formula.
- WhyItMattersHere: Incorrect formula causes camera to look in wrong direction.

### 2.3 View Matrix Assembly (Proof Type: P3 Dataflow)

**Dataflow trace:**

```
cam.position -> eye (XMVECTOR)
forward (computed) -> fwd (XMVECTOR)
eye + fwd -> target (XMVECTOR)
(0, 1, 0) -> up (XMVECTOR)
XMMatrixLookAtRH(eye, target, up) -> view (XMMATRIX)
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `BuildFreeCameraViewProj()`
- WhatToInspect:
  - Lines 70-77: LookAt construction
  ```cpp
  XMFLOAT3 pos = { cam.position[0], cam.position[1], cam.position[2] };
  XMVECTOR eye = XMLoadFloat3(&pos);
  XMVECTOR fwd = XMLoadFloat3(&forward);
  XMVECTOR target = XMVectorAdd(eye, fwd);
  XMVECTOR up = XMVectorSet(0, 1, 0, 0);
  XMMATRIX view = XMMatrixLookAtRH(eye, target, up);
  ```
- Claim: View matrix constructed from camera state using RH convention.
- WhyItMattersHere: Using LH function would flip Z direction.

---

## 3. Projection Matrix

### 3.1 Perspective Projection (Theorem)

**Theorem:** `XMMatrixPerspectiveFovRH(fovY, aspect, near, far)` produces:

```
P = | cot(fovY/2)/aspect   0              0                    0  |
    | 0                    cot(fovY/2)    0                    0  |
    | 0                    0              far/(near-far)      -1  |
    | 0                    0              near*far/(near-far)  0  |
```

Where `cot(x) = 1/tan(x)`.

**Properties:**
- Points at z = -near map to z_ndc = 0
- Points at z = -far map to z_ndc = 1
- W component is -z (perspective divide by distance)

### 3.2 Depth Mapping (Derivation)

For a view-space point at z = z_v (negative, in front of camera):

```
z_clip = z_v * (far/(near-far)) + near*far/(near-far)
w_clip = -z_v

z_ndc = z_clip / w_clip
      = [z_v * far/(near-far) + near*far/(near-far)] / (-z_v)
      = -far/(near-far) - near*far/[z_v*(near-far)]
      = far/[-(near-far)] + near*far/[-z_v*(near-far)]
      = far/(far-near) + near*far/[z_v*(far-near)]
```

At z_v = -near:
```
z_ndc = far/(far-near) + near*far/[-near*(far-near)]
      = far/(far-near) - far/(far-near)
      = 0
```

At z_v = -far:
```
z_ndc = far/(far-near) + near*far/[-far*(far-near)]
      = far/(far-near) - near/(far-near)
      = (far-near)/(far-near)
      = 1
```

**Spec Evidence**
- Source: MS Learn "Projection Transform"
- WhatToInspect:
  - DirectX depth range [0, 1]
  - Near plane maps to 0, far to 1
- Claim: Standard (non-reversed) Z mapping.
- WhyItMattersHere: Depth buffer cleared to 1.0 (far); LESS comparison passes closer.

### 3.3 ViewProj Composition

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `BuildFreeCameraViewProj()`
- WhatToInspect:
  - Line 84: `return XMMatrixMultiply(view, proj)`
- Claim: ViewProj = View * Proj (not Proj * View).
- WhyItMattersHere: Matrix multiplication order determines transformation sequence.

---

## 4. Repo Mapping

### 4.1 Camera Update Loop

```
Render()
  |
  +-> UpdateDeltaTime()
  |     Returns: float dt
  |
  +-> UpdateCamera(dt)
        |
        +-> Read input keys (W/S/A/D/Space/Ctrl/Q/E)
        +-> Compute movement in camera space
        +-> Update m_camera.position[0..2]
        +-> Update m_camera.yaw
```

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `Dx12Context::UpdateCamera()`
- WhatToInspect:
  - Lines 29-58: Input handling and position update
- Claim: Camera position and yaw updated from keyboard input.
- WhyItMattersHere: Movement is in camera's local coordinate frame.

### 4.2 Movement Vectors

**Horizontal forward (XZ plane):**
```cpp
XMFLOAT3 forward = { sinY, 0, cosY };  // sinY = sin(yaw), cosY = cos(yaw)
```

**Horizontal right:**
```cpp
XMFLOAT3 right = { cosY, 0, -sinY };
```

Note: These are different from the full forward vector used for view matrix (which includes pitch).

**Source-of-Truth**
- EvidenceType: E2 (API call site)
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `Dx12Context::UpdateCamera()`
- WhatToInspect:
  - Lines 49-52: Movement vector computation
- Claim: Movement ignores pitch (fly camera, not walk camera).
- WhyItMattersHere: W/S always moves horizontally in the XZ plane.

### 4.3 Input Mapping

| Key | Action | Variable |
|-----|--------|----------|
| W / Up | Move forward (+Z) | moveZ += 1 |
| S / Down | Move backward (-Z) | moveZ -= 1 |
| A / Left | Strafe left (-X) | moveX -= 1 |
| D / Right | Strafe right (+X) | moveX += 1 |
| Space | Move up (+Y) | moveY += 1 |
| Ctrl | Move down (-Y) | moveY -= 1 |
| Q | Rotate left (- yaw) | yawDelta -= 1 |
| E | Rotate right (+ yaw) | yawDelta += 1 |

---

## 5. Worked Examples

### Example 5.1: Default Camera View

**Given:**
- Position: (0, 180, -220)
- Yaw: 0
- Pitch: -0.5

**Forward Vector:**
```
cosP = cos(-0.5) = 0.8776
sinP = sin(-0.5) = -0.4794

forward = (sin(0)*0.8776, -0.4794, cos(0)*0.8776)
        = (0, -0.4794, 0.8776)
```

**Interpretation:**
- Camera is 180 units above origin
- Camera is 220 units behind origin (negative Z)
- Looking forward-down (positive Z, negative Y component)
- Target point: (0, 180-0.4794, -220+0.8776) = (0, 179.52, -219.12)

This makes the camera look toward the cube grid which is centered at origin.

### Example 5.2: After Pressing W for 1 Second

**Given:**
- Initial position: (0, 180, -220)
- Yaw: 0, Pitch: -0.5
- moveSpeed: 100 units/sec
- dt: 1.0 sec

**Movement:**
```
moveZ = 1.0 (W pressed)
forward_horiz = (sin(0), 0, cos(0)) = (0, 0, 1)
right_horiz = (cos(0), 0, -sin(0)) = (1, 0, 0)

movement = forward_horiz * moveZ * speed * dt
         = (0, 0, 1) * 1.0 * 100 * 1.0
         = (0, 0, 100)

new_position = (0, 180, -220) + (0, 0, 100) = (0, 180, -120)
```

Camera moves 100 units closer to the grid (toward +Z).

### Example 5.3: Projection of Near-Plane Point

**Given:**
- View-space point: (0, 0, -1) (on near plane, directly in front)
- near: 1, far: 1000, fovY: pi/4, aspect: 16/9

**Projection:**
```
f = 1/tan(pi/8) = 2.414

P * (0, 0, -1, 1) =
  x_clip = 0 * 2.414/1.778 = 0
  y_clip = 0 * 2.414 = 0
  z_clip = -1 * 1000/(1-1000) + 1*1000/(1-1000)
         = 1000/999 - 1000/999 = 0
  w_clip = -(-1) = 1

p_clip = (0, 0, 0, 1)
p_ndc = (0, 0, 0)  // At near plane, screen center
```

### Example 5.4: Projection of Far-Plane Point

**Given:**
- View-space point: (0, 0, -1000) (on far plane)

**Projection:**
```
z_clip = -1000 * 1000/(1-1000) + 1*1000/(1-1000)
       = 1000000/999 - 1000/999
       = (1000000 - 1000)/999
       = 999000/999
       = 1000 (approximately)

w_clip = 1000

z_ndc = 1000/1000 = 1  // At far plane
```

---

## 6. Failure Signatures

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Scene inverted | LH instead of RH | Use `XMMatrixLookAtRH` |
| Movement inverted | Wrong forward formula | Check sin/cos order |
| Nothing visible | Camera inside geometry or behind | Adjust initial position |
| Extreme distortion | Aspect ratio wrong | Check width/height order |
| Z-fighting | Near plane too close | Increase nearZ |
| Geometry disappears at edge | FOV too narrow | Increase fovY |

---

## 7. Verification by Inspection

### 7.1 Sanity Checklist

- [ ] **Forward at yaw=0, pitch=0:** Should be (0, 0, 1) = +Z
  - Check: `Dx12Context.cpp:64-68`
  - Verify: sin(0)=0, cos(0)=1, so forward=(0, 0, 1)

- [ ] **ViewProj produces valid clip coords:** w > 0 for visible points
  - Check: Points in front of camera should have positive w after projection
  - The projection matrix has -1 in position [2][3], so w_clip = -z_view > 0 when z_view < 0

- [ ] **Depth buffer clears to 1.0:**
  - Check: `Dx12Context.cpp:250` has `clearValue.DepthStencil.Depth = 1.0f`

- [ ] **Depth test LESS passes closer fragments:**
  - Check: `ShaderLibrary.cpp:252` has `DepthFunc = LESS`
  - Closer fragments have smaller z_ndc < 1.0

### 7.2 Console Verification

To verify camera state at runtime, check the diagnostic log:
- Frame index and mode logged in `Render()` (lines 569-576)
- Camera position not logged by default, but can be added

---

## 8. Further Reading / References

### (A) This Repo
- File: `Renderer/DX12/Dx12Context.cpp`
- Symbol: `UpdateCamera()`, `BuildFreeCameraViewProj()`
- What to look for: Input handling, matrix construction
- Why it matters HERE: Complete camera implementation

### (B) MS Learn
- Page: "DirectXMath Library"
- What to look for: `XMMatrixLookAtRH`, `XMMatrixPerspectiveFovRH`
- Why it matters HERE: Reference for matrix functions

### (C) DirectX-Graphics-Samples
- Repo: microsoft/DirectX-Graphics-Samples
- Sample: D3D12MeshShaders
- File: Camera helper classes
- What to look for: Alternative camera implementations
- Why it matters HERE: Comparison with more complex cameras

### (D) Real-Time Rendering (Book)
- Chapter: Transforms
- What to look for: Derivation of projection matrices
- Why it matters HERE: Deep understanding of perspective math

---

## 9. Study Path

**Read Now:**
- Section 1 (Definitions) for camera state structure
- Section 2 (View Matrix) for LookAt construction
- Section 3 (Projection) for depth mapping

**Read When Broken:**
- Section 6 (Failure Signatures) when camera doesn't work as expected

**Read Later:**
- Section 5 (Worked Examples) for detailed calculations
