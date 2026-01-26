# How to Extend

This document provides patterns for adding new features.

---

## Adding a New Root Parameter

### Step 1: Update RootParam Enum

```cpp
// In ShaderLibrary.h
enum RootParam : uint32_t {
    RP_FrameCB = 0,
    RP_TransformsTable = 1,
    RP_InstanceOffset = 2,
    RP_DebugCB = 3,
    RP_NewParam = 4,        // ← Add new entry
    RP_Count
};
```

### Step 2: Update Root Signature Creation

```cpp
// In ShaderLibrary.cpp - CreateRootSignature()
CD3DX12_ROOT_PARAMETER1 rootParams[RP_Count];

// Existing parameters...
rootParams[RP_FrameCB].InitAsConstantBufferView(0);  // b0
// ...

// Add new parameter
rootParams[RP_NewParam].InitAsConstantBufferView(3);  // b3 (example)
```

### Step 3: Bind During Render

```cpp
// In PassOrchestrator or relevant pass
cmd->SetGraphicsRootConstantBufferView(RP_NewParam, newDataGpuVA);
```

### Step 4: Update Shader

```hlsl
cbuffer NewCB : register(b3)
{
    // New data...
};
```

---

## Adding a New Toggle

### Step 1: Add State Variable

```cpp
// In ToggleSystem.h
private:
    static inline bool s_newFeatureEnabled = false;
```

### Step 2: Add Accessors

```cpp
public:
    static bool IsNewFeatureEnabled() { return s_newFeatureEnabled; }
    static void SetNewFeatureEnabled(bool enabled) { s_newFeatureEnabled = enabled; }
    static void ToggleNewFeature() { s_newFeatureEnabled = !s_newFeatureEnabled; }
```

### Step 3: Add Key Handler

```cpp
// In App.cpp or window proc
case 'N':  // Example key
    ToggleSystem::ToggleNewFeature();
    break;
```

### Step 4: Use in Render

```cpp
if (ToggleSystem::IsNewFeatureEnabled())
{
    // New feature code...
}
```

---

## Adding a New Pass

### Step 1: Create Pass Header

```cpp
// NewPass.h
#pragma once
#include "RenderContext.h"

namespace Renderer
{
    struct NewPassInputs
    {
        // Pass-specific inputs
    };

    class NewPass
    {
    public:
        static uint32_t Execute(const RenderContext& ctx, const NewPassInputs& inputs);
    };
}
```

### Step 2: Create Pass Implementation

```cpp
// NewPass.cpp
#include "NewPass.h"

namespace Renderer
{
    uint32_t NewPass::Execute(const RenderContext& ctx, const NewPassInputs& inputs)
    {
        auto cmd = ctx.commandList;

        // Set PSO, bindings, etc.
        // Record draw commands
        // Return draw call count
    }
}
```

### Step 3: Add to PassOrchestrator

```cpp
// In PassOrchestrator.cpp
uint32_t PassOrchestrator::Execute(const PassOrchestratorInputs& inputs, ...)
{
    uint32_t drawCalls = 0;

    if (flags.clearPass) { /* ... */ }
    if (flags.geometryPass) { /* ... */ }

    // Add new pass
    if (flags.newPass)
    {
        NewPassInputs newInputs = { /* ... */ };
        drawCalls += NewPass::Execute(ctx, newInputs);
    }

    if (flags.imguiPass) { /* ... */ }

    return drawCalls;
}
```

---

## Adding Per-Frame Data

### Step 1: Add Allocation in Render

```cpp
// In Dx12Context::Render()
Allocation newDataAlloc = m_uploadArena.Allocate(NEW_DATA_SIZE, 256, "NewData");

// Fill data
float* data = static_cast<float*>(newDataAlloc.cpuPtr);
// ...
```

### Step 2: Copy to Default Heap (if needed)

For large data that needs fast GPU access:

```cpp
// Add buffer to FrameContext
ResourceHandle newDataHandle;

// Create in FrameContextRing::CreatePerFrameBuffers()
// Register with StateTracker
// Copy in RecordBarriersAndCopy()
```

### Step 3: Bind for Shader

```cpp
// Via root CBV (small data):
cmd->SetGraphicsRootConstantBufferView(RP_NewParam, newDataAlloc.gpuVA);

// Via SRV table (large data):
// Create SRV, bind table
```

---

## Adding New Geometry

### Step 1: Add Buffers to RenderScene

```cpp
// In RenderScene.h
private:
    ComPtr<ID3D12Resource> m_newVertexBuffer;
    ComPtr<ID3D12Resource> m_newIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_newVbv = {};
    D3D12_INDEX_BUFFER_VIEW m_newIbv = {};
    uint32_t m_newIndexCount = 0;
```

