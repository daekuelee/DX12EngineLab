PHASE: EXPLORE (do NOT implement / do NOT edit files)
Goal: Identify the exact code paths that control (1) index buffer setup, (2) HLSL matrix multiply convention, (3) CPU ViewProj + transform matrix write/transpose, and (4) why toggle '3' can crash (nvwgf2umx).

=== Ground Truth (authoritative) ===
Read and treat as the bug contract:
- docs/contracts/Day1_Debug_IssuePacket_ExplodingTriangles.md

=== Rules ===
- NO CODE CHANGES. No formatting edits. No “quick fix”.
- Output must be short, evidence-based, and cite file paths + line ranges (approx ok if tool can’t give exact lines).
- If multiple plausible causes, rank them with reasoning tied to evidence.

=== What to find (exact) ===
A) Index buffer creation details:
- Where indices are defined (type: uint16/uint32), where IBV.Format is set (DXGI_FORMAT_R16_UINT / R32_UINT),
- Ensure DrawIndexedInstanced uses correct index count and buffer contents.

B) HLSL vertex shader multiply convention:
- Locate the vertex shader source (embedded string or file) and extract only the relevant lines:
  - float4 wpos = mul(...);
  - o.Pos = mul(...);
- Note whether row_major / column_major is specified anywhere.

C) CPU-side ViewProj constant buffer write:
- Find where ViewProj is computed and written into the CB mapped memory.
- Check whether transpose is applied (or not).
- Identify matrix layout assumption (row-major vs column-major) based on code.

D) CPU-side per-instance Transform matrix write:
- Find where Transforms[i] matrix is written.
- Confirm where translation is placed (indices [12..14] vs [3,7,11] etc).
- Confirm whether data is written as row-major or column-major and whether shader expects the same.

E) Root signature + binding:
- Confirm RP indices and root param types in root signature creation.
- Confirm SetGraphicsRootConstantBufferView uses a CBV root param index.
- Confirm SetGraphicsRootDescriptorTable uses a descriptor-table root param index.
- Analyze why “break_RPIndexSwap” (toggle 3) can crash: is it API misuse (wrong root param type)? Provide evidence.

=== Output format (strict) ===
1) Repo paths found (bullet list, each with what it controls)
2) Evidence snippets (max 10 lines each, minimal)
3) Suspect ranking (A/B/C/D) with 1–2 sentences each
4) Safety note: whether toggle 3 is “expected crash” (API misuse) vs “safe proof toggle”
