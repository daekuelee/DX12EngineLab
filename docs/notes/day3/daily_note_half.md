# Day 3.4 - 3.10 Daily Notes: Collision System Evolution

## Overview

This document summarizes the collision system development from Day 3.4 through Day 3.10, tracking the iterative debugging process, root causes discovered, and fixes applied.

---

## Day 3.4: Collision Debug & Iterative Solver

**Commits**: `8883d1b`, `f39e7ee`

**Problem**: Character gets wedged between cubes, feet sink into surfaces, movement inconsistent (sprint works but walk fails).

**Root Cause**:
1. X/Z collision bounds mismatch: `cubeHalfXZ = 0.45` but visual extent is 0.9 (local 1.0 x scale 0.9)
2. Single-pass collision only resolves deepest cube per axis, causing wedging

**Fixes**:
- `cubeHalfXZ`: 0.45 -> 0.9 (match rendered geometry)
- Iterative solver: up to 8 iterations with epsilon convergence (0.001)
- Added diagnostics: `iterationsUsed`, `contacts`, `maxPenetrationAbs`, `hitMaxIter`

**HUD Fields Added**:
- Solver iteration count (1-8)
- Collision contacts (summed, not deduplicated)
- Max penetration absolute value
- HIT_MAX_ITER warning (red) when solver doesn't converge

---

## Day 3.5: Support Query & onGround Stabilization

**Commits**: `3dbd0f6`

**Problem**: After Day 3.4 fixes, feet still visually sink into floor, onGround flickers, jump input sometimes fails.

**Root Cause**:
1. Anchor convention mismatch (feet-at-posY vs center-at-posY)
2. Contact vs penetration definition (touch counted as intersection)
3. Multiple mutation points for onGround

**Fixes**:
- `QuerySupport()`: Pure function returning `SupportResult` (FLOOR/CUBE/NONE)
- Single mutation point: TickFixed applies snap/velY/onGround
- Strict Intersects: `<` and `>` (touching is NOT intersection)
- Snap-to-support logic with 0.05f epsilon

**New Types**:
```cpp
enum class SupportSource : uint8_t { FLOOR = 0, CUBE = 1, NONE = 2 };
struct SupportResult { source, supportY, cubeId, gap, candidateCount };
```

---

## Day 3.6: Multi-Issue Fix (Shading, Embedding, Fall-through)

**Commits**: `ec7339a`

**Problems**:
1. One cube face shading broken (-Y face dark)
2. Character embedded in surfaces (legs below feet level)
3. Floor fall-through on edge step-off

**Root Causes**:
1. Normal derivation priority X > Y > Z at corners
2. Character leg `offsetY=0.75` with `scaleY=1.5` puts bottom at Y=-0.75
3. No recovery if pawn overshoots below floor by > epsilon

**Fixes**:
1. Shader normal priority: Y > X > Z (cube_vs.hlsl)
2. Leg offsetY: 0.75 -> 1.5 (CharacterRenderer.cpp)
3. Floor penetration recovery block in TickFixed

---

## Day 3.7: Face Culling, A/D Movement, Axis-Aware Collision

**Commits**: `cf63029`, `397ff26`, `5f60a6e`

**Problems**:
1. Only 4 cube faces visible (missing +Z yellow, -Y cyan)
2. A/D movement mirrored
3. Side collision visually embeds (arms sink into cube sides)

**Root Causes**:
1. +Z and -Y face indices have CCW winding (other 4 faces CW)
2. `cross(up, fwd)` yields LEFT in right-handed coords
3. `pawnHalfWidth` doesn't cover arm visual extent

**Fixes**:
1. Fix +Z/-Y face index winding in RenderScene.cpp
2. Change to `cross(fwd, up)` = `(-fwdZ, 0, fwdX)` for correct right vector
3. Axis-aware collision: `pawnHalfExtentX=1.4f`, `pawnHalfExtentZ=0.4f`

**Winding Fix** (RenderScene.cpp:111-120):
```cpp
// +Z face: 4,6,5 -> 4,5,6 and 4,7,6 -> 4,6,7
// -Y face: 0,4,7 -> 0,7,4 and 0,7,3 -> 0,3,7
```

---

## Day 3.8: Face-Dependent Penetration Fix

**Commits**: `5846944`, `78a69b1`

**Problems**:
1. Collision asymmetric: some faces block, others allow penetration
2. A/D still mirrored after Day 3.7 changes

**Root Causes**:
1. Fixed X->Z->Y resolution order ignores which axis has minimum penetration
2. Right vector calculation in movement code

**Fixes**:
1. MTV-based XZ resolution: resolve along axis with smaller penetration
2. Camera-relative movement fix: `camRightX = -camFwdZ`, `camRightZ = camFwdX`

