---my explore
We will run Microtest A first (instrumentation-first, proof-driven). Use explore -> plan -> execute.

Context / Evidence:
- DIAG confirms client/viewport/scissor all match full window (e.g., 1904x1000).
- Corner NDC magenta markers show at window corners => viewport/scissor is applied.
- Toggle works (draw counts differ), but visuals differ drastically:
  naive shows large red region; instanced shows tiny red dot.
- Hypothesis: instanced path’s per-instance transform fetch (SRV/descriptor/stride/binding) is broken.
- Goal: isolate SRV/descriptor issues by eliminating transform SRV completely.

Microtest A (must-do):
1) In the cube vertex shader, IGNORE the transforms SRV entirely.
2) Compute per-instance translation directly from SV_InstanceID:
   - uint id = SV_InstanceID
   - uint gx = id % 100;
   - uint gz = id / 100;
   - float tx = float(gx) * 2.0f - 99.0f;
   - float tz = float(gz) * 2.0f - 99.0f;
   - float3 worldPos = inPos + float3(tx, 0.0f, tz);
3) Then apply the existing viewProj (do NOT change camera/viewProj, viewport/scissor, floor, or depth settings).
4) Keep DIAG logging + corner markers enabled.
5) Verification contract:
   - Instanced and Naive must now look IDENTICAL (only draw count differs).
   - Clear color must fill full window (confirm visually).
   - Save 2 screenshots: captures/microtestA_instanced.png and captures/microtestA_naive.png
   - Copy one DIAG line for each mode into docs/contracts/MicrotestA_DIAG.md

Constraints:
- Minimal scope: only touch shader code + any plumbing strictly required to compile.
- No “guess fixes” (no resize handling, no new camera, no floor/depth changes in this microtest).
- If shader lives in ShaderLibrary.cpp string, edit that string only.
- If there are two VS paths (marker/floor/cube), only change the cube VS.

After execute:
- Report: do instanced & naive match? If yes => SRV/descriptor/stride binding is the root cause.
- Next step will be Microtest B only if needed.

---result claude

The plan is minimal and focused:

  Single change: Modify kVertexShader in ShaderLibrary.cpp to compute grid position from SV_InstanceID directly, bypassing the Transforms   SRV entirely.
                                                                                                                                            Expected result: If instanced and naive look identical after this change, the SRV/descriptor binding is confirmed as the root cause.

────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                           Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Microtest A: Bypass SRV to Prove Descriptor/Binding Issue

 Goal

 Isolate whether the SRV/descriptor/stride binding is the root cause of the visual difference between instanced and naive modes by
 completely bypassing the transforms SRV in the shader.

 Evidence So Far

 - DIAG confirms client/viewport/scissor all match (e.g., 1904x1000)
 - Corner markers appear at window corners (viewport/scissor is correct)
 - Toggle works (draw counts differ: 3 vs 10002)
 - Problem: naive shows large red region; instanced shows tiny red dot
 - Hypothesis: instanced path's per-instance transform fetch (SRV/descriptor/stride/binding) is broken

 ---
 Implementation

 Single File Change: Renderer/DX12/ShaderLibrary.cpp

 Modify kVertexShader (lines 10-40) to compute translation directly from SV_InstanceID instead of reading from the Transforms SRV.

 Current shader (broken for instanced):
 VSOut VSMain(VSIn vin, uint iid : SV_InstanceID)
 {
     VSOut o;
     float4x4 M = Transforms[iid].M;              // <-- SRV fetch (suspected broken)
     float4 wpos = mul(float4(vin.Pos, 1.0), M);
     o.Pos = mul(wpos, ViewProj);
     return o;
 }

 Microtest A shader (bypass SRV):
 VSOut VSMain(VSIn vin, uint iid : SV_InstanceID)
 {
     VSOut o;
     // Microtest A: bypass transforms SRV, compute grid position from instance ID
     uint gx = iid % 100;
     uint gz = iid / 100;
     float tx = float(gx) * 2.0f - 99.0f;
     float tz = float(gz) * 2.0f - 99.0f;
     float3 worldPos = vin.Pos + float3(tx, 0.0f, tz);
     o.Pos = mul(float4(worldPos, 1.0), ViewProj);
     return o;
 }

 What NOT to Change

 - ViewProj / camera setup (Dx12Context.cpp)
 - Viewport / scissor
 - Floor shader/geometry
 - Depth buffer settings
 - Marker shaders
 - Root signature / PSO structure (shader still declares the SRV, just doesn't use it)

 ---
 Verification Contract

 Expected Outcome if SRV is the Root Cause

 1. Instanced and Naive modes look IDENTICAL (same 100x100 red cube grid)
 2. Only draw count differs (3 instanced vs 10002 naive)
 3. Clear color (sky blue) fills full window behind the grid
 4. Corner markers remain at corners

 Evidence to Capture

 1. captures/microtestA_instanced.png - screenshot in instanced mode
 2. captures/microtestA_naive.png - screenshot in naive mode
 3. docs/contracts/MicrotestA_DIAG.md - one DIAG line for each mode

 Pass/Fail Criteria

 - PASS: Both screenshots show identical 100x100 grid layout
   - Confirms SRV/descriptor binding is broken
   - Next step: fix SRV binding (Microtest B not needed)
 - FAIL: Still different visuals
   - SRV is NOT the only issue
   - Need Microtest B to investigate further

 ---
 Files to Modify
 ┌─────────────────────────────────┬─────────────────────────────────────────────┐
 │              File               │                   Change                    │
 ├─────────────────────────────────┼─────────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.cpp │ Replace VSMain body in kVertexShader string │
 └─────────────────────────────────┴─────────────────────────────────────────────┘
 ---
 Build & Run

 msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64
 # Run app, capture screenshots, copy DIAG lines