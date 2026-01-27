You are working in my repo (DX12EngineLab). Context: I recently fixed a cube collision/render mismatch (cubeHalfXZ 0.45→0.9) and added iterative solver diagnostics (iterationsUsed, contacts(sum), maxPenetrationAbs, hitMaxIter) end-to-end in HUD. Builds pass.

if you need to understand contaxt, you can see git log or docs/contracts/day3.4/colison-debug.md

However, the gameplay symptoms remain:
- After jump / landing, feet visually sink into the floor (still happens).
- Standing/walking on cube tops feels “wedged” / cannot reliably traverse even though shift-move works.
- Jump input sometimes fails at certain positions; onGround likely flickers.

High-level diagnosis hypothesis (do NOT assume it is correct):
This is not primarily a “needs a brand new collision system” issue yet. It’s likely a mismatch in (1) pawn anchor convention (feet-at-posY vs center-at-posY) across physics/renderer/floor clamp, and/or (2) contact vs penetration definition (touch counted as intersection), causing solver thrash and unstable onGround.

Goal (this task): Implement the “minimal-fix route” only:
A) Anchor contract proof + correction
B) Contact definition correction (touch != penetration)
C) onGround stabilization via snap-to-support logic
NO swept/TOI rewrite in this task. If minimal-fix doesn’t solve the symptoms, we’ll do a separate task for a larger rewrite.

Non-negotiable constraints:
- Keep infra frozen: UploadArena, ResourceStateTracker, DescriptorRingAllocator untouched.
- Preserve fixed-step loop and current HUD pipeline patterns.
- Provide proof artifacts: logs/HUD fields that verify the contract (not just “seems fixed”).
- Minimal scope: change only collision/character movement code paths necessary for A/B/C.
- Ensure Debug and Release builds still pass; no new debug layer errors on happy path.
- Do not “paper over” by arbitrary offsets unless the anchor contract is explicitly decided and proven.

Expected outputs:
1) A short “Contract + Proof Plan” (bullets): what invariants must hold, and what evidence (log/HUD) will prove them.
2) A concrete patch (edits) implementing A/B/C, with file list and rationale per change.
3) A verification checklist (manual repro steps + expected HUD/log values) focusing on:
   - BuildPawnAABB anchor: pawnAabb.minY == posY AND pawnAabb.maxY == posY + pawnHeight (if feet-anchored), OR explicitly switch to center-anchored and adjust renderer accordingly.
   - Intersects/contact: strict overlap with epsilon; touch should not count as penetration. Show how eps is applied.
   - onGround stability: define when it becomes true, and how snap-to-floor / snap-to-cubeTop works.

Important: I want you to be autonomous. Don’t just follow a prescriptive solution. Instead:
- First, have 5 “senior reviewer” sub-agents independently inspect the current code and symptoms, each with a distinct focus:
  1) Physics/Collision senior: anchor conventions + AABB math + contact/penetration definitions.
  2) Gameplay/Movement senior: onGround contract, jump gating, friction/velocity zeroing side effects.
  3) Debugging/Instrumentation senior: what evidence to add (HUD/log) to prove the contract and localize mismatch fast.
  4) Architecture senior: minimal-change boundaries, avoid regressions, keep responsibilities clean.
  5) QA/Edge-case senior: corner cases (cube edges, corners, high speed shift, stepping onto cube), reproducibility.
- Each senior produces a short review note: suspected root cause(s), minimal interventions, risks, proof hooks.
- Then you (the main agent) synthesize a single plan and implement it.

Implementation guidance (do not treat as mandatory; use if it matches findings):
- If anchor mismatch: decide one canonical anchor (prefer feet-at-posY if that’s the current contract) and ensure BOTH physics AABB and rendering are consistent (e.g., if render mesh is center-anchored, renderY = posY + pawnHeight*0.5).
- Intersects should use strict overlap with epsilon (touch should be false), or separate “contact” vs “penetration” checks.
- onGround should be stabilized AFTER collision resolution using a support test (velY<=0 and near a support surface within snapEps), snapping posY and zeroing velY accordingly.
- Add HUD/log proof showing (posY, pawnAabb.minY/maxY, onGround, velY, supportSurfaceY, and whether we snapped this tick).

Deliverables must be crisp and evidence-driven. If you are uncertain about current anchor, instrument first and prove it before changing behavior.

Proceed now: run the 5 senior reviews, propose the plan, then implement the patch.







--------------------------------
────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── Ready to code?
                                                                                                                                           Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌ Day3.5 Collision Minimal Fix - Final Implementation Plan

 New Types

 // In WorldState.h
 enum class SupportSource : uint8_t { FLOOR = 0, CUBE = 1, NONE = 2 };

 struct SupportResult
 {
     SupportSource source = SupportSource::NONE;
     float supportY = -1000.0f;
     int32_t cubeId = -1;
     float gap = 0.0f;
     uint32_t candidateCount = 0;  // For gap anomaly log
 };

 CollisionStats Extensions

 SupportSource supportSource = SupportSource::NONE;
 float supportY = -1000.0f;
 int32_t supportCubeId = -1;
 bool snappedThisTick = false;
 float supportGap = 0.0f;

 WorldState Members

 bool m_justJumpedThisTick = false;
 SupportResult QuerySupport(float px, float py, float pz, float velY) const;

 Implementation

 1. Strict Intersects()

 bool Intersects(const AABB& a, const AABB& b) const
 {
     return (a.minX < b.maxX && a.maxX > b.minX &&
             a.minY < b.maxY && a.maxY > b.minY &&
             a.minZ < b.maxZ && a.maxZ > b.minZ);
 }

 2. QuerySupport (CORRECTED with safety adjustments)

 SupportResult QuerySupport(float px, float py, float pz, float velY) const
 {
     SupportResult result;
     const float SUPPORT_EPSILON = 0.05f;
     float pawnBottom = py;

     // Early-out when rising (still return result for HUD)
     if (velY > 0.0f) return result;

     // Build query AABB with Y expanded
     AABB queryAABB = BuildPawnAABB(px, py, pz);
     queryAABB.minY -= SUPPORT_EPSILON;
     queryAABB.maxY += SUPPORT_EPSILON;

     // 1. Check floor support
     bool inFloorBounds = (px >= m_config.floorMinX && px <= m_config.floorMaxX &&
                           pz >= m_config.floorMinZ && pz <= m_config.floorMaxZ);
     if (inFloorBounds && fabsf(pawnBottom - m_config.floorY) < SUPPORT_EPSILON &&
         pawnBottom >= m_config.floorY - SUPPORT_EPSILON)  // Safety: not below floor
     {
         result.source = SupportSource::FLOOR;
         result.supportY = m_config.floorY;
         result.cubeId = -1;
         result.gap = fabsf(pawnBottom - m_config.floorY);
     }

     // 2. Check cube support (pick highest)
     float pawnMinX = queryAABB.minX;
     float pawnMaxX = queryAABB.maxX;
     float pawnMinZ = queryAABB.minZ;
     float pawnMaxZ = queryAABB.maxZ;

     auto candidates = QuerySpatialHash(queryAABB);
     result.candidateCount = static_cast<uint32_t>(candidates.size());

     for (uint16_t cubeIdx : candidates)
     {
         AABB cube = GetCubeAABB(cubeIdx);

         // XZ footprint overlap (inclusive)
         bool xzOverlap = (pawnMinX <= cube.maxX && pawnMaxX >= cube.minX &&
                           pawnMinZ <= cube.maxZ && pawnMaxZ >= cube.minZ);
         if (!xzOverlap) continue;

         float cubeTop = cube.maxY;
         float dist = fabsf(pawnBottom - cubeTop);

         // Safety B: Only support from above (pawnBottom >= cubeTop - SUPPORT_EPSILON)
         if (pawnBottom < cubeTop - SUPPORT_EPSILON) continue;

         // Safety A: Select cube if better than current
         if (dist < SUPPORT_EPSILON && (result.source == SupportSource::NONE || cubeTop > result.supportY))
         {
             result.source = SupportSource::CUBE;
             result.supportY = cubeTop;
             result.cubeId = cubeIdx;
             result.gap = dist;
         }
     }

     return result;
 }

 3. TickFixed Support Application

 // After collision resolution:

 // Query support (ALWAYS for HUD)
 SupportResult support = QuerySupport(m_pawn.posX, m_pawn.posY, m_pawn.posZ, m_pawn.velY);

 // Copy to collision stats for HUD
 m_collisionStats.supportSource = support.source;
 m_collisionStats.supportY = support.supportY;
 m_collisionStats.supportCubeId = support.cubeId;
 m_collisionStats.supportGap = support.gap;
 m_collisionStats.snappedThisTick = false;

 // Safety C: Rising case - clear onGround
 if (!m_justJumpedThisTick && m_pawn.velY > 0.0f)
 {
     m_pawn.onGround = false;
 }
 // Falling or standing case
 else if (!m_justJumpedThisTick && m_pawn.velY <= 0.0f)
 {
     if (support.source != SupportSource::NONE)
     {
         // Apply snap
         if (m_pawn.posY != support.supportY)
         {
             m_pawn.posY = support.supportY;
             m_collisionStats.snappedThisTick = true;
         }
         m_pawn.velY = 0.0f;
         m_pawn.onGround = true;
     }
     else
     {
         m_pawn.onGround = false;
     }
 }
 // If justJumped: don't touch onGround (already set false in jump)

 // Gap anomaly detection
 if (support.source == SupportSource::NONE && fabsf(m_pawn.posY - 3.0f) < 0.02f)
 {
     bool inFloorBounds = (m_pawn.posX >= m_config.floorMinX && m_pawn.posX <= m_config.floorMaxX &&
                           m_pawn.posZ >= m_config.floorMinZ && m_pawn.posZ <= m_config.floorMaxZ);
     char buf[320];
     sprintf_s(buf, "[GAP_ANOMALY] px=%.2f pz=%.2f py=%.3f inFloor=%d gap=%.3f foot=[%.2f..%.2f] cand=%u\n",
         m_pawn.posX, m_pawn.posZ, m_pawn.posY, inFloorBounds ? 1 : 0, support.gap,
         m_pawn.posX - m_config.pawnHalfWidth, m_pawn.posX + m_config.pawnHalfWidth,
         support.candidateCount);
     OutputDebugStringA(buf);
 }

 m_justJumpedThisTick = false;  // Reset for next tick

 4. Jump Grace

 if (m_pawn.onGround && input.jump && !m_jumpConsumedThisFrame)
 {
     m_pawn.velY = m_config.jumpVelocity;
     m_pawn.onGround = false;
     m_jumpQueued = true;
     m_jumpConsumedThisFrame = true;
     m_justJumpedThisTick = true;
 }

 5. Remove Blanket Reset

 Delete line 130: m_pawn.onGround = false;

 6. Simplify ResolveFloorCollision

 Remove snap/onGround/velY logic. Keep logging only.

 7. Simplify ResolveAxis Y

 Remove onGround logic from Y-axis section.

 Files to Modify
 ┌──────────────────────────────┬───────────────────────────────────────────────────────────────────────────────────────┐
 │             File             │                                        Changes                                        │
 ├──────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/WorldState.h          │ SupportSource, SupportResult, CollisionStats, m_justJumpedThisTick, QuerySupport decl │
 ├──────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────┤
 │ Engine/WorldState.cpp        │ Strict Intersects, QuerySupport, TickFixed support pass, jump grace, BuildSnapshot    │
 ├──────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/Dx12Context.h  │ HUDSnapshot support fields                                                            │
 ├──────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ImGuiLayer.h   │ WorldStateFields support fields                                                       │
 ├──────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────┤
 │ Renderer/DX12/ImGuiLayer.cpp │ SetHUDSnapshot copy, BuildHUDContent display                                          │
 └──────────────────────────────┴───────────────────────────────────────────────────────────────────────────────────────┘
 Verification Checklist

 a) Floor idle 10s

 Support: FLOOR Y=0.000
 onGround: YES (no flicker)
 posY: 0.000

 b) Jump and land

 - Rising: Support=NONE, onGround=NO
 - Landing: Support=FLOOR, Snapped=YES, onGround=YES

 c) Cube top idle

 Support: CUBE(id) Y=3.000
 onGround: YES
 contacts: 0

 d) Walk on cube top

 - onGround=YES continuous

 e) Step off edge

 - Before: Support=CUBE
 - After: Support=NONE, onGround=NO

 f) Cube gap

 - No GAP_ANOMALY expected (footprint 0.8 > gap 0.2)

 SUPPORT_EPSILON

 Single constant: 0.05f