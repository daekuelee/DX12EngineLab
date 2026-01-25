# Pinned Sources

Curated external references for DX12 development. Re-fetch policy indicates expected stability.

---

## Re-fetch Policy

| Tag | Meaning |
|-----|---------|
| `[STABLE]` | Official docs, unlikely to change |
| `[CHECK-YEARLY]` | May have updates |
| `[CHECK-MONTHLY]` | Actively evolving |

---

## 1. Core DX12 Architecture

### Root Signatures
- **Root Signature Overview** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures-overview
- **Root Signature Limits** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits
  - Max 64 DWORDs; root descriptors = 2 DWORDs each; tables = 1 DWORD

### Descriptor Heaps
- **Descriptor Heaps** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps
  - Only one shader-visible CBV_SRV_UAV heap bound at a time
  - Only one shader-visible SAMPLER heap bound at a time
- **Creating Descriptor Heaps** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-descriptor-heaps

### Resource Binding
- **Resource Binding** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding
- **Descriptor Range Flags** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_descriptor_range_flags

### Resource States & Barriers
- **Using Resource Barriers** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers
- **Resource State Transitions** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers#resource-states

---

## 2. Modern DX12 Platform

### Agility SDK
- **Agility SDK Overview** `[CHECK-YEARLY]`
  https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/
  - Decouples DX12 features from Windows version
  - Use for: Enhanced Barriers, Mesh Shaders on older OS, Work Graphs
- **Agility SDK NuGet** `[CHECK-MONTHLY]`
  https://www.nuget.org/packages/Microsoft.Direct3D.D3D12

### Feature Levels & Optional Features
- **D3D12_FEATURE_DATA queries** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_d3d12_options
  - Use `CheckFeatureSupport` before relying on optional features

---

## 3. Diagnostics & Debugging

### Debug Layer
- **Debug Layer Overview** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-the-debug-layer
  - Enable with `D3D12GetDebugInterface` before device creation
  - GPU-based validation (GBV): Additional runtime checks, significant perf cost
- **Debug Layer Messages** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/d3d12-debug-layer-reference

### GPU-Based Validation (GBV)
- **GBV Documentation** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/gpu-based-validation
  - Catches: descriptor heap out-of-bounds, uninitialized descriptors, resource state mismatches
  - Enable: `SetEnableGPUBasedValidation(TRUE)` on debug interface

### DRED (Device Removed Extended Data)
- **DRED Overview** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/use-dred
  - Auto-breadcrumbs: Identify last GPU operation before crash
  - Page fault info: Virtual address that caused GPU fault
- **D3D12_DRED_ENABLEMENT** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_dred_enablement

### PIX for Windows
- **PIX Home** `[CHECK-MONTHLY]`
  https://devblogs.microsoft.com/pix/
- **PIX Download** `[CHECK-MONTHLY]`
  https://devblogs.microsoft.com/pix/download/
  - GPU captures, timing analysis, shader debugging
  - Use for: draw call inspection, descriptor validation, barrier visualization

---

## 4. Modern Rendering Features

### Variable Rate Shading (VRS)
- **VRS Overview** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/vrs
- **D3D12_SHADING_RATE enum** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_shading_rate
  - Tier 1: Per-draw (RSSetShadingRate)
  - Tier 2: Per-primitive (SV_ShadingRate from VS/GS) + screen-space image
- **Sample: D3D12VariableRateShading** `[CHECK-YEARLY]`
  https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12VariableRateShading

### Mesh Shaders
- **Mesh Shader Overview** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/mesh-shaders
- **Amplification Shaders** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/amplification-shaders
  - Pipeline: Amplification Shader (optional) -> Mesh Shader -> Pixel Shader
  - Replaces: IA + VS + (HS/DS/GS)
- **Sample: D3D12MeshShaders** `[CHECK-YEARLY]`
  https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12MeshShaders

### Raytracing (DXR)
- **DXR Overview** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/raytracing
- **Acceleration Structures** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/acceleration-structures
  - BLAS: Bottom-Level (per-mesh geometry)
  - TLAS: Top-Level (scene instances referencing BLAS)
- **Shader Tables (SBT)** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/shader-tables
  - Records: RayGen, Miss, HitGroup
  - Layout must match DispatchRays parameters
- **Sample: D3D12Raytracing** `[CHECK-YEARLY]`
  https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Raytracing

---

## 5. Modern Synchronization & Barriers

### Enhanced Barriers
- **Enhanced Barriers Overview** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/enhanced-barriers
  - D3D12_BARRIER_SYNC, D3D12_BARRIER_ACCESS, D3D12_BARRIER_LAYOUT
  - More explicit than legacy ResourceBarrier
  - Requires Agility SDK or Windows 11 22H2+
- **Barrier Best Practices** `[STABLE]`
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-barrier-best-practices

---

## 6. Official Microsoft Samples (GitHub)

| Sample | Features Demonstrated | Relevance |
|--------|----------------------|-----------|
| [D3D12HelloWorld](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld) | Minimal triangle | Baseline sanity |
| [D3D12Bundles](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Bundles) | Command bundles | Draw call optimization |
| [D3D12Multithreading](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Multithreading) | Per-thread allocators | Job system integration |
| [D3D12VariableRateShading](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12VariableRateShading) | VRS Tiers 1 & 2 | Future perf optimization |
| [D3D12MeshShaders](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12MeshShaders) | Amplification + Mesh | Modern geometry pipeline |
| [D3D12Raytracing](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Raytracing) | DXR 1.0/1.1 | Raytracing integration |
| [D3D12ExecuteIndirect](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12ExecuteIndirect) | Indirect drawing | GPU-driven rendering |
| [D3D12DynamicIndexing](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12DynamicIndexing) | Bindless-style indexing | Descriptor table patterns |

---

## 7. Tools

| Tool | URL | Purpose |
|------|-----|---------|
| PIX for Windows | https://devblogs.microsoft.com/pix/download/ | GPU capture and debugging |
| Agility SDK NuGet | https://www.nuget.org/packages/Microsoft.Direct3D.D3D12 | Feature rollout |
| DXC (Shader Compiler) | https://github.com/microsoft/DirectXShaderCompiler | SM 6.x compilation |
| RenderDoc | https://renderdoc.org/ | Alternative GPU debugger (limited DX12 support) |

---

## Adding New Sources

When adding a new source:
1. Verify URL is official/authoritative
2. Assign re-fetch tag
3. Add brief note on why it's useful
4. Commit with message: `docs(dx12-cache): pin <source-name>`
