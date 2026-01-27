ROLE
You are a senior engine programmer reviewing and fixing a minimal-scope collision + rendering alignment regression in DX12EngineLab.

CONTEXT
We recently introduced a support-query-based onGround system:
- Intersects() is STRICT (< and >): touching is NOT intersection.
- QuerySupport() returns a pure SupportResult (no mutation).
- TickFixed is the single mutation point for snap/velY/onGround.
- Floor collision clamp logic was moved out (ResolveFloorCollision now logging-only).
- HUD proof chain was wired (WorldState → Snapshot → ImGui).

CURRENT ISSUES (observed in video)
1) One cube face shading is broken (lighting looks wrong on exactly one face).
2) Character appears slightly embedded into surfaces:
   - On cube tops and cube side collisions, it visually penetrates instead of stopping flush.
   - Character starts at posY=3.0 (feet), but looks sunk and can move while “inside”.
3) Stepping off cube edges sometimes results in falling through the floor (within floor bounds).
4) If I jump first (higher Y) and then land/move, floor fall-through happens less often.

HYPOTHESES
- Visual penetration may be physics/render origin mismatch: physics uses posY=feet but rendering uses posY=center; or mesh extents don’t match pawnHalfWidth/pawnHeight.
- Floor fall-through likely because floor support reacquisition requires |posY-floorY| < eps, so if we overshoot below floor by > eps in one tick, we never snap back.

NON-NEGOTIABLES
- Keep Intersects() strict (< and >).
- Keep QuerySupport pure (no mutation) and keep TickFixed as the single mutation point for applying snap/velY/onGround.
- Frozen infra untouched: UploadArena / ResourceStateTracker / DescriptorRingAllocator.
- Maintain debug layer 0 errors in happy path.
- Provide proof artifacts: HUD fields + targeted debug logs for any anomaly.

REQUEST
Use subagents if helpful:
A) Physics/Collision agent:
   - Diagnose floor fall-through. Ensure floor can recover from overshoot/penetration while staying within the single-mutation-point rule.
   - Propose minimal patch: floor support should clamp even if pawnBottom is below floor (not only within epsilon).
   - Add one anomaly log when inFloorBounds && pawnBottom < floorY - 0.1 (or similar), capturing pos, velY, dt.

B) Render/Transform agent:
   - Verify render vs physics anchor: physics contract is posY=feet (bottom).
   - Check how the pawn is rendered. If rendering uses posY as center, fix by rendering at (posY + pawnHeight*0.5).
   - Also verify mesh half extents vs pawnHalfWidth. If mismatch, either adjust render scale or adjust collision dims (minimal choice preferred).
   - Add HUD debug lines: pawnBottom, pawnTop, renderYUsed, pawnHalfWidth.

C) Shading agent:
   - Diagnose the single broken cube face shading.
   - Add a debug mode: output normals as color to confirm if that face normal is flipped.
   - If cube uses shared vertices, consider per-face normals (flat shading) or correct indices/winding for that face.

DELIVERABLES
1) Minimal code changes implementing the fixes.
2) A short “why it works” explanation.
3) A verification checklist I can run immediately (must include the 6 stop-conditions below).
4) If a hypothesis is unconfirmed, add a small proof hook (HUD/log) instead of guessing.

STOP-CONDITION TESTS (must pass)
- Floor idle 10s: Support=FLOOR, posY=0.000, onGround=YES never flickers.
- Cube top idle 10s: Support=CUBE, posY=3.000, onGround=YES; contacts not exploding due to touch.
- Jump spam 30 times: no “jump eaten”, especially right after landing.
- Step off cube edge: Support becomes NONE and pawn falls naturally, then lands on floor (if in bounds).
- GAP_ANOMALY: must never trigger while posY≈3.0 on cube tops; if triggers, log line must be preserved.
- Debug layer: 0 errors in happy path.

Proceed: read relevant files, implement minimal patch, build Debug+Release, summarize changes.





----------------------
 Let me first read the existing plan file, then launch exploration agents to investigate the issues.

● Read 1 file (ctrl+o to expand)

● The existing plan was for Day3.5 (just implemented). This is a new request addressing remaining issues. Let me launch exploration agents   to investigate the three problem areas.
                                                                                                                                          ● 3 Explore agents finished (ctrl+o to expand)
   ├─ Explore physics/collision code · 19 tool uses · 39.9k tokens
   │  ⎿  Done
   ├─ Explore render vs physics anchor · 29 tool uses · 49.4k tokens
   │  ⎿  Done
   └─ Explore cube shading/normals · 14 tool uses · 43.2k tokens
      ⎿  Done

● Excellent! All three agents have returned comprehensive findings. Let me read a few critical files to verify the findings, then write
  the final plan.
                                                                                                                                          ● Read 3 files (ctrl+o to expand)

● I've verified all three findings. Now let me write the final implementation plan.

● Updated plan
  ⎿  /plan to preview

● I realize Fix 2 needs more clarification. The cube tops (Y=+3) actually align between render and collision - the mismatch is at the
  BOTTOM (render -3 vs collision 0). Let me ask about the embedding issue.
                                                                                                                                          ● User answered Claude's questions:                                                                                                         ⎿  · For the character embedding issue: does the character appear embedded when standing on CUBE TOPS (feet should be at Y=3), or
     when colliding with cube SIDES (XZ collision)? → Both                                                                                
● Thank you. I now understand the character embedding better - let me update the plan with the character part offset fix and verify the
  cube analysis.
                                                                                                                                          ● Read 1 file (ctrl+o to expand)

● I've confirmed the character embedding issue. The leg parts have offsetY=0.75 with scaleY=1.5, which puts the leg bottom at Y=-0.75
  (0.75 units below the feet). Let me update the plan with this fix.
                                                                                                                                          ● Updated plan
  ⎿  /plan to preview

● The plan is complete with all three fixes:

  1. Cube shading: Fix vertex shader normal priority (Y > X > Z)                                                                            2. Character embedding: Fix leg part offsetY (0.75 → 1.5)
  3. Floor fall-through: Add penetration recovery in TickFixed                                                                            