### Step 2: Create Geometry via Factory

```cpp
// In RenderScene.cpp
bool RenderScene::CreateNewGeometry(GeometryFactory* factory)
{
    struct Vertex { float x, y, z, nx, ny, nz; };
    Vertex vertices[] = { /* ... */ };
    uint16_t indices[] = { /* ... */ };

    auto vbResult = factory->CreateVertexBuffer(vertices, sizeof(vertices), sizeof(Vertex));
    if (!vbResult.resource) return false;

    auto ibResult = factory->CreateIndexBuffer(indices, sizeof(indices), DXGI_FORMAT_R16_UINT);
    if (!ibResult.resource) return false;

    m_newVertexBuffer = vbResult.resource;
    m_newVbv = vbResult.view;
    m_newIndexBuffer = ibResult.resource;
    m_newIbv = ibResult.view;
    m_newIndexCount = ARRAYSIZE(indices);

    return true;
}
```

### Step 3: Add Draw Method

```cpp
void RenderScene::RecordDrawNew(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->IASetVertexBuffers(0, 1, &m_newVbv);
    cmdList->IASetIndexBuffer(&m_newIbv);
    cmdList->DrawIndexedInstanced(m_newIndexCount, 1, 0, 0, 0);
}
```

---

## Adding a New PSO

### Step 1: Add Shader Blobs

```cpp
// In ShaderLibrary.h
private:
    ComPtr<ID3DBlob> m_newVsBlob;
    ComPtr<ID3DBlob> m_newPsBlob;
    ID3D12PipelineState* m_newPso = nullptr;
```

### Step 2: Load Shaders

```cpp
// In ShaderLibrary::LoadShaders()
if (!LoadCompiledShader(L"shaders/NewVS.cso", m_newVsBlob)) return false;
if (!LoadCompiledShader(L"shaders/NewPS.cso", m_newPsBlob)) return false;
```

### Step 3: Create PSO

```cpp
// In ShaderLibrary::CreatePSO()
D3D12_GRAPHICS_PIPELINE_STATE_DESC newPsoDesc = { /* ... */ };
newPsoDesc.VS = { m_newVsBlob->GetBufferPointer(), m_newVsBlob->GetBufferSize() };
newPsoDesc.PS = { m_newPsBlob->GetBufferPointer(), m_newPsBlob->GetBufferSize() };
// Configure other state...

m_newPso = m_psoCache.GetOrCreate(device, newPsoDesc, "NewPSO");
```

### Step 4: Add Accessor

```cpp
ID3D12PipelineState* GetNewPSO() const { return m_newPso; }
```

---

## Checklist: Before Submitting

- [ ] **Build green**: Debug x64 compiles
- [ ] **Build green**: Release x64 compiles
- [ ] **Debug layer clean**: No D3D12 ERROR messages
- [ ] **Visual check**: New feature works as expected
- [ ] **Toggle test**: Feature can be enabled/disabled cleanly
- [ ] **No regressions**: Existing features still work

---

## Common Mistakes

### 1. Forgetting Alignment

```cpp
// Wrong
Allocation alloc = m_uploadArena.Allocate(size);

// Right (for CBV)
Allocation alloc = m_uploadArena.Allocate(size, 256, "Tag");
```

### 2. Missing State Transition

```cpp
// Wrong - using resource without transition
cmdList->CopyBufferRegion(dest, ...);

// Right
m_stateTracker.Transition(dest, D3D12_RESOURCE_STATE_COPY_DEST);
m_stateTracker.FlushBarriers(cmdList);
cmdList->CopyBufferRegion(dest, ...);
```

### 3. Wrong Root Parameter Index

```cpp
// Wrong - hardcoded index
cmd->SetGraphicsRoot32BitConstant(5, value, 0);

// Right - use enum
cmd->SetGraphicsRoot32BitConstant(RP_NewParam, value, 0);
```

---

## Further Reading

| Topic | Document | What to Look For |
|-------|----------|------------------|
| Architecture | [10-architecture-map.md](10-architecture-map.md) | Component relationships |
| Binding | [40-binding-abi.md](40-binding-abi.md) | Root signature details |
| Upload | [50-uploadarena.md](50-uploadarena.md) | Allocation patterns |
| Debug | [70-debug-playbook.md](70-debug-playbook.md) | When things go wrong |

---

## Study Path

### Read Now
- [90-exercises.md](90-exercises.md) - Practice these patterns

### Read When Broken
- [70-debug-playbook.md](70-debug-playbook.md) - Error diagnosis

### Read Later
- Contract docs in `docs/contracts/` for phase requirements
