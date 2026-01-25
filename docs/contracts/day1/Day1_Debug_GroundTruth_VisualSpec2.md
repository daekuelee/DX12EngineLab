## Visual Target (non-negotiable)

- Clear color (background): SKY BLUE (e.g. float4(0.53, 0.81, 0.92, 1))
- Floor: a large plane at y=0, BEIGE (e.g. float4(0.90, 0.85, 0.70, 1))
- Cubes: 100x100 grid above the floor, RED (e.g. float4(0.90, 0.10, 0.10, 1))
- Depth: floor behind cubes where overlapped (depth test on)
- Camera: any view is fine as long as cubes are clearly separated and visible as a grid (not collapsed to a line)

## Mode equivalence (critical)
Instanced and Naive MUST render the SAME final image.
Only CPU time / draw count differs.

## Acceptance (DONE means)
- Background is sky blue
- Floor plane is visible and beige
- Cube grid is visible and red (many cubes, clearly separated)
- Pressing T toggles instanced/naive but the image stays identical
- No shader compile errors, no DX12 debug layer errors


----
  Summary of Changes                                                                                                                      
  Files Modified                                                                                                                            ┌─────────────────────────────────┬────────────────────────────────────────────────────────────────────────────────────────────┐
  │              File               │                                          Changes                                           │
  ├─────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/Dx12Context.h     │ Added DSV heap + depth buffer members                                                      │
  ├─────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/Dx12Context.cpp   │ Sky blue clear color, DSV/depth buffer creation, clear/bind DSV, render floor before cubes │
  ├─────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/ShaderLibrary.h   │ Added floor PSO + floor PS blob members + GetFloorPSO accessor                             │
  ├─────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/ShaderLibrary.cpp │ Red cube PS, beige floor PS, floor PSO creation, depth testing enabled                     │
  ├─────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/RenderScene.h     │ Added floor geometry members + RecordDrawFloor method                                      │
  ├─────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Renderer/DX12/RenderScene.cpp   │ CreateFloorGeometry + RecordDrawFloor implementation                                       │
  └─────────────────────────────────┴────────────────────────────────────────────────────────────────────────────────────────────┘
  Visual Changes

  - Background: Sky blue (0.53, 0.81, 0.92)
  - Cubes: Red (0.90, 0.10, 0.10) - 100x100 grid
  - Floor: Beige (0.90, 0.85, 0.70) - large quad at y=-0.01
  - Depth testing: Enabled with D3D12_COMPARISON_FUNC_LESS

  Draw Call Counts

  - Instanced mode: 2 draw calls (1 floor + 1 instanced cubes)
  - Naive mode: 10001 draw calls (1 floor + 10000 individual cubes)

  The application should now show a sky blue background with red cubes on a beige floor, with proper depth testing so the floor appears
  behind the cubes where they overlap. Pressing 'T' toggles between instanced and naive modes, producing visually identical images but
  different draw call counts.