────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                           Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day3.6 Multi-Issue Fix Plan

 Issues Summary
 ┌─────┬─────────────────────────────────────────┬────────────────────────────────────────────────────────┬─────────────────────────────┐
 │  #  │                  Issue                  │                       Root Cause                       │        Fix Location         │
 ├─────┼─────────────────────────────────────────┼────────────────────────────────────────────────────────┼─────────────────────────────┤
 │ 1   │ Cube face shading broken (-Y face dark) │ Normal derivation priority X>Y>Z at corners            │ shaders/cube_vs.hlsl:44-49  │
 ├─────┼─────────────────────────────────────────┼────────────────────────────────────────────────────────┼─────────────────────────────┤
 │ 2   │ Character embedded in surfaces          │ Character leg offsetY=0.75 puts bottom at Y=-0.75      │ CharacterRenderer.cpp:24-26 │
 ├─────┼─────────────────────────────────────────┼────────────────────────────────────────────────────────┼─────────────────────────────┤
 │ 3   │ Floor fall-through on edge step-off     │ No recovery if pawn overshoots below floor by >epsilon │ WorldState.cpp TickFixed    │
 └─────┴─────────────────────────────────────────┴────────────────────────────────────────────────────────┴─────────────────────────────┘
 ---
 Fix 1: Cube Face Shading (Normal Priority)

 File: shaders/cube_vs.hlsl

 Problem: At cube corners where |x| == |y| == |z|, the first condition (X) wins, causing -Y face vertices to derive X-axis normals
 instead of Y-axis normals.

 Current (lines 44-49):
 if (absPos.x >= absPos.y && absPos.x >= absPos.z)
     normal = float3(sign(vin.Pos.x), 0, 0);
 else if (absPos.y >= absPos.x && absPos.y >= absPos.z)
     normal = float3(0, sign(vin.Pos.y), 0);
 else
     normal = float3(0, 0, sign(vin.Pos.z));

 Fix: Change priority to Y > X > Z (ensures top/bottom faces get correct normals):
 // Derive normal from dominant axis (Y > X > Z priority for floor/ceiling faces)
 if (absPos.y >= absPos.x && absPos.y >= absPos.z)
     normal = float3(0, sign(vin.Pos.y), 0);
 else if (absPos.x >= absPos.z)
     normal = float3(sign(vin.Pos.x), 0, 0);
 else
     normal = float3(0, 0, sign(vin.Pos.z));

 ---
 Fix 2: Character Part Y Offsets

 File: Renderer/DX12/CharacterRenderer.cpp

 Problem: Character parts use a unit cube mesh (vertices at ±1). With scaleY=1.5 and offsetY=0.75, the legs span [-0.75, +2.25] relative
 to posY. This puts the leg bottom 0.75 units BELOW the feet, causing visual embedding.

 Formula: To place part bottom at posY (feet level), offsetY must equal scaleY.

 Current (lines 24-26):
 // LeftLeg
 {-0.4f, 0.75f, 0.0f,  0.5f, 1.5f, 0.5f },
 // RightLeg
 { 0.4f, 0.75f, 0.0f,  0.5f, 1.5f, 0.5f },

 Fix: Adjust leg offsetY to equal scaleY:
 // LeftLeg - offsetY = scaleY so bottom at feet
 {-0.4f, 1.5f, 0.0f,  0.5f, 1.5f, 0.5f },
 // RightLeg
 { 0.4f, 1.5f, 0.0f,  0.5f, 1.5f, 0.5f },

 Result: Legs now span [0, 3.0] relative to posY (feet at Y=0, knees at Y=3.0).

 Note: This changes leg visual height. If proportions look wrong, alternative is to reduce scaleY=0.75 with offsetY=0.75 → legs span [0,
 1.5].

 ---
 Fix 3: Floor Penetration Recovery

 File: Engine/WorldState.cpp

 Problem: QuerySupport() only grants floor support if |pawnBottom - floorY| < 0.05. If pawn overshoots below floor by >0.05, no recovery
 occurs and pawn falls forever.

 Fix Location: In TickFixed(), after QuerySupport() call (around line 185), before support application.

 Add floor recovery block:
 // Day3.6: Floor penetration recovery (handle overshoots beyond epsilon)
 if (support.source == SupportSource::NONE && m_pawn.velY <= 0.0f)
 {
     bool inFloorBounds = (m_pawn.posX >= m_config.floorMinX && m_pawn.posX <= m_config.floorMaxX &&
                           m_pawn.posZ >= m_config.floorMinZ && m_pawn.posZ <= m_config.floorMaxZ);
     // If pawn is below floor and in bounds, force floor recovery
     if (inFloorBounds && m_pawn.posY < m_config.floorY)
     {
         float overshoot = m_config.floorY - m_pawn.posY;
         // Log anomaly for diagnostics
         char buf[256];
         sprintf_s(buf, "[FLOOR_RECOVERY] posY=%.3f overshoot=%.3f velY=%.2f\n",
             m_pawn.posY, overshoot, m_pawn.velY);
         OutputDebugStringA(buf);

         // Force floor support
         support.source = SupportSource::FLOOR;
         support.supportY = m_config.floorY;
         support.cubeId = -1;
         support.gap = overshoot;
     }
 }

 ---
 Files to Modify
 ┌─────────────────────────────────────┬──────────────────────────────────────────────┐
 │                File                 │                   Changes                    │
 ├─────────────────────────────────────┼──────────────────────────────────────────────┤
 │ shaders/cube_vs.hlsl                │ Fix normal priority: Y > X > Z (lines 44-49) │
 ├─────────────────────────────────────┼──────────────────────────────────────────────┤
 │ Renderer/DX12/CharacterRenderer.cpp │ Fix leg offsetY: 0.75 → 1.5 (lines 24-26)    │
 ├─────────────────────────────────────┼──────────────────────────────────────────────┤
 │ Engine/WorldState.cpp               │ Add floor penetration recovery in TickFixed  │
 └─────────────────────────────────────┴──────────────────────────────────────────────┘
 ---
 Verification Checklist

 Stop-Condition Tests (must pass)

 1. Floor idle 10s: Support=FLOOR, posY=0.000, onGround=YES never flickers
 2. Cube top idle 10s: Support=CUBE, posY=3.000, onGround=YES; contacts=0
 3. Jump spam 30 times: No "jump eaten", especially right after landing
 4. Step off cube edge: Support becomes NONE, pawn falls, lands on floor (if in bounds)
 5. GAP_ANOMALY: Must never trigger at posY≈3.0 on cube tops
 6. Debug layer: 0 errors in happy path

 Visual Verification

 - All 6 cube faces have correct shading (bottom face not dark in Lambert mode)
 - Character feet align flush with floor (no sinking below Y=0)
 - Character feet align flush with cube tops (no sinking below Y=3)
 - Walking off cube edge results in smooth fall to floor
 - No [FLOOR_RECOVERY] logs during normal floor landing (only on edge case overshoots)

 HUD Proof Fields

 - Support: FLOOR/CUBE(id)/NONE
 - supportY: 0.000 or 3.000
 - onGround: YES/NO
 - Snapped: YES/NO
 - Gap: 0.0000

 ---
 Build & Test Commands

 # Build Debug
 msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64

 # Build Release
 msbuild DX12EngineLab.sln /m /p:Configuration=Release /p:Platform=x64

 Both must succeed with 0 errors.

 ---
 Why It Works

 1. Normal Priority Fix: The -Y face (bottom) of cubes shares corner vertices with ±X and ±Z faces. By prioritizing Y-axis in the normal
 derivation logic, we ensure that vertices at y=-1 (bottom corners) get the correct (0,-1,0) normal instead of an X or Z axis normal.
 This fixes Lambert shading for floor-facing cube faces.
 2. Character Offset Fix: The character mesh is a unit cube centered at origin. Scaling by scaleY expands it symmetrically around 0.
 Translating by offsetY = scaleY shifts the bottom edge to exactly 0 (the foot level), eliminating visual embedding.
 3. Floor Recovery Fix: The current support detection uses a strict epsilon window (0.05 units). If the pawn falls fast enough to skip
 past this window in one tick, it never gets floor support. The recovery check explicitly handles this case by detecting when the pawn is
  BELOW the floor (not just near it) and forcing support back on.