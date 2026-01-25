ROLE
You are a senior AAA DX12 engine engineer + technical writer.
Your job is to synthesize a coherent debugging narrative from existing project logs and git history.
Be concise, evidence-based, and link to real files/commits.

REPO
daekuelee/DX12EngineLab

GOAL
Create a high-quality Day1 debugging narrative document and add a Day1 summary snippet to README.
We already have many Day1 debug logs/plans/issue packets under docs/contracts/day1/.
We also have Day1 commits + diffs in git history. Use both as ground truth.

SCOPE
- Docs-only + README update. Do NOT refactor engine code in this task.
- Do NOT delete or rename existing Day1 docs; keep them as raw evidence. The new document should summarize and link to them.

INPUT SOURCES YOU MUST USE
1) docs/contracts/day1/* (all Day1_Debug_*.md, InstancingVsNaive*.md, RuntimeLog*.md, IssuePacket*.md, etc.)
2) git history for Day1:
   - Use `git log` to find Day1 commits relevant to instancing vs naive, SRV heap, transforms SRV, root signature, shader binding, camera preset debug, etc.
   - Use `git show` / `git diff` to extract what changed and why.

OUTPUTS (FILES TO CREATE / EDIT)
A) Create: docs/debug/day1/dailyNote.md
B) Edit: README.md (add a “Day 1” section that summarizes what was done and links to the dailyNote + key evidence docs)

DOCUMENT REQUIREMENTS (dailyNote.md)
Write it as a narrative that an interviewer/engineer can follow.
Must include:

1) One-paragraph “Goal & Initial Symptom”
   - What was broken (e.g., Instanced vs Naive mismatch, wrong positions, flicker, etc.)
   - What “success” means.

2) Timeline (Evidence-based)
   - A chronological list of key checkpoints:
     - Experiment/Microtest names (from day1 docs)
     - What was changed (refer to commit hashes + file paths)
     - What was observed (logs/screenshots references if present)
   - Keep it readable: 8–15 bullets max.

3) Root Cause(s) and Why It Happened
   - Binding chain explanation in plain DX12 terms:
     TransformBuffer → SRV descriptor → heap slot → GPU handle → SetRootDescriptorTable → shader reads t0
   - Call out the specific bug(s) discovered (descriptor stomp, wrong SRV format/stride, wrong slot, wrong fence gating, etc.)
   - If multiple hypotheses were tested, list which were ruled out and how.

4) Fix Summary (Patch-level)
   - What final changes solved it (by file path + commit hash)
   - Why the fix works (tie back to invariants)

5) Proof / Verification
   - Minimal proof points:
     - Which debug logs confirm correct descriptor addresses/slots
     - Which microtest shows instanced==naive (or expected equivalence)
     - Any before/after screenshot references (only if available in repo)

6) “Takeaways / Invariants” (for future)
   - 5–10 bullet invariants/contracts (e.g., fence-gated reuse, per-frame slice allocation, SRV stride rules)
   - Include a short “Next Step” section pointing to Day1.5 refactoring focus.

7) Link Index (Raw Evidence)
   - At the end: link to the most important original Day1 docs (5–12 links), grouped by:
     - Plans
     - Issue packets
     - Runtime logs
     - Experiments

README UPDATE REQUIREMENTS
- Add a “Day 1” section near the top (or in a Devlog/Progress section if it exists).
- Keep it short (6–10 bullets).
- Include links to:
  - docs/debug/day1/dailyNote.md (primary)
  - 2–4 key evidence docs under docs/contracts/day1/
- Include “Outcome” and “Key learning (1–2 bullets)” and “Next: Day1.5 refactor target”.

QUALITY BAR
- No fluff. Every major claim should be backed by a file link or commit diff reference.
- Prefer concrete facts: exact filenames, commit IDs, log strings, slot numbers, etc.
- Avoid giant paste. Summarize and cite the raw docs instead.

PROCESS (DO THIS IN ORDER)
1) Scan docs/contracts/day1/ and write a short outline of the narrative.
2) Scan git log for Day1 and pick the key commits; map them onto the outline.
3) Generate docs/debug/day1/dailyNote.md
4) Update README.md with the Day1 summary + links.
5) Print the final list of files created/edited.

DELIVERABLE AT END OF RUN
- A brief note: “If anything is missing (e.g., images not committed), list what and where to add it.”

--------------------------------------------------------
 Ready to code?
                                                                                                                                                                                         Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day1 Debugging Narrative Document Plan

 Goal

 Create docs/debug/day1/dailyNote.md - a cohesive debugging narrative synthesized from Day1 logs and git history. Update README with a Day1 summary section.

 ---
 Files to Create/Edit
 ┌────────┬──────────────────────────────┬──────────────────────────┐
 │ Action │             File             │       Description        │
 ├────────┼──────────────────────────────┼──────────────────────────┤
 │ CREATE │ docs/debug/day1/dailyNote.md │ Main narrative document  │
 ├────────┼──────────────────────────────┼──────────────────────────┤
 │ EDIT   │ README.md                    │ Add Day1 summary section │
 └────────┴──────────────────────────────┴──────────────────────────┘
 ---
 Document Structure: dailyNote.md

 1. Goal & Initial Symptom (1 paragraph)

 Content:
 - Goal: Render 10,000 cubes in two modes (Instanced vs Naive) with runtime toggle
 - Initial symptom: "Exploding/giant triangles" - screen-filling wedges instead of cube grid
 - Success criteria: Visually identical output in both modes, no GPU lifetime bugs

 2. Timeline (Evidence-based, 10-12 bullets)
 ┌──────────────────────────────────────┬──────────────────┬────────────────────────────────────────┐
 │              Checkpoint              │      Commit      │              Observation               │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Initial 10k instancing scaffold      │ df195e4          │ Giant wedges, not cubes                │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Top-down orthographic camera         │ 34caf64          │ Still wrong geometry                   │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Diagnostic instrumentation           │ a74d067          │ Viewport/scissor verified OK           │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Microtest A: bypass SRV              │ d05787d          │ Instanced==Naive → SRV path is suspect │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Microtest B: raw SRV diagnostic      │ bf93d05          │ Color sentinel shows data readable     │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Per-frame state tracking fix         │ 4ed54f0          │ Fixes barrier #527 error               │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Restore StructuredBuffer + row_major │ 5cac813          │ Primary fix - matrices now correct     │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Camera presets                       │ 2661185, 6ecda94 │ Proper 3D perspective view             │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Scale cubes for gaps                 │ fa87515          │ Visual separation achieved             │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Fix cross product order              │ 32ab876          │ Outward-facing normals                 │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ SV_PrimitiveID for normals           │ 8122229          │ Reliable face normals                  │
 ├──────────────────────────────────────┼──────────────────┼────────────────────────────────────────┤
 │ Index buffer winding fix             │ d434845          │ All 6 faces visible                    │
 └──────────────────────────────────────┴──────────────────┴────────────────────────────────────────┘
 3. Root Cause(s) and Why It Happened

 Primary Root Cause: Matrix Layout Mismatch
 - CPU writes matrices row-major (translation at indices [12..14])
 - HLSL defaults to column-major without row_major annotation
 - Result: Matrix transposed → translation in wrong place → extreme vertex positions

 Binding Chain (DX12 terms):
 TransformBuffer (DEFAULT)
   → SRV descriptor (StructuredBuffer, stride=64)
   → Heap slot (per-frame slice)
   → GPU handle
   → SetGraphicsRootDescriptorTable(RP_TransformsTable, handle)
   → Shader reads t0: Transforms[SV_InstanceID]

 Secondary Issues:
 1. Per-frame barrier state tracking - global s_firstFrame didn't account for triple-buffering
 2. Cross product order - reversed, normals pointed inward
 3. ddx/ddy unreliable - GPU computes per 2x2 quad, averages across face boundaries

 Hypotheses Ruled Out:
 - SRV format/stride wrong → Microtest B showed data readable
 - Descriptor heap binding wrong → Microtest A isolated the issue
 - ViewProj wrong → Issue persisted after orthographic camera

 4. Fix Summary (Patch-level)
 ┌─────────────────────────────────────────────────────┬─────────┬──────────────────────────────────────────┬───────────────────────────────────────────────────────┐
 │                        File                         │ Commit  │                  Change                  │                     Why It Works                      │
 ├─────────────────────────────────────────────────────┼─────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────────┤
 │ ShaderLibrary.cpp                                   │ 5cac813 │ Add row_major to ViewProj and Transforms │ CPU row-major matches HLSL interpretation             │
 ├─────────────────────────────────────────────────────┼─────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────────┤
 │ Dx12Context.cpp, FrameContextRing.h                 │ 4ed54f0 │ Per-frame transformsState field          │ Each frame context tracks its own barrier state       │
 ├─────────────────────────────────────────────────────┼─────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────────┤
 │ ShaderLibrary.cpp                                   │ 8122229 │ Use SV_PrimitiveID for face index        │ faceIndex = (primID % 12) / 2 → deterministic normals │
 ├─────────────────────────────────────────────────────┼─────────┼──────────────────────────────────────────┼───────────────────────────────────────────────────────┤
 │ Dx12Context.cpp, RenderScene.cpp, ShaderLibrary.cpp │ d434845 │ Fix index buffer winding order           │ All 6 cube faces use CW winding from outside          │
 └─────────────────────────────────────────────────────┴─────────┴──────────────────────────────────────────┴───────────────────────────────────────────────────────┘
 5. Proof / Verification

 Debug Logs Confirming Fix:
 - Descriptor heap GPU base address logged at bind time
 - Per-frame SRV slot allocation verified (no stomp)
 - Barrier state transitions logged per-frame

 Microtest Results:
 - Microtest A (bypass SRV): Instanced==Naive → confirmed SRV path was the issue
 - Microtest B (ByteAddressBuffer): Color sentinel validated data readable
 - MICROTEST_MODE toggle preserved for future diagnostics

 Visual Verification:
 - 100x100 cube grid visible with proper 3D perspective
 - All 6 faces render with distinct lighting
 - Instanced and Naive modes produce identical output
 - Screenshots referenced in docs/contracts/day1/ (if present)

 6. Takeaways / Invariants

 Invariants (8 bullets):
 1. Matrix layout matching: CPU row-major requires row_major HLSL annotation
 2. Per-frame state tracking: Each FrameContext owns its resource states (not global)
 3. Fence-gated reuse: Wait for CompletedFence >= FrameContext.fenceValue before reuse
 4. Per-frame descriptor slices: Avoid descriptor stomp across in-flight frames
 5. GPU derivative limitations: ddx/ddy unreliable for small triangles; use SV_PrimitiveID
 6. Proof-driven debugging: Isolated microtests pinpoint root cause faster than guessing
 7. Index buffer winding: Consistent CW from outside for all faces
 8. MICROTEST_MODE toggle: Preserve diagnostic capability in production code

 Next Step: Day1.5 Refactor Targets
 - Upload arena suballocation (replace per-frame buffer copies)
 - Resource state tracker (explicit barrier management)
 - GPU timestamp query integration

 7. Link Index (Raw Evidence)

 Plans:
 - ../../contracts/day1/Day1_InstancingVsNaive.md - Original contract
 - ../../contracts/day1/Day1_InstancingVsNaive_PLAN.md - Implementation plan
 - ../../contracts/day1/Day1_Debug_plan.md - Initial debug plan
 - ../../contracts/day1/Day1_Debug_plan5-exp1.md - Microtest A plan

 Issue Packets:
 - ../../contracts/day1/Day1_Debug_IssuePacket_ExplodingTriangles.md - Primary symptom
 - ../../contracts/day1/Day1_Debug_GroundTruth_VisualSpec.md - Expected vs actual

 Runtime Logs:
 - ../../contracts/day1/Day1_Debug_RuntimeLog_RowMajorCompileFail.md - row_major syntax issue

 Experiments:
 - ../../contracts/day1/Day1_experiment1.md - Microtest results
 - ../../contracts/day1/Day1_Debug_explore.md - Codebase exploration

 ---
 README.md Update

 Add after "Day0.5 Contract" section:

 ## Day 1: Instanced vs Naive (10k Cubes)

 **Goal**: Render 10,000 cubes with runtime toggle between instanced (1 draw) and naive (10k draws) modes.

 **Outcome**:
 - Both modes produce identical visual output
 - 100x100 cube grid with proper 3D perspective and face lighting
 - Triple-buffered frame contexts with fence-gated resource reuse

 **Key Commits**:
 - `df195e4` - Initial 10k instancing scaffold
 - `5cac813` - Fix: `row_major` annotation for matrix layout match
 - `4ed54f0` - Fix: Per-frame barrier state tracking
 - `d434845` - Fix: Cube index buffer winding for all 6 faces

 **Key Learning**:
 - CPU row-major matrices require `row_major` HLSL annotation
 - Per-frame resource state tracking prevents triple-buffer barrier mismatches

 **Debug Narrative**: [docs/debug/day1/dailyNote.md](docs/debug/day1/dailyNote.md)

 **Evidence**: [docs/contracts/day1/](docs/contracts/day1/)

 **Next**: Day1.5 - Upload arena, resource state tracker

 ---
 Verification

 After implementation:
 - docs/debug/day1/dailyNote.md exists with all 7 sections
 - README.md has Day1 section with links
 - All commit hashes reference real commits (verified via git log)
 - All doc links resolve to existing files

 ---
 Missing Assets Note

 If not already committed:
 - Screenshots showing before/after cube rendering
 - Debug layer log captures showing #527 fix

 These should be added to docs/contracts/day1/ or captures/ if available.