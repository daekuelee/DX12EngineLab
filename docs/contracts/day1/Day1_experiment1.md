Microtest A implementation complete.

  Summary of changes:                                                                                                                       - Modified Renderer/DX12/ShaderLibrary.cpp lines 32-43
  - The vertex shader now computes grid positions directly from SV_InstanceID instead of reading from the Transforms SRV                    - The SRV declaration remains (root signature compatibility) but is not used

  Build status: Success (0 errors, 0 warnings)

  Next steps:
  1. Run the app in instanced mode (default) - capture captures/microtestA_instanced.png
  2. Press D to toggle to naive mode - capture captures/microtestA_naive.png
  3. Copy DIAG lines for both modes

  Expected outcome if SRV is the root cause:
  - Both modes show identical 100×100 red cube grid
  - Only draw count differs (3 vs 10002)

  If visuals are still different:
  - SRV binding is NOT the only issue
  - Will need Microtest B to investigate further