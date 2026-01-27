-------------bug

Bug A — Movement input A/D is mirrored

Symptom

Pressing A moves the character right.

Pressing D moves the character left.

Expected

A → left, D → right (in the same screen/camera frame as before).

Notes / Suspicions

Likely a sign error or handedness mismatch in:

input mapping (key → axis),

local-to-world transform for movement basis (right vector sign),

camera yaw basis (right = cross(up, forward) vs cross(forward, up)),

or view matrix convention flip.

Stop condition

With camera facing +Z, A decreases X and D increases X (or consistent with your defined world axes).

Bug B — Cube shading shows only 4 faces (missing two faces)

Symptom

In debug visualization, cube shading appears on only 4 faces (green/orange/red/blue), as if 2 faces are unlit/incorrect.

Expected

All 6 faces should shade correctly.

Notes / Suspicions

Could be:

Normal generation still wrong (dominant-axis derivation fails due to shared vertices / tie cases).

Backface culling / winding: two faces might be culled.

Normal transform wrong (world matrix inverse-transpose not used when non-uniform scaling exists).

Lighting uses dot(N,L) but N might be zero/NaN on two faces.

Stop condition

In Lambert mode, all 6 faces show expected lighting variation when rotating camera/light.

Bug C — Side collision still visually embeds (arms sink into cube sides)

Symptom

On XZ side collision, the character can partially enter the cube (arms/side sinks in).

Top collision seems fixed.

Expected

The character should stop flush against the cube side when the collision shape contacts it.

Notes / Suspicions

Could be one (or both):

Physics shape vs Render shape mismatch: collision uses pawnHalfWidth that doesn’t cover arms width, so visually it “clips” even if physics is correct.

Resolver penetration / axis order: X/Z resolution leaves residual penetration because solver uses center-based AABB but visuals extend further.

Foot-based posY vs render pivot mismatch in XZ too (less likely, but check).

Stop condition

Either:

(Option 1) physics capsule/AABB covers full rendered silhouette, so no visual embed, or

(Option 2) accept mismatch but HUD/visual clearly indicate collision proxy.


-----------------

ROLE
You are a senior engine/physics+render debugging assistant. Your goal is to fix three remaining issues with minimal, evidence-driven changes.
Use sub-agents if helpful, but do not ask me for vague clarifications—propose small verification steps and proceed.

PROJECT CONTEXT (brief)
DX12EngineLab. Fixed-step TickFixed drives physics.
Day3.5 introduced a strict collision rule and support query:
- QuerySupport is PURE (no mutation) and returns a SupportResult.
- TickFixed is the SINGLE mutation point for snap/velY/onGround.
- Debug layer must remain 0 errors in happy path.
Frozen infra: UploadArena / ResourceStateTracker / DescriptorRingAllocator must NOT be touched.

NEW BUGS TO FIX (3)
A) Movement input A/D is mirrored:
- Press A -> character moves right.
- Press D -> character moves left.
Expected: A left, D right (consistent with our world/camera basis).

B) Cube shading shows only 4 faces shaded in debug view:
- Only 4 faces show (green/orange/red/blue). 2 faces appear missing/unshaded.
Expected: all 6 cube faces shade correctly.

C) Side collision still visually embeds:
- On XZ side collision, character arms/side visually sink into cube.
- Top collision looks fixed now.
Expected: either (1) collision proxy matches visible silhouette, or (2) proxy/visual mismatch is explicitly debug-visualized and explained.

NON-NEGOTIABLES
1) Keep QuerySupport pure (no mutation).
2) Keep TickFixed as the single mutation point for snap/velY/onGround.
3) Keep strict Intersects semantics (touching is NOT intersection).
4) Keep debug layer 0 errors in happy path.
5) Do NOT touch frozen infra: UploadArena/ResourceStateTracker/DescriptorRingAllocator.

WHAT I NEED AS OUTPUT
Deliver in this exact structure:

1) Root-cause hypothesis per bug (A/B/C), ranked by likelihood.
2) A minimal verification plan (micro-tests) for each bug:
   - each test must produce a concrete proof artifact (HUD field, log line, or screenshot).
   - avoid rabbit holes: 1-2 tests per bug max.
3) The minimal code changes to fix each bug:
   - Provide patch-style edits (file+function+diff-like snippet).
   - Keep changes small and localized.
4) Post-fix stop-conditions:
   - A/D direction correct.
   - Cube shading: all 6 faces visible and normal debug view sane.
   - Side collision: either no visual embed OR proxy-vs-visual debug evidence + clear contract.
   - Debug layer still 0 errors.

SUB-AGENT SUGGESTION (optional)
- Agent 1: movement basis/sign investigation (input mapping + camera right vector).
- Agent 2: cube shading investigation (normals vs culling vs winding; propose a definitive test).
- Agent 3: collision proxy vs render silhouette mismatch (AABB extents + debug draw).

IMPORTANT GUIDANCE
- For Bug B, first disambiguate: are the 2 faces missing due to culling/winding, or are they drawn but unlit due to normals?
  Propose a single definitive test (e.g., NoCull toggle OR normal-as-color output).
- For Bug C, prioritize proving whether this is a proxy mismatch vs solver penetration.
  If it’s proxy mismatch, prefer adding debug draw + contract over inflating physics shape blindly.

Proceed with the investigation + patches.
---------------------------
Good catch on the consistency issue. Let me fix the plan and then implement.

● Updated plan
  ⎿  /plan to preview

● Plan updated with:
  - Bug A: Consistent contract "pawnXZ - eyeXZ" + orthogonality proof (dot ≈ 0)
  - Bug B: Screenshot instructions for both cubes (6 faces) AND character (no missing parts)                                                             - Bug C: Bounds proof + formula
                                                                                                                                                       ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                                        Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day3.7 Bug Fixes: Movement, Shading, Axis-Aware Collision

 Contract: Proof-First, Minimal Diffs, 1 Commit Per Bug

 ---
 Bug B: Only 4 Cube Faces Visible (FIRST)

 PSO Scope

 The cube PSO (m_pso) is shared by grid cubes AND character parts. Both use same mesh winding.

 Diagnosis

 Temporarily set CullMode = D3D12_CULL_MODE_NONE (line 226), build, run.
 - If 6 faces appear → winding mismatch confirmed
 - If still 4 faces → investigate draw params

 Fix (1 line)

 File: Renderer/DX12/ShaderLibrary.cpp line 227
 rasterizer.FrontCounterClockwise = TRUE;  // Was FALSE
 Then restore CullMode = D3D12_CULL_MODE_BACK.

 Proof (Screenshot Instructions)

 1. Cubes: All 6 faces visible with distinct debug colors
 2. Character: All 6 body parts visible (head, torso, 2 arms, 2 legs), no missing faces
 3. Debug layer: 0 errors

 Commit

 fix(gfx): Day3.7 - Fix cube face culling (FrontCounterClockwise=TRUE)

 ---
 Bug A: A/D Movement Mirrored (SECOND)

 Contract

 Camera-relative movement. Forward = pawnXZ - eyeXZ (normalized).

 Fix

 File: Engine/WorldState.cpp lines 82-103

 // Camera-relative movement: forward = pawnXZ - eyeXZ
 float camFwdX = m_pawn.posX - m_camera.eyeX;
 float camFwdZ = m_pawn.posZ - m_camera.eyeZ;
 float fwdLen = sqrtf(camFwdX * camFwdX + camFwdZ * camFwdZ);

 if (fwdLen < 0.001f) {
     // Fallback to pawn yaw if camera too close
     camFwdX = sinf(m_pawn.yaw);
     camFwdZ = cosf(m_pawn.yaw);
 } else {
     camFwdX /= fwdLen;
     camFwdZ /= fwdLen;
 }

 // Right = cross(up, fwd) = (fwdZ, 0, -fwdX)
 float camRightX = camFwdZ;
 float camRightZ = -camFwdX;

 // Store for HUD + orthogonality proof
 m_camera.dbgFwdX = camFwdX;
 m_camera.dbgFwdZ = camFwdZ;
 m_camera.dbgRightX = camRightX;
 m_camera.dbgRightZ = camRightZ;
 m_camera.dbgDot = camFwdX * camRightX + camFwdZ * camRightZ;  // Should be ~0

 Velocity (lines 102-103):
 m_pawn.velX = (camFwdX * input.moveZ + camRightX * input.moveX) * currentSpeed;
 m_pawn.velZ = (camFwdZ * input.moveZ + camRightZ * input.moveX) * currentSpeed;

 HUD Proof

 - Cam: fwd=(X,Z) right=(X,Z) dot=D
 - Expect: dot ≈ 0 (orthogonality proof)

 Files to Add HUD Fields

 - WorldState.h: CameraState add dbgFwdX/Z, dbgRightX/Z, dbgDot
 - Dx12Context.h: HUDSnapshot add camFwdX/Z, camRightX/Z, camDot
 - ImGuiLayer.h/cpp: Copy and display

 Stop-Condition

 - A = screen-left, D = screen-right
 - HUD dot ≈ 0
 - If still mirrored: flip input.moveX sign at mapping layer

 Commit

 fix(gfx): Day3.7 - Camera-relative A/D movement (pawnXZ - eyeXZ)

 ---
 Bug C: Side Embedding (THIRD)

 Bounds Proof

 Mesh vertices ±1.0 (RenderScene.cpp). Extent formula: offset ± scale.
 RightArm: 1.0 + 0.4 = 1.4

 Fix

 File: Engine/WorldState.h line 139
 float pawnHalfExtentX = 1.4f;  // Arms outer edge
 float pawnHalfExtentZ = 0.4f;  // Keep tight

 File: Engine/WorldState.cpp BuildPawnAABB (lines 354-359)
 aabb.minX = px - m_config.pawnHalfExtentX;
 aabb.maxX = px + m_config.pawnHalfExtentX;
 aabb.minZ = pz - m_config.pawnHalfExtentZ;
 aabb.maxZ = pz + m_config.pawnHalfExtentZ;

 HUD Proof

 - Extent: X=1.40 Z=0.40

 Files to Add HUD Fields

 - Dx12Context.h: HUDSnapshot add pawnExtentX/Z
 - ImGuiLayer.h/cpp: Copy and display

 Stop-Condition

 - Side ±X: No arm embedding
 - Side ±Z: Stops tight
 - HUD: X=1.40 Z=0.40

 Commit

 fix(gfx): Day3.7 - Axis-aware collision (X=1.4, Z=0.4)

 ---
 Build

 msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64