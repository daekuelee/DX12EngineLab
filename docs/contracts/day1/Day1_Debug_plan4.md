/plan
First priority: add proof instrumentation to eliminate ambiguity before further rendering changes.

Add a debug overlay OR OutputDebugString logs that report:
- Client size (GetClientRect): W,H
- Viewport: TopLeftX,TopLeftY,Width,Height
- Scissor: left,top,right,bottom (+ derived W,H)
- Mode (instanced/naive), instanceCount, draw call count

Additionally, add a simple visual marker pass to locate the actual drawable region:
- Draw 4 tiny colored triangles (or points) near the intended corners of the viewport/scissor:
  (0,0), (W-1,0), (0,H-1), (W-1,H-1) in screen space OR in NDC mapped to those corners.
- This should make it obvious if drawing is confined to top-left or any sub-rect.

Log/overlay should update on:
- startup
- when pressing 'T'
- on window resize events (WM_SIZE) if present; otherwise once per second.

Only after instrumentation proves the mismatch, fix viewport/scissor so drawing covers the full client area in BOTH modes.
Do not change camera/viewproj/floor/depth until this is proven.


------ plan

The plan has been updated with the instrumentation-first approach:

  Phase 1: Diagnostic Logging                                                                                                               - Log client size, viewport, scissor, mode, instances, draws
  - Trigger on startup, 'T' press, and periodically                                                                                       
  Phase 2: Visual Corner Markers
  - 4 magenta triangles at NDC corners (-1,-1), (1,-1), (-1,1), (1,1)
  - Pass-through vertex shader (no transforms)
  - Proves where the actual drawable region is

  Phase 3: Fix (only after proof)
  - Update viewport/scissor per-frame if mismatch is proven

  No camera/viewproj/floor/depth changes until instrumentation proves the issue.

────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                           Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day1 Visual Ground Truth - Instrumentation First

 Priority: Prove the Problem Before Fixing

 Add diagnostic instrumentation to eliminate ambiguity about viewport/scissor mismatch.
 Do NOT change camera/viewproj/floor/depth until instrumentation proves the issue.

 ---
 Step 1: Add Diagnostic Logging

 File: Renderer/DX12/Dx12Context.cpp

 Add OutputDebugString logs reporting:
 - Client size from GetClientRect(m_hwnd, &rect): W, H
 - Viewport: TopLeftX, TopLeftY, Width, Height
 - Scissor: left, top, right, bottom (+ derived W, H)
 - Mode (instanced/naive), instanceCount, draw call count

 Trigger logging on:
 1. Startup (first frame)
 2. When pressing 'T' (mode toggle)
 3. On WM_SIZE if handled, otherwise once per second

 Implementation location: In Render() after existing evidence logging (~line 457)

 // Diagnostic: log viewport/scissor/client mismatch
 static uint64_t s_lastLogFrame = 0;
 static bool s_logOnToggle = false;
 bool shouldLog = (m_frameId == 0) || s_logOnToggle || ((m_frameId - s_lastLogFrame) >= 60);
 if (shouldLog) {
     RECT clientRect;
     GetClientRect(m_hwnd, &clientRect);
     char diagBuf[512];
     sprintf_s(diagBuf,
         "DIAG[%llu]: client=%dx%d viewport=(%.0f,%.0f,%.0f,%.0f) scissor=(%ld,%ld,%ld,%ld)=%ldx%ld mode=%s instances=%u draws=%u\n",
         m_frameId,
         clientRect.right - clientRect.left, clientRect.bottom - clientRect.top,
         m_viewport.TopLeftX, m_viewport.TopLeftY, m_viewport.Width, m_viewport.Height,
         m_scissorRect.left, m_scissorRect.top, m_scissorRect.right, m_scissorRect.bottom,
         m_scissorRect.right - m_scissorRect.left, m_scissorRect.bottom - m_scissorRect.top,
         ToggleSystem::GetDrawModeName(), InstanceCount, drawCalls);
     OutputDebugStringA(diagBuf);
     s_lastLogFrame = m_frameId;
     s_logOnToggle = false;
 }

 File: DX12EngineLab.cpp - In WM_KEYDOWN handler for 'T'

 Set a flag to trigger diagnostic logging on next frame:
 // After ToggleDrawMode() call, set flag for diagnostic log
 extern bool g_logOnNextFrame; // or use ToggleSystem static

 ---
 Step 2: Add Visual Corner Markers

 Draw 4 small colored triangles at the intended viewport corners to visually prove drawable region.

 File: Renderer/DX12/RenderScene.h - Add:
 Microsoft::WRL::ComPtr<ID3D12Resource> m_markerVertexBuffer;
 D3D12_VERTEX_BUFFER_VIEW m_markerVbv = {};
 void RecordDrawMarkers(ID3D12GraphicsCommandList* cmdList);

 File: Renderer/DX12/RenderScene.cpp - Add marker geometry:
 bool RenderScene::CreateMarkerGeometry(ID3D12Device* device, ID3D12CommandQueue* queue)
 {
     // 4 small triangles at NDC corners: (-1,-1), (1,-1), (-1,1), (1,1)
     // Each triangle is 3 vertices, total 12 vertices
     // Use distinct colors: red, green, blue, yellow
     struct MarkerVertex { float x, y, z; };

     const float s = 0.05f; // Size in NDC
     const MarkerVertex vertices[] = {
         // Bottom-left (NDC -1,-1) - will appear top-left on screen
         {-1.0f, -1.0f, 0.5f}, {-1.0f+s, -1.0f, 0.5f}, {-1.0f, -1.0f+s, 0.5f},
         // Bottom-right (NDC 1,-1)
         {1.0f-s, -1.0f, 0.5f}, {1.0f, -1.0f, 0.5f}, {1.0f, -1.0f+s, 0.5f},
         // Top-left (NDC -1,1)
         {-1.0f, 1.0f-s, 0.5f}, {-1.0f+s, 1.0f, 0.5f}, {-1.0f, 1.0f, 0.5f},
         // Top-right (NDC 1,1)
         {1.0f-s, 1.0f, 0.5f}, {1.0f, 1.0f, 0.5f}, {1.0f, 1.0f-s, 0.5f},
     };
     // ... upload to DEFAULT heap
 }

 void RenderScene::RecordDrawMarkers(ID3D12GraphicsCommandList* cmdList)
 {
     cmdList->IASetVertexBuffers(0, 1, &m_markerVbv);
     cmdList->DrawInstanced(12, 1, 0, 0); // 4 triangles x 3 verts
 }

 File: Renderer/DX12/ShaderLibrary.cpp - Add marker shaders:
 // Marker VS - pass-through (vertices already in NDC)
 float4 VSMarker(float3 pos : POSITION) : SV_Position
 {
     return float4(pos, 1.0);
 }

 // Marker PS - bright magenta for visibility
 float4 PSMarker() : SV_Target
 {
     return float4(1.0, 0.0, 1.0, 1.0); // Magenta
 }

 File: Renderer/DX12/Dx12Context.cpp - Draw markers after floor/cubes:
 // Draw corner markers (uses pass-through VS, no transforms)
 m_commandList->SetPipelineState(m_shaderLibrary.GetMarkerPSO());
 m_scene.RecordDrawMarkers(m_commandList.Get());

 ---
 Step 3: After Instrumentation - Fix Viewport/Scissor (If Proven)

 Only after logs show mismatch, update viewport/scissor to match client rect per-frame:

 File: Renderer/DX12/Dx12Context.cpp - In Render() before RSSetViewports:
 // Update viewport/scissor from current client rect
 RECT clientRect;
 GetClientRect(m_hwnd, &clientRect);
 uint32_t clientW = clientRect.right - clientRect.left;
 uint32_t clientH = clientRect.bottom - clientRect.top;
 if (clientW != m_width || clientH != m_height) {
     m_width = clientW;
     m_height = clientH;
     m_viewport = { 0.0f, 0.0f, (float)m_width, (float)m_height, 0.0f, 1.0f };
     m_scissorRect = { 0, 0, (LONG)m_width, (LONG)m_height };
     // Note: depth buffer resize would also be needed for proper handling
 }

 ---
 Files to Modify
 ┌─────────────────────────────────┬───────────────────────────────────────────┐
 │              File               │                  Change                   │
 ├─────────────────────────────────┼───────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.cpp   │ Add diagnostic logging in Render()        │
 ├─────────────────────────────────┼───────────────────────────────────────────┤
 │ Renderer/DX12/RenderScene.h     │ Add marker geometry members               │
 ├─────────────────────────────────┼───────────────────────────────────────────┤
 │ Renderer/DX12/RenderScene.cpp   │ Create marker geometry, RecordDrawMarkers │
 ├─────────────────────────────────┼───────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.h   │ Add marker PSO member                     │
 ├─────────────────────────────────┼───────────────────────────────────────────┤
 │ Renderer/DX12/ShaderLibrary.cpp │ Add marker VS/PS, create marker PSO       │
 ├─────────────────────────────────┼───────────────────────────────────────────┤
 │ DX12EngineLab.cpp               │ (Optional) Set log flag on 'T' press      │
 └─────────────────────────────────┴───────────────────────────────────────────┘
 ---
 Verification Checklist

 1. Run app and check debug output:
   - Log shows client size matches viewport/scissor
   - Log shows mode, instanceCount=10000, correct draw count
 2. Visual verification:
   - 4 magenta corner markers visible at window corners
   - If markers are NOT at corners → viewport/scissor mismatch proven
   - If markers ARE at corners but content is wrong → camera/transform issue
 3. Toggle test:
   - Press 'T' → log updates, image should be identical
   - Only draw count changes (2 vs 10001)

 ---
 Evidence Artifacts

 - Debug output showing DIAG lines with all dimensions
 - Screenshot showing corner markers position