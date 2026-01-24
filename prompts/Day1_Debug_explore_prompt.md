You are in EXPLORE mode. Investigate a DX12 app where runtime shader compilation fails.

Facts:
- Build succeeds (Debug/Release). App fails at runtime shader compilation:
  "VS compile error: ... VSMain(7,18-26): error X3000: syntax error: unexpected token 'row_major'"
  "ShaderLibrary: Failed to compile shaders" then app exits.

Goal of this EXPLORE:
- Find exactly where 'row_major' appears in the embedded HLSL source and why FXC rejects it.
- Identify the shader compiler path (D3DCompile / D3DCompile2 / DXC), the flags used, and the exact HLSL string that gets compiled.
- Produce a concise report with:
  1) The exact HLSL snippet around the failing line with line numbers as seen by the compiler
  2) The compile call site (file/function) and flags
  3) The safest correction options that work under D3DCompiler_47 (FXC), without guessing

Constraints:
- Do not assume previous explore is correct; verify in code.
- Focus only on what is needed to fix this compile failure and then unblock the original exploding triangles issue.

Use these repo notes as input context:
- docs/contracts/Day1_Debug_IssuePacket_ExplodingTriangles.md
- docs/contracts/Day1_Debug_explore.md
- docs/contracts/Day1_Debug_RuntimeLog_RowMajorCompileFail.md

Write the explore output to:
docs/contracts/Day1_Debug_explore_rowmajor_fix.md
