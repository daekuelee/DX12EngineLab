 Ready to code?

 Here is Claude's plan:                                                                                                                                                        ╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ DX12 Naive Draw Mode Bug Fix Plan                                                                                                                                             
 Problem Summary

 - Camera preset 3 (key '3'): Renders correctly (floor + 10k red cubes)
 - After mode toggle (key 'T'): Floor visible but cubes collapse to tiny fragment
 - Expected: Both modes should render identically

 Root Cause

 The DX12 Rule (Official Spec)

 From https://microsoft.github.io/hlsl-specs/proposals/0015-extended-command-info/ and https://github.com/microsoft/DirectXShaderCompiler/issues/2946:

 SV_InstanceID does NOT include StartInstanceLocation. SV_InstanceID always counts from 0 to (InstanceCount - 1).

 How This Breaks Naive Mode

 Naive draw loop (RenderScene.cpp:88-91):
 for (uint32_t i = 0; i < instanceCount; ++i)
     cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, i);  // StartInstanceLocation = i

 Shader (ShaderLibrary.cpp:85-88):
 VSOut VSMain(VSIn vin, uint iid : SV_InstanceID)
 {
     float4x4 world = Transforms[iid].M;  // iid is ALWAYS 0!

 Result: All 10,000 draws read Transforms[0] → all cubes render at same position → tiny stacked fragment visible.

 State Diff Table (Mode 3 vs Mode 4)
 ┌───────────────────────┬────────────────────────┬──────────────────────────────┐
 │         State         │ Instanced Mode (works) │     Naive Mode (broken)      │
 ├───────────────────────┼────────────────────────┼──────────────────────────────┤
 │ PSO                   │ Same cube PSO          │ Same cube PSO                │
 ├───────────────────────┼────────────────────────┼──────────────────────────────┤
 │ Root Signature        │ Same                   │ Same                         │
 ├───────────────────────┼────────────────────────┼──────────────────────────────┤
 │ Descriptor Heap       │ Same                   │ Same                         │
 ├───────────────────────┼────────────────────────┼──────────────────────────────┤
 │ SRV GPU Handle        │ Same                   │ Same                         │
 ├───────────────────────┼────────────────────────┼──────────────────────────────┤
 │ CB GPU VA             │ Same                   │ Same                         │
 ├───────────────────────┼────────────────────────┼──────────────────────────────┤
 │ VB/IB Views           │ Same                   │ Same                         │
 ├───────────────────────┼────────────────────────┼──────────────────────────────┤
 │ InstanceCount         │ 10,000                 │ 1                            │
 ├───────────────────────┼────────────────────────┼──────────────────────────────┤
 │ SV_InstanceID range   │ 0..9999                │ 0 only                       │
 ├───────────────────────┼────────────────────────┼──────────────────────────────┤
 │ StartInstanceLocation │ 0                      │ 0..9999 (ignored by shader!) │
 └───────────────────────┴────────────────────────┴──────────────────────────────┘
 ---
 Minimal Fix

 Add a root constant to pass the instance offset to the shader. This is the standard DX12 pattern per
 https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setgraphicsroot32bitconstants.

 Files to Modify

 1. ShaderLibrary.h - Add RP_InstanceOffset to enum
 2. ShaderLibrary.cpp - Root signature + HLSL shader
 3. RenderScene.cpp - Naive draw loop
 4. Dx12Context.cpp - Instanced draw binding

 ---
 Patch (Diff Style)

 1. ShaderLibrary.h (line ~13)

  enum RootParam : uint32_t
  {
      RP_FrameCB = 0,          // b0 space0
      RP_TransformsTable = 1,  // t0 space0
 +    RP_InstanceOffset = 2,   // b1 space0 (1 DWORD root constant)
      RP_Count
  };

 2. ShaderLibrary.cpp - HLSL Shader (line ~70)

  static const char* kVertexShader = R"(
  cbuffer FrameCB : register(b0, space0)
  {
      row_major float4x4 ViewProj;
  };

 +cbuffer InstanceCB : register(b1, space0)
 +{
 +    uint InstanceOffset;
 +};
 +
  struct TransformData
  {
      row_major float4x4 M;
  };
  StructuredBuffer<TransformData> Transforms : register(t0, space0);

  struct VSIn { float3 Pos : POSITION; };
  struct VSOut { float4 Pos : SV_Position; };

  VSOut VSMain(VSIn vin, uint iid : SV_InstanceID)
  {
      VSOut o;
 -    float4x4 world = Transforms[iid].M;
 +    float4x4 world = Transforms[iid + InstanceOffset].M;
      float3 worldPos = mul(float4(vin.Pos, 1.0), world).xyz;
      o.Pos = mul(float4(worldPos, 1.0), ViewProj);
      return o;
  }
  )";

 3. ShaderLibrary.cpp - Root Signature (after line ~381)

      // RP1: Transforms SRV table
      rootParams[RP_TransformsTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      // ... existing code ...

 +    // RP2: Instance offset root constant (b1 space0) - 1 DWORD
 +    rootParams[RP_InstanceOffset].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
 +    rootParams[RP_InstanceOffset].Constants.ShaderRegister = 1;  // b1
 +    rootParams[RP_InstanceOffset].Constants.RegisterSpace = 0;
 +    rootParams[RP_InstanceOffset].Constants.Num32BitValues = 1;
 +    rootParams[RP_InstanceOffset].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

 4. RenderScene.cpp - Naive Draw Loop (line ~81)

  void RenderScene::RecordDrawNaive(ID3D12GraphicsCommandList* cmdList, uint32_t instanceCount)
  {
      cmdList->IASetVertexBuffers(0, 1, &m_vbv);
      cmdList->IASetIndexBuffer(&m_ibv);

      for (uint32_t i = 0; i < instanceCount; ++i)
      {
 +        cmdList->SetGraphicsRoot32BitConstants(2, 1, &i, 0);  // RP_InstanceOffset
 -        cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, i);
 +        cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
      }
  }

 5. Dx12Context.cpp - Instanced Path (before line ~542)

          m_commandList->SetPipelineState(m_shaderLibrary.GetPSO());
          if (ToggleSystem::GetDrawMode() == DrawMode::Instanced)
          {
 +            uint32_t zero = 0;
 +            m_commandList->SetGraphicsRoot32BitConstants(2, 1, &zero, 0);  // RP_InstanceOffset = 0
              m_scene.RecordDraw(m_commandList.Get(), InstanceCount);

 ---
 Verification Checklist

 Debug Layer

 - No D3D12 errors after fix
 - No root signature/shader register mismatch warnings

 Visual Verification

 - Toggle mode with 'T' key
 - Both modes render identical 100x100 cube grid
 - Floor visible in both modes
 - No z-fighting or position drift

 Debug Log Verification

 - PASS: log shows same PSO pointers in both modes
 - B1-NAIVE: log shows correct StartInstance range (but now StartInstanceLocation=0)
 - Add new log: NAIVE: InstanceOffset=%u to verify root constant is bound

 PIX Verification (Optional)

 - Capture frame in naive mode
 - Inspect root signature bindings: RP0=CBV, RP1=SRV table, RP2=root constant
 - Verify draw calls have incrementing InstanceOffset values (0, 1, 2, ... 9999)

 ---
 DX12 Rule Citations

 1. SV_InstanceID behavior: https://microsoft.github.io/hlsl-specs/proposals/0015-extended-command-info/
 "SV_InstanceID doesn't include the StartInstanceLocation values provided to the API"
 2. Root Constants usage: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setgraphicsroot32bitconstants
 Sets a group of constants in the graphics root signature.
 3. DirectX-Graphics-Samples pattern: HelloConstBuffers sample uses root constants for small per-draw data.
 --------------------------------------
 Both Debug and Release builds pass with 0 warnings and 0 errors.

  Summary of Changes
                                                                                                                                                                                 The fix adds a root constant (RP_InstanceOffset) to pass the instance index to the shader, because SV_InstanceID does NOT include StartInstanceLocation in DX12.
                                                                                                                                                                                 Files Modified:

  1. ShaderLibrary.h:11-17 - Added RP_InstanceOffset = 2 to the RootParam enum
  2. ShaderLibrary.cpp:70-93 - HLSL shader changes:
    - Added cbuffer InstanceCB : register(b1, space0) with uint InstanceOffset
    - Changed transform lookup from Transforms[iid] to Transforms[iid + InstanceOffset]
  3. ShaderLibrary.cpp:377-387 - Root signature changes:
    - Added RP_InstanceOffset as a 32-bit root constant bound to b1 space0
  4. RenderScene.cpp:81-93 - Naive draw loop:
    - Added SetGraphicsRoot32BitConstants(2, 1, &i, 0) before each draw call
    - Changed StartInstanceLocation from i to 0 (no longer needed)
  5. Dx12Context.cpp:540-546 - Instanced draw path:
    - Added SetGraphicsRoot32BitConstants(2, 1, &zero, 0) to set offset to 0

  How It Works:
  ┌───────────┬─────────────────────────────┬─────────────────────────────────────────────────────────────────────┐
  │   Mode    │         Before Fix          │                              After Fix                              │
  ├───────────┼─────────────────────────────┼─────────────────────────────────────────────────────────────────────┤
  │ Instanced │ SV_InstanceID = 0..9999     │ InstanceOffset=0, SV_InstanceID=0..9999 → reads Transforms[0..9999] │
  ├───────────┼─────────────────────────────┼─────────────────────────────────────────────────────────────────────┤
  │ Naive     │ SV_InstanceID = 0 (always!) │ InstanceOffset=i, SV_InstanceID=0 → reads Transforms[i]             │
  └───────────┴─────────────────────────────┴─────────────────────────────────────────────────────────────────────┘