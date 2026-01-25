 Summary of Changes                                                                                                                      
  1. Diagnostic Logging (Renderer/DX12/ToggleSystem.h)                                                                                    
  - Added ShouldLogDiagnostics(), RequestDiagnosticLog(), ClearDiagnosticLog() methods
  - Static flag s_logDiagnostics to trigger logging on demand

  2. Diagnostic Logging in Render (Renderer/DX12/Dx12Context.cpp)

  - Added DIAG logging that outputs:
    - Client rect dimensions (W x H)
    - Viewport (TopLeftX, TopLeftY, Width, Height)
    - Scissor (left, top, right, bottom) + derived W x H
    - Mode (instanced/naive), instance count, draw calls
  - Logs on: frame 0, 'T' toggle, and every 60 frames

  3. Toggle Triggers Logging (DX12EngineLab.cpp)

  - Pressing 'T' now also calls RequestDiagnosticLog() to trigger a DIAG log on next frame

  4. Visual Corner Markers (RenderScene.h/.cpp)

  - Added 4 magenta triangles at NDC corners (-1,-1), (1,-1), (-1,1), (1,1)
  - 8% screen size triangles for visibility
  - CreateMarkerGeometry() and RecordDrawMarkers() methods

  5. Marker Shader & PSO (ShaderLibrary.h/.cpp)

  - Pass-through vertex shader (vertices already in NDC)
  - Solid magenta pixel shader
  - Separate root signature (empty - no bindings needed)
  - PSO with depth test disabled (markers always on top) and no culling

  6. Marker Drawing (Dx12Context.cpp)

  - Draw markers after floor/cubes, before backbuffer transition
  - Uses separate root signature and PSO

  Verification

  When you run the app:
  1. Debug output will show DIAG[frameId]: client=WxH viewport=... scissor=... mode=... instances=10000 draws=N
  2. 4 magenta corner markers should appear at the exact corners of the viewport
  3. Press 'T' to toggle mode and trigger an immediate diagnostic log

  If markers are NOT at window corners → viewport/scissor mismatch is proven
  If markers ARE at corners but content is wrong → camera/transform issue