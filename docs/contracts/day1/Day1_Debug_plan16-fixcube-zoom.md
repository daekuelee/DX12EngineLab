 I now have complete code evidence. Let me analyze the winding order bug and write the plan.

  Key Finding: The index buffer has inconsistent winding. Let me verify by tracing the vertices:                                                                    
  Vertices:                                                                                                                                                           - 0-3: Front face (z=-1): BL(-1,-1,-1), TL(-1,+1,-1), TR(+1,+1,-1), BR(+1,-1,-1)
  - 4-7: Back face (z=+1): BL(-1,-1,+1), TL(-1,+1,+1), TR(+1,+1,+1), BR(+1,-1,+1)

  Top face (+Y): 1, 6, 5, 1, 2, 6
  - Triangle 1→6→5 viewed from above: front-left → back-right → back-left = CLOCKWISE ✓
  - Triangle 1→2→6 viewed from above: front-left → front-right → back-right = CLOCKWISE ✓

  Front face (-Z): 0, 1, 2, 0, 2, 3
  - Triangle 0→1→2 viewed from -Z: bottom-left → top-left → top-right = COUNTER-CLOCKWISE ✗
  - This is treated as back-facing and CULLED!

  The top face is the only face with correct clockwise winding. All other faces have CCW winding when viewed from outside, so they're culled.

  Let me write the plan.

● Updated plan
  ⎿  Wrote 126 lines to ../../../../../../../home/daeku/.claude/plans/rosy-soaring-dragon.md
     # Plan: Fix Cube Face Rendering + Debug Face Colors

     ## Problem Analysis                                                                                                                                            
     **Root Cause:** The cube index buffer has **inconsistent winding order**.                                                                                      
     ### Code Evidence

     **Rasterizer settings** (`ShaderLibrary.cpp:480-483`):
     ```cpp
     … +116 lines (ctrl+o to expand)

● I've written the plan with code evidence and the debug face color implementation. The key finding is that 5 out of 6 faces have incorrect counter-clockwise
  winding and are being culled - only the top face has correct clockwise winding.
                                                                                                                                                                    ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                     Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Plan: Fix Cube Face Rendering + Debug Face Colors

 Problem Analysis

 Root Cause: The cube index buffer has inconsistent winding order.

 Code Evidence

 Rasterizer settings (ShaderLibrary.cpp:480-483):
 rasterizer.CullMode = D3D12_CULL_MODE_BACK;
 rasterizer.FrontCounterClockwise = FALSE;  // CW = front-facing

 Current index buffer (RenderScene.cpp:124-137):
 ┌─────────────┬─────────────┬───────────────────────────────┬─────────┐
 │    Face     │   Indices   │ Winding (viewed from outside) │ Result  │
 ├─────────────┼─────────────┼───────────────────────────────┼─────────┤
 │ -Z (front)  │ 0,1,2,0,2,3 │ CCW                           │ CULLED  │
 ├─────────────┼─────────────┼───────────────────────────────┼─────────┤
 │ +Z (back)   │ 4,5,6,4,6,7 │ CCW                           │ CULLED  │
 ├─────────────┼─────────────┼───────────────────────────────┼─────────┤
 │ -X (left)   │ 4,5,1,4,1,0 │ CCW                           │ CULLED  │
 ├─────────────┼─────────────┼───────────────────────────────┼─────────┤
 │ +X (right)  │ 3,2,6,3,6,7 │ CCW                           │ CULLED  │
 ├─────────────┼─────────────┼───────────────────────────────┼─────────┤
 │ +Y (top)    │ 1,6,5,1,2,6 │ CW                            │ Renders │
 ├─────────────┼─────────────┼───────────────────────────────┼─────────┤
 │ -Y (bottom) │ 4,3,0,4,7,3 │ CCW                           │ CULLED  │
 └─────────────┴─────────────┴───────────────────────────────┴─────────┘
 Vertex layout:
 Front (z=-1):     Back (z=+1):
 1---2             5---6
 |   |             |   |
 0---3             4---7

 ---
 Implementation Plan

 Step 1: Fix Index Buffer Winding Order

 File: Renderer/DX12/RenderScene.cpp (lines 124-137)

 Replace the index buffer with correct clockwise winding for all faces when viewed from outside:

 const uint16_t indices[] = {
     // -Z face (front) - CW viewed from -Z
     0, 2, 1,  0, 3, 2,
     // +Z face (back) - CW viewed from +Z
     4, 6, 5,  4, 7, 6,     // was 4,5,6,4,6,7 (CCW)
     // -X face (left) - CW viewed from -X
     0, 1, 5,  0, 5, 4,     // was 4,5,1,4,1,0 (CCW)
     // +X face (right) - CW viewed from +X
     3, 6, 2,  3, 7, 6,     // was 3,2,6,3,6,7 (CCW)
     // +Y face (top) - CW viewed from +Y (already correct)
     1, 2, 6,  1, 6, 5,     // reordered for consistency
     // -Y face (bottom) - CW viewed from -Y
     0, 4, 7,  0, 7, 3      // was 4,3,0,4,7,3 (CCW)
 };

 Face order mapping (for shader faceIndex):
 - faceIndex 0: -Z (front)
 - faceIndex 1: +Z (back)
 - faceIndex 2: -X (left)
 - faceIndex 3: +X (right)
 - faceIndex 4: +Y (top)
 - faceIndex 5: -Y (bottom)

 Step 2: Add Debug Face Color Mode

 File: ShaderLibrary.cpp (pixel shader ~line 112)

 Add a static debug color array and output directly for visual verification:

 float4 PSMain(PSIn pin, uint primID : SV_PrimitiveID) : SV_Target
 {
     // DEBUG MODE: Face-based colors
     static const float3 faceDebugColors[6] = {
         float3(0, 1, 0),      // 0: -Z (front): Green
         float3(1, 1, 0),      // 1: +Z (back): Yellow
         float3(0, 0, 1),      // 2: -X (left): Blue
         float3(1, 0.5, 0),    // 3: +X (right): Orange
         float3(1, 0, 0),      // 4: +Y (top): Red
         float3(0, 1, 1),      // 5: -Y (bottom): Cyan
     };

     uint faceIndex = (primID % 12) / 2;
     return float4(faceDebugColors[faceIndex], 1.0);
 }

 Color scheme (matching your spec):
 - Top = Red (visible from camera above)
 - Front = Green (should now be visible)
 - Right = Orange (should now be visible)
 - Back = Yellow
 - Left = Blue
 - Bottom = Cyan

 ---
 Files to Modify

 1. Renderer/DX12/RenderScene.cpp - Fix index buffer winding (lines 124-137)
 2. Renderer/DX12/ShaderLibrary.cpp - Add debug face colors (lines 112-148)

 ---
 Verification

 1. Build: msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64
 2. Run the application
 3. Expected result:
   - Each cube face should have a distinct solid color
   - From the current camera angle, you should see:
       - Red (top)
     - Green (front) - newly visible
     - Orange (right) or Blue (left) depending on camera position
 4. After verification: Remove debug colors, restore normal lighting shader

 ---
 Rollback Plan

 If debug mode confirms faces are rendering correctly, restore the original lighting shader by removing the debug color output and uncommenting the normal lighting
  code.

  ---------------------------------
  The WSL environment can't execute Windows binaries directly. The code changes have been made successfully. Here's a summary:

  Changes Made                                                                                                                                                      
  1. Renderer/DX12/RenderScene.cpp (lines 124-137)                                                                                                                    - Fixed index buffer winding order to clockwise for all faces when viewed from outside
  - Face order preserved: -Z, +Z, -X, +X, +Y, -Y

  2. Renderer/DX12/ShaderLibrary.cpp (lines 105-127)
  - Replaced lighting shader with debug face colors:
    - Green = -Z (front)
    - Yellow = +Z (back)
    - Blue = -X (left)
    - Orange = +X (right)
    - Red = +Y (top)
    - Cyan = -Y (bottom)

  Build & Run

  Build from Visual Studio or command line:
  msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64

  Expected Result

  When you run the application, each cube face should display its debug color. From the current camera angle (above and slightly to the front), you should see:
  - Red tops (looking down at +Y faces)
  - Green front faces (-Z)
  - Orange right faces (+X) or Blue left faces (-X) depending on camera position

  If faces are still missing after this fix, the issue lies elsewhere (camera/transform problem). The debug colors will make it obvious which faces are rendering.