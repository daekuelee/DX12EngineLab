────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                                               Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day1 Debug: Fix Collapsed Geometry (Horizontal Line)

 Problem Summary

 The 100x100 cube grid renders as a single horizontal white line instead of a 3D grid of cubes.

 Root Cause Analysis

 File: Renderer/DX12/Dx12Context.cpp:227-242

 The current ViewProj matrix is a simple orthographic projection looking along the -Z axis:
 float viewProj[16] = {
     scale / aspect, 0.0f,  0.0f, 0.0f,   // X scaled
     0.0f,           scale, 0.0f, 0.0f,   // Y scaled
     0.0f,           0.0f,  1.0f, 0.0f,   // Z pass-through (PROBLEM!)
     0.0f,           0.0f,  0.0f, 1.0f
 };

 The cube grid layout (lines 248-255):
 - X spans: -99 to +99
 - Y = 0 (all cubes on same horizontal plane)
 - Z spans: -99 to +99

 Why it fails:
 1. The camera looks along -Z axis (default), viewing the XY plane face-on
 2. Cubes are spread across the XZ plane with Y=0
 3. From the camera's viewpoint, all cubes are at the same Y (screen vertical) = 0
 4. Result: all cubes project to a horizontal line at screen center
 5. Additionally, Z values (-99 to +99) pass through unchanged, exceeding NDC [0,1] range, causing most cubes to be clipped

 Solution: Top-Down Orthographic Camera

 Create a proper View + Projection matrix that looks down at the XZ grid from above:
 - Camera at (0, 100, 0) looking down -Y axis
 - World X → Screen X
 - World Z → Screen Y (vertical on screen)
 - World Y → Screen depth (NDC Z)

 ViewProj Matrix (Row-Major for v * M)

 Combining view rotation + orthographic projection:
 float scale = 0.01f;          // Maps -100..100 to -1..1
 float zScale = 0.005f;        // 1/(far-near) = 1/199 ≈ 0.005
 float zOffset = 0.495f;       // Centers grid in NDC Z

 float viewProj[16] = {
     scale / aspect,  0.0f,     0.0f,     0.0f,   // World X → NDC X
     0.0f,            0.0f,     zScale,   0.0f,   // World Z → NDC Z (depth)
     0.0f,           -scale,    0.0f,     0.0f,   // World Y → NDC Y (screen up, negated for DX coord)
     0.0f,            0.0f,     zOffset,  1.0f    // Translation
 };

 Verification of transform:
 - World (0, 0, 0) → NDC (0, 0, 0.495) ✓ centered
 - World (99, 0, 99) → NDC (0.99, 0, 0.99) ✓ within bounds
 - World (-99, 0, -99) → NDC (-0.99, 0, 0.01) ✓ within bounds

 Files to Modify

 - Renderer/DX12/Dx12Context.cpp:234-239 (ViewProj matrix)

 Implementation

 Replace lines 234-239:
 // Before (looking along -Z, sees XY plane edge-on)
 float viewProj[16] = {
     scale / aspect, 0.0f,  0.0f, 0.0f,
     0.0f,           scale, 0.0f, 0.0f,
     0.0f,           0.0f,  1.0f, 0.0f,
     0.0f,           0.0f,  0.0f, 1.0f
 };

 // After (top-down view, sees XZ plane from above)
 float zScale = 0.005f;   // 1/(far-near), maps Y to NDC depth
 float zOffset = 0.5f;    // Center the depth range

 float viewProj[16] = {
     scale / aspect,  0.0f,     0.0f,     0.0f,   // X → screen X
     0.0f,            0.0f,     zScale,   0.0f,   // Z → depth
     0.0f,           -scale,    0.0f,     0.0f,   // -Y → screen Y
     0.0f,            0.0f,     zOffset,  1.0f
 };

 Verification

 1. Build Debug x64 in VS2022
 2. Run DX12EngineLab.exe
 3. Expected: See a grid of white cubes from top-down view
 4. Toggle instanced/naive with 'T' - both should render identical images
 5. No DX12 debug layer errors
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