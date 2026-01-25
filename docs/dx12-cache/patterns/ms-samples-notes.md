# Microsoft Samples Analysis

Engine integration hooks extracted from official DirectX-Graphics-Samples.

---

## Foundational Samples

### D3D12HelloWorld
- **Pattern**: Minimal PSO setup - triangle with no transforms
- **Key Files**: `D3D12HelloTriangle.cpp`
- **Warning**: No fence gating, no triple buffering - NOT production-ready
- **Engine hook**: None - use only for initial sanity check

### D3D12Bundles
- **Pattern**: Command bundle for reusable draw sequences
- **Key Observation**: Bundles inherit PSO and root signature from parent command list
- **Engine hook**:
  - Record bundle once with static geometry draw calls
  - Execute bundle from per-frame command list
  - Bundles cannot contain barriers or resource state changes

### D3D12Multithreading
- **Pattern**: Per-thread command allocator + command list
- **Key Observation**: One allocator per thread per frame-in-flight
- **Engine hook**:
  - Job system integration for parallel recording
  - Main thread: `ExecuteCommandLists` with array of lists
  - Each worker: own allocator (fence-gated per frame)

### D3D12ExecuteIndirect
- **Pattern**: GPU-driven draw calls via indirect buffer
- **Key Observation**: Argument buffer filled by compute shader
- **Engine hook**:
  - Compute shader writes draw arguments
  - `ExecuteIndirect` with command signature
  - Useful for GPU culling results

---

## Variable Rate Shading (VRS)

### D3D12VariableRateShading Sample

**Repository**: `Samples/Desktop/D3D12VariableRateShading`

**Key Entrypoints**:
- `CreateRootSignature()` - No VRS-specific root params needed
- `CreatePipelineState()` - No PSO change for per-draw VRS (Tier 1)
- `PopulateCommandList()` - `RSSetShadingRate()` call

**Tier Breakdown**:

| Tier | Feature | API |
|------|---------|-----|
| Tier 1 | Per-draw shading rate | `RSSetShadingRate(rate, combiners)` |
| Tier 2 | Per-primitive (VS output) + screen-space image | Above + SV_ShadingRate + VRS image |

**Engine Integration Hooks**:

| Aspect | Change Required |
|--------|----------------|
| PSO | None for Tier 1 |
| Root Signature | None |
| Descriptors | Tier 2 image-based: SRV for shading rate image |
| Barriers | Shading rate image: `D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE` |
| Frame Context | Optional: per-frame shading rate image if dynamic |
| Command List | Add `RSSetShadingRate()` before draw calls |

**Sample Code Pattern**:
```cpp
// Per-draw VRS (Tier 1)
D3D12_SHADING_RATE_COMBINER combiners[] = {
    D3D12_SHADING_RATE_COMBINER_PASSTHROUGH,
    D3D12_SHADING_RATE_COMBINER_PASSTHROUGH
};
cmdList->RSSetShadingRate(D3D12_SHADING_RATE_2X2, combiners);
cmdList->DrawIndexedInstanced(...);
```

---

## Mesh Shaders

### D3D12MeshShaders Sample

**Repository**: `Samples/Desktop/D3D12MeshShaders`

**Key Entrypoints**:
- `CreateRootSignature()` - Same pattern, visibility flags for MS/AS
- `CreatePipelineState()` - `D3D12_MESH_SHADER_PIPELINE_STATE_DESC`
- `DispatchMesh()` - Replaces Draw/DrawIndexed

**Pipeline Comparison**:

| Traditional | Mesh Shader |
|-------------|-------------|
| IA -> VS -> HS -> DS -> GS -> Rasterizer | AS (optional) -> MS -> Rasterizer |

**Engine Integration Hooks**:

| Aspect | Change Required |
|--------|----------------|
| PSO | New desc type: `D3D12_MESH_SHADER_PIPELINE_STATE_DESC` |
| Root Signature | Visibility: `D3D12_SHADER_VISIBILITY_MESH`, `_AMPLIFICATION` |
| Input Layout | None - mesh shaders don't use IA |
| Descriptors | Same heap, visibility flags differ |
| Barriers | No special barriers needed |
| Frame Context | Meshlet buffer if geometry is per-frame |
| Command List | `DispatchMesh(x, y, z)` instead of `DrawIndexedInstanced` |

