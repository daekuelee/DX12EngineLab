PHASE: PLAN (do NOT implement yet)
Goal: Produce a slice-by-slice debug plan to fix “Exploding / giant triangles” rendering + make proof toggles safe.
Must be directly actionable and keep build green at every slice.

=== Ground Truth Inputs (must read first) ===
1) docs/contracts/Day1_Debug_IssuePacket_ExplodingTriangles.md   (bug contract)
2) docs/contracts/Day1_InstancingVsNaive.md                      (Day1 intent)
3) Current code state in repo (use your EXPLORE findings)

=== Hard Rules ===
- No implementation in this response.
- No large code blocks.
- Each slice must change ONE hypothesis lever at a time.
- “Toggle 3 crash” must be addressed: if it’s API misuse, change the *proof design* (not just “yeah it crashes”).

=== Required Plan Content ===
0) What is the primary failure mode?
- Name it (1–2 lines) in LLM-friendly terms.

1) Hypotheses (ranked)
- At minimum cover:
  H1: Matrix layout mismatch (row/column-major) between CPU data and HLSL
  H2: mul order mismatch (mul(v,M) vs mul(M,v))
  H3: ViewProj CB not matching HLSL expectations (transpose/packing)
  H4: Root param misuse in proof toggle 3 causing undefined behavior/driver crash
  H5: Index buffer format/type mismatch (R16 vs R32) (lower probability but must rule out)

2) Slice plan S0..Sn (debug slices)
For each slice:
- Hypothesis tested
- Minimal change scope (which file(s))
- Expected visual/log outcome
- Acceptance criteria (observable)
- Rollback / next step if fails
IMPORTANT: S0 should be “make proofs safe” and “baseline capture”:
- disable/replace toggle 3 if it’s API misuse
- add one-line runtime overlay/log for current mode + toggles (if already exists, just specify)

3) Evidence plan
- What screenshots to take per slice (names)
- What debug output to log per slice (exact fields)

4) Output files
- Update or create: docs/contracts/Day1_Debug_Plan_ExplodingTriangles.md (store this plan)
- If you propose doc changes, give patch-style diffs only

=== Output Format (strict) ===
- 0) Problem name
- 1) Hypotheses ranked
- 2) Slices S0..Sn
- 3) Evidence plan
- 4) Files to write/update
