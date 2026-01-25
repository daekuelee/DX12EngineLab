The plan is complete with:

  1. 8 precise steps covering all visual ground truth requirements                                                                                                                                                              2. Exact file locations and line numbers for each change
  3. Code snippets showing before/after for key changes                                                                                                                                                                         4. Verification checklist with evidence artifact paths
  5. Minimal scope - only changes strictly necessary

  Key technical decisions:
  - Floor at y=-0.01 (slightly below cubes at y=0) ensures proper depth ordering
  - Separate floor PSO with constant beige color (per your constraint)
  - Floor rendered first, cubes second - both use same depth test
  - Toggle remains unchanged - floor is always 1 draw call, cube draw count varies

────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                                                                               Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day1 Visual Ground Truth Implementation Plan

 Current State vs Required
 ┌──────────────┬──────────────────────────────┬─────────────────────────────────┐
 │   Element    │           Current            │            Required             │
 ├──────────────┼──────────────────────────────┼─────────────────────────────────┤
 │ Clear color  │ Dark gray (0.05, 0.07, 0.10) │ SKY BLUE (0.53, 0.81, 0.92)     │
 ├──────────────┼──────────────────────────────┼─────────────────────────────────┤
 │ Cube color   │ White (1.0, 1.0, 1.0)        │ RED (0.90, 0.10, 0.10)          │
 ├──────────────┼──────────────────────────────┼─────────────────────────────────┤
 │ Floor plane  │ Does NOT exist               │ BEIGE (0.90, 0.85, 0.70) at y=0 │
 ├──────────────┼──────────────────────────────┼─────────────────────────────────┤
 │ Depth buffer │ Does NOT exist               │ Enabled (floor behind cubes)    │
 ├──────────────┼──────────────────────────────┼─────────────────────────────────┤
 │ Toggle       │ Works                        │ Must produce identical images   │
 └──────────────┴──────────────────────────────┴─────────────────────────────────┘
 ---
 Step-by-Step Changes

 Step 1: Change Clear Color to Sky Blue

 File: Renderer/DX12/Dx12Context.cpp (line 334)

 // FROM:
 const float clearColor[] = { 0.05f, 0.07f, 0.10f, 1.0f };
 // TO:
 const float clearColor[] = { 0.53f, 0.81f, 0.92f, 1.0f };

 ---
 Step 2: Change Cube Color to Red

 File: Renderer/DX12/ShaderLibrary.cpp (line 45)

 // FROM:
 return float4(1.0, 1.0, 1.0, 1.0);
 // TO:
 return float4(0.90, 0.10, 0.10, 1.0);

 ---
 Step 3: Create Depth Buffer and DSV Heap

 File: Renderer/DX12/Dx12Context.h - Add members:
 Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
 Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;

 File: Renderer/DX12/Dx12Context.cpp - In Initialize() after RTV creation (~line 150):
 1. Create DSV descriptor heap (1 descriptor)
 2. Create depth buffer resource (DXGI_FORMAT_D32_FLOAT, dimensions match window)
 3. Create DSV for the depth buffer

 ---
 Step 4: Enable Depth Testing in PSO

 File: Renderer/DX12/ShaderLibrary.cpp - In CreatePSO() (lines 237-239):

 // FROM:
 D3D12_DEPTH_STENCIL_DESC depthStencil = {};
 depthStencil.DepthEnable = FALSE;
 depthStencil.StencilEnable = FALSE;

 // TO:
 D3D12_DEPTH_STENCIL_DESC depthStencil = {};
 depthStencil.DepthEnable = TRUE;
 depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
 depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
 depthStencil.StencilEnable = FALSE;

 Also add DSV format to PSO:
 psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

 ---
 Step 5: Update Render Loop for Depth

 File: Renderer/DX12/Dx12Context.cpp - In Render():

 After clearing RTV (~line 335), add:
 // Clear depth buffer
 D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
 m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

 Update OMSetRenderTargets (line 342):
 // FROM:
 m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
 // TO:
 m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

 ---
 Step 6: Add Floor Geometry to RenderScene

 File: Renderer/DX12/RenderScene.h - Add members:
 Microsoft::WRL::ComPtr<ID3D12Resource> m_floorVertexBuffer;
 Microsoft::WRL::ComPtr<ID3D12Resource> m_floorIndexBuffer;
 D3D12_VERTEX_BUFFER_VIEW m_floorVbv = {};
 D3D12_INDEX_BUFFER_VIEW m_floorIbv = {};
 uint32_t m_floorIndexCount = 0;

 Add method:
 void RecordDrawFloor(ID3D12GraphicsCommandList* cmdList);

 File: Renderer/DX12/RenderScene.cpp - Add floor geometry:
 - 4 vertices: corners of a large quad at y=-0.01 (slightly below cubes)
   - (-200, -0.01, -200), (-200, -0.01, 200), (200, -0.01, 200), (200, -0.01, -200)
 - 6 indices: 2 triangles
 - Upload to DEFAULT heap same as cube geometry

 ---
 Step 7: Add Floor PSO with Beige Color

 File: Renderer/DX12/ShaderLibrary.h - Add member:
 Microsoft::WRL::ComPtr<ID3D12PipelineState> m_floorPso;
 Microsoft::WRL::ComPtr<ID3DBlob> m_floorPsBlob;

 Add accessor:
 ID3D12PipelineState* GetFloorPSO() const { return m_floorPso.Get(); }

 File: Renderer/DX12/ShaderLibrary.cpp - Add floor pixel shader:
 static const char* kFloorPixelShader = R"(
 float4 PSFloor() : SV_Target
 {
     return float4(0.90, 0.85, 0.70, 1.0);
 }
 )";

 In CompileShaders() - compile floor PS.
 In CreatePSO() - create floor PSO (same root sig, VS, depth state; different PS).

 ---
 Step 8: Render Floor Before Cubes

 File: Renderer/DX12/Dx12Context.cpp - In Render():

 After setting render state and before drawing cubes:
 // Draw floor (single draw call, always rendered first)
 m_commandList->SetPipelineState(m_shaderLibrary.GetFloorPSO());
 m_scene.RecordDrawFloor(m_commandList.Get());

 // Draw cubes
 m_commandList->SetPipelineState(m_shaderLibrary.GetPSO());
 // ... existing cube draw logic

 Update draw count logging to include floor (+1).

 ---
 Files to Modify (Summary)
 ┌─────────────────────────────────┬─────────────────────────────────────────────────────────────────────┐
 │              File               │                               Changes                               │
 ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.h     │ Add DSV heap + depth buffer members                                 │
 ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp   │ Clear color, create depth buffer, clear DSV, bind DSV, render floor │
 ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.h   │ Add floor PSO + floor PS blob members                               │
 ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.cpp │ Red cube PS, floor beige PS, floor PSO, enable depth                │
 ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/RenderScene.h     │ Add floor geometry members + RecordDrawFloor                        │
 ├─────────────────────────────────┼─────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/RenderScene.cpp   │ Create floor geometry, RecordDrawFloor                              │
 └─────────────────────────────────┴─────────────────────────────────────────────────────────────────────┘
 ---
 Verification Checklist

 - Background is sky blue (0.53, 0.81, 0.92)
 - Floor plane is visible and beige (large quad at y=-0.01)
 - Cube grid is red (100x100 cubes clearly separated)
 - Depth test works (floor behind cubes where overlapped)
 - Press 'T' - image stays IDENTICAL, only draw count changes
 - Debug log shows: mode name + draw count on toggle
 - No DX12 debug layer errors
 - No shader compile errors

 Evidence Artifacts

 - Screenshot: captures/Day1_visual_groundtruth.png
 - Log output showing:
 Mode: Instanced, Draw calls: 2 (1 floor + 1 cubes)
 Mode: Naive, Draw calls: 10001 (1 floor + 10000 cubes)

 ---
 Estimated Line Changes
 ┌──────────────────────────────────┬────────────┐
 │              Change              │   Lines    │
 ├──────────────────────────────────┼────────────┤
 │ Clear color                      │ 1          │
 ├──────────────────────────────────┼────────────┤
 │ Cube PS color                    │ 1          │
 ├──────────────────────────────────┼────────────┤
 │ DSV heap + depth buffer creation │ ~25        │
 ├──────────────────────────────────┼────────────┤
 │ Depth state in PSO               │ 5          │
 ├──────────────────────────────────┼────────────┤
 │ Clear DSV + bind DSV             │ 3          │
 ├──────────────────────────────────┼────────────┤
 │ Floor geometry (RenderScene)     │ ~40        │
 ├──────────────────────────────────┼────────────┤
 │ Floor PS shader                  │ 5          │
 ├──────────────────────────────────┼────────────┤
 │ Floor PSO creation               │ ~15        │
 ├──────────────────────────────────┼────────────┤
 │ Render floor call                │ 5          │
 ├──────────────────────────────────┼────────────┤
 │ Total                            │ ~100 lines │
 └──────────────────────────────────┴────────────┘