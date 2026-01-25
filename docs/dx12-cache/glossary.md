# Glossary

DX12 and project-specific terminology.

---

## DX12 Core Terms

| Term | Definition |
|------|------------|
| **Descriptor** | Small struct describing a resource view (SRV, CBV, UAV, RTV, DSV) |
| **Descriptor Heap** | GPU memory block holding an array of descriptors |
| **Descriptor Table** | Root parameter pointing to a range of descriptors in a heap |
| **Root Signature** | Contract defining what resources shaders can access |
| **Root Parameter** | One binding slot in a root signature |
| **PSO** | Pipeline State Object - compiled graphics/compute pipeline |
| **Fence** | GPU/CPU synchronization primitive |
| **Command Allocator** | Memory backing for command list recording |
| **Command List** | Recorded GPU commands to be submitted |
| **Command Queue** | Submits command lists to GPU |

---

## Descriptor Types

| Abbreviation | Full Name | Purpose |
|--------------|-----------|---------|
| **CBV** | Constant Buffer View | Access constant buffer data |
| **SRV** | Shader Resource View | Read-only access to buffers/textures |
| **UAV** | Unordered Access View | Read/write access (compute, pixel shaders) |
| **RTV** | Render Target View | Write to render target |
| **DSV** | Depth Stencil View | Depth/stencil buffer access |

---

## Resource States

| State | When Used |
|-------|-----------|
| `COMMON` | Initial state, implicit transitions allowed |
| `RENDER_TARGET` | Bound as RTV |
| `PRESENT` | Ready for swap chain present |
| `COPY_DEST` | Target of copy operation |
| `COPY_SOURCE` | Source of copy operation |
| `PIXEL_SHADER_RESOURCE` | Bound as SRV in pixel shader |
| `NON_PIXEL_SHADER_RESOURCE` | Bound as SRV in VS/GS/etc |

---

## Modern DX12 Terms

| Term | Definition |
|------|------------|
| **Agility SDK** | Decouples DX12 features from Windows version |
| **DRED** | Device Removed Extended Data - crash diagnostics |
| **GBV** | GPU-Based Validation - runtime descriptor checks |
| **Enhanced Barriers** | New barrier model with explicit sync/access/layout |
| **VRS** | Variable Rate Shading - per-region shading rate control |
| **Mesh Shader** | Replaces IA+VS+HS/DS/GS with compute-like geometry stage |
| **Amplification Shader** | Optional stage before mesh shader for culling/LOD |
| **BLAS** | Bottom-Level Acceleration Structure (per-mesh RT geometry) |
| **TLAS** | Top-Level Acceleration Structure (scene instances) |
| **SBT** | Shader Binding Table - maps rays to shaders in DXR |
| **RTPSO** | Raytracing Pipeline State Object |

---

## Project-Specific Terms

| Term | Definition | Location |
|------|------------|----------|
| **FrameContext** | Per-frame resources (allocator, buffers, fence value) | `FrameContextRing.h` |
| **FrameContextRing** | Triple-buffer manager for frame resources | `FrameContextRing.h` |
| **RootParam / RP_** | Root parameter enum values | `ShaderLibrary.h` |
| **Naive mode** | Per-instance draw calls (debug/baseline) | Planned toggle |
| **Instanced mode** | Single draw call for all instances | Current default |
| **MICROTEST_MODE** | Diagnostic shader mode with color sentinel | `RenderConfig.h` |

---

## Naming Conventions

### Member Variables
- Prefix: `m_` (e.g., `m_rootSignature`)
- Non-owning pointers: raw pointer, owning: `ComPtr`

### Enums
- Prefix: Context-specific (e.g., `RP_` for RootParam)
- All caps with underscores for values

### Root Parameters
- `RP_FrameCB` - Frame constant buffer (b0)
- `RP_TransformsTable` - Transforms SRV table (t0)
- `RP_InstanceOffset` - Instance offset constant (b1)

### Register Notation
- `b0 space0` - Constant buffer register 0, space 0
- `t0 space0` - Texture/buffer register 0, space 0
- `u0 space0` - UAV register 0, space 0
- `s0 space0` - Sampler register 0, space 0

---

## Matrix Conventions

| Convention | Usage |
|------------|-------|
| **Row-major storage** | `row_major float4x4` in HLSL |
| **Multiplication order** | `mul(float4(pos, 1.0), Matrix)` - vector on LEFT |
| **DirectXMath default** | Row-major in memory (matches HLSL `row_major`) |

### Why Row-Major
- DirectXMath stores row-major by default
- Using `row_major` in HLSL avoids transpose on upload
- `mul(v, M)` reads naturally left-to-right