**Sample PSO Creation Pattern**:
```cpp
D3DX12_MESH_SHADER_PIPELINE_STATE_DESC psoDesc = {};
psoDesc.pRootSignature = m_rootSignature.Get();
psoDesc.AS = { asBlob->GetBufferPointer(), asBlob->GetBufferSize() }; // optional
psoDesc.MS = { msBlob->GetBufferPointer(), msBlob->GetBufferSize() };
psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
// ... rasterizer, blend, depth stencil, etc.

auto stream = CD3DX12_PIPELINE_MESH_STATE_STREAM(psoDesc);
D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = { sizeof(stream), &stream };
device->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_pso));
```

**Meshlet Data Structure**:
- Meshlets: small batches of triangles (typically 64-128 verts, 126 tris)
- Meshlet buffer: vertex indices + triangle indices + meshlet descriptors
- Amplification shader can cull meshlets before mesh shader runs

---

## Raytracing (DXR)

### D3D12Raytracing / D3D12RaytracingHelloWorld Samples

**Repository**: `Samples/Desktop/D3D12Raytracing`

**Key Entrypoints**:
- `CreateRaytracingPipeline()` - RTPSO with shader libraries
- `BuildAccelerationStructures()` - BLAS/TLAS creation
- `CreateShaderTables()` - SBT layout (RayGen, Miss, HitGroup)
- `DispatchRays()` - Replaces Draw for RT passes

**Acceleration Structure Hierarchy**:

```
TLAS (Top-Level)
  ├── Instance 0 → BLAS 0 (mesh geometry)
  ├── Instance 1 → BLAS 0 (same mesh, different transform)
  └── Instance 2 → BLAS 1 (different mesh)
```

**Engine Integration Hooks**:

| Aspect | Change Required |
|--------|----------------|
| PSO | New: `D3D12_STATE_OBJECT_DESC` (RTPSO) with shader libraries |
| Root Signature | Local vs Global root sigs; SBT-aware layout |
| Descriptors | TLAS SRV in shader-visible heap; UAV for output |
| Barriers | `D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE` for AS build |
| Frame Context | Per-frame TLAS if dynamic; SBT updates for moving objects |
| Command List | `DispatchRays()` with D3D12_DISPATCH_RAYS_DESC |

**Shader Binding Table (SBT) Layout**:

| Section | Contents |
|---------|----------|
| Ray Generation | Entry point for ray dispatch |
| Miss | Shader when ray hits nothing |
| Hit Group | Closest-hit + any-hit + intersection shaders |

**Sample RTPSO Creation Pattern**:
```cpp
CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

// DXIL library with shaders
auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
lib->SetDXILLibrary(&libraryDXIL);

// Hit group
auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
hitGroup->SetClosestHitShaderImport(L"ClosestHitShader");
hitGroup->SetHitGroupExport(L"HitGroup");
hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

// Shader config, pipeline config, root signature associations...

device->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_rtpso));
```

**BLAS/TLAS Build Pattern**:
```cpp
// BLAS geometry description
D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
geometryDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
geometryDesc.Triangles.VertexCount = vertexCount;
geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
geometryDesc.Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress();
geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
geometryDesc.Triangles.IndexCount = indexCount;

// Build BLAS
D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
inputs.NumDescs = 1;
inputs.pGeometryDescs = &geometryDesc;
inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
```

---

## Integration Priority

For this engine, suggested integration order:

1. **VRS (Tier 1)** - Minimal changes, immediate perf win for peripheral areas
2. **Mesh Shaders** - Replaces current VS pipeline, enables GPU culling
3. **Raytracing** - Most invasive, separate render path

Each feature requires:
- Feature support check via `CheckFeatureSupport`
- Agility SDK for older Windows versions
- Fallback path for unsupported hardware