**New Function**: `ResolveXZ_MTV()` - computes penX/penZ, resolves along smaller axis

**HUD Fields Added**:
- `penX`, `penZ` (before resolution)
- `mtvAxis` (0=X, 2=Z)
- `mtvMagnitude`
- `centerDiffX`, `centerDiffZ`

---

## Day 3.9: Wall-Climb Regression Fix

**Commits**: `1ae3fcc`

**Problems**:
1. Collision still asymmetric after MTV change
2. New regression: pushing against wall causes pawn to climb up
3. Pawn not flush against cube surface (residual penetration)

**Root Causes**:
1. MTV only resolved one axis per iteration, leaving residual overlap
2. Residual XZ overlap triggers Y resolution to push upward

**Fixes**:
1. Separable-axis XZ push-out: apply BOTH penX and penZ
2. Anti-step-up guard: only allow upward Y if `wasAboveTop && fallingOrLanding`

**Key Logic** (ResolveAxis for Y):
```cpp
bool wouldPushUp = (deltaY > 0.0f);
bool wasAboveTop = (prevPawnBottom >= cubeTop - 0.01f);
bool fallingOrLanding = (m_pawn.velY <= 0.0f);
bool isLandingFromAbove = wasAboveTop && fallingOrLanding;

if (wouldPushUp && !isLandingFromAbove) {
    m_collisionStats.yStepUpSkipped = true;
    continue;  // Skip this cube for Y
}
```

**HUD Fields Added**:
- `xzStillOverlapping` (proof of separation)
- `yStepUpSkipped`
- `yDeltaApplied`

---

## Day 3.10: Penetration Sign Inversion Fix

**Commits**: `b69e7ed`

**Problem**: Collision still asymmetric after all previous fixes.

**Root Cause**: `ComputeSignedPenetration` returns wrong sign direction.

**The Bug** (WorldState.cpp:437):
```cpp
// WRONG: Returns positive when pawn is LEFT of cube
float sign = (centerPawn < centerCube) ? 1.0f : -1.0f;
```

**Proof by Example**:
- Pawn center X=9.5, Cube center X=10
- `centerPawn(9.5) < centerCube(10)` -> sign = +1.0
- pen = +1.5
- `newX = 9.5 + 1.5 = 11.0` -> pushes pawn RIGHT (through cube!)

**The Fix**:
```cpp
// CORRECT: Negative pushes left, positive pushes right
float sign = (centerPawn < centerCube) ? -1.0f : 1.0f;
```

**Result**: Single-line fix corrects all axes (X, Y, Z) simultaneously.

---

## Summary Table

| Day | Issue | Root Cause | Fix |
|-----|-------|------------|-----|
| 3.4 | Wedging, sinking | Collision bounds 0.45 vs 0.9 | cubeHalfXZ=0.9, iterative solver |
| 3.5 | onGround flicker | Multiple mutation points | QuerySupport pure, single mutation |
| 3.6 | Shading, embedding, fall-through | Normal priority, leg offset, no recovery | Y>X>Z, offsetY=1.5, recovery block |
| 3.7 | 4 faces only, A/D mirrored | Winding mismatch, cross product | Fix indices, cross(fwd,up) |
| 3.8 | Face-dependent penetration | Fixed axis order | MTV-based XZ resolution |
| 3.9 | Wall-climb regression | Single-axis MTV, no step-up guard | Both axes + anti-step-up |
| 3.10 | Asymmetric collision | Inverted penetration sign | Sign fix in ComputeSignedPenetration |

---

## Key Contracts Established

1. **Anchor Convention**: `posY` = feet bottom (not center)
2. **Intersects**: Strict overlap (`<` and `>`), touching is NOT intersection
3. **QuerySupport**: Pure function, no mutation
4. **TickFixed**: Single mutation point for snap/velY/onGround
5. **ComputeSignedPenetration**: Returns delta to ADD to position (`pos += pen` pushes away)
6. **Anti-Step-Up**: Only allow upward Y correction if truly landing from above

---

## Files Modified (Day 3.4-3.10)

| File | Changes |
|------|---------|
| Engine/WorldState.h | CollisionStats fields, ResolveAxis signature, SupportResult |
| Engine/WorldState.cpp | Iterative solver, QuerySupport, ResolveXZ_MTV, anti-step-up, sign fix |
| Renderer/DX12/Dx12Context.h | HUDSnapshot fields |
| Renderer/DX12/ImGuiLayer.h | WorldStateFields |
| Renderer/DX12/ImGuiLayer.cpp | HUD display sections |
| Renderer/DX12/RenderScene.cpp | Face index winding |
| Renderer/DX12/CharacterRenderer.cpp | Leg offsetY |
| shaders/cube_vs.hlsl | Normal priority |
