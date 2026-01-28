# Day 3.11 Daily Notes: Capsule Controller Development

## Overview

This document tracks the development of an alternative controller mode using a vertical capsule with sweep/slide collision, implemented as a toggle alongside the existing AABB controller.

**Phased Implementation**:
- P0: Toggle + Reset infrastructure
- P1: Capsule geometry SSOT + HUD proof
- P2: Depenetration safety net
- P3: XZ Sweep/Slide MVP

**SSOT Reference**: [docs/notes/sweep_capsule.md](../../notes/sweep_capsule.md)

---

## Phase 0: Toggle + Reset

**Commit**: `1665692`

**Goal**: Runtime switching between AABB and Capsule modes with unified reset logic.

**Changes**:
- `ControllerMode` enum: `{ AABB, Capsule }`
- F6 key toggles mode at runtime
- `RespawnResetControllerState()` unified reset (works for both modes)
- Mode persists across respawn (only physics state resets)

**Key Code**:
```cpp
enum class ControllerMode : uint8_t { AABB = 0, Capsule = 1 };
```

---

## Phase 1: Capsule SSOT + HUD Proof

**Commit**: `fdf832d`

**Goal**: Define canonical capsule geometry with feet-bottom anchor and prove correctness via HUD.

**Capsule Parameters**:
| Parameter | Value | Description |
|-----------|-------|-------------|
| `capsuleRadius` | 0.8 | Hemisphere/cylinder radius |
| `capsuleHalfHeight` | 1.7 | Half cylinder height |
| `totalHeight` | 5.0 | `2*r + 2*hh` |

**Anchor Convention**:
- `posY` = feet-bottom (lowest point of bottom hemisphere)
- Same convention as AABB mode

**Function**: `MakeCapsuleFromFeet()`
```cpp
Capsule MakeCapsuleFromFeet(float3 feetPos, float radius, float halfHeight) {
    Capsule c;
    c.P0 = feetPos + float3(0, radius, 0);           // bottom sphere center
    c.P1 = feetPos + float3(0, radius + 2*halfHeight, 0);  // top sphere center
    c.radius = radius;
    return c;
}
```

**HUD Proof Fields**:
- `P0.y` (bottom sphere center Y)
- `P1.y` (top sphere center Y)
- `H` (total height verification)

---

## Phase 2: Depenetration Safety Net

**Commit**: `a100801`

**Goal**: Push capsule out of geometry at tick start (spawn, teleport, edge cases).

**Algorithm**: `ResolveOverlaps_Capsule()`
1. Gather collision candidates via spatial hash
2. Sort + deduplicate by `cubeIdx` (determinism)
3. For each overlapping cube, compute push-out vector
4. Apply clamped total push

**Clamps**:
| Parameter | Value |
|-----------|-------|
| Per-cube clamp | 1.0f |
| Per-frame clamp | 2.0f |
| Max iterations | 4 |

**HUD Fields**:
- `depenApplied` (bool: was depenetration needed?)
- `depenTotalMag` (magnitude of total push)
- `depenClampTriggered` (bool: hit per-frame limit?)

---

## Phase 3: XZ Sweep/Slide MVP

**Commit**: `d765038`

**Goal**: Swept collision detection for horizontal movement with wall sliding.

**Algorithm**:
1. Expand AABB by capsule radius (Minkowski sum)
2. Sweep both P0 and P1 using slab method
3. Take earliest hit
4. Clip velocity against hit normal (wall slide)
5. Repeat up to MAX_SWEEPS

**Constants**:
| Constant | Value |
|----------|-------|
| MAX_SWEEPS | 2 (later 4) |
| OVERCLIP | 1.001 |
| STOP_EPSILON | 0.001 |

**Wall Sliding**: `ClipVelocity(vel, normal, OVERCLIP)`
```cpp
float3 ClipVelocity(float3 vel, float3 normal, float overbounce) {
    float backoff = dot(vel, normal) * overbounce;
    return vel - normal * backoff;
}
```

**HUD Fields**:
- `sweepTOI` (time of impact [0..1])
- `sweepNormal` (hit surface normal)
- `slideVec` (resulting slide velocity)

---

## Debug Narrative: Phase 3 Bug Fixes

### Bug 1: Y-Overlap False Wall Hits

**Commit**: `348e6e4`

**Symptom**: Jitter and blocking when standing on cube tops or floor adjacent to cubes.

**Root Cause**: Y-overlap check included bottom hemisphere which touches support cubes (floor, cubes standing on). This falsely triggered XZ sweep against cubes the player is properly supported by.

**Fix**:
1. When grounded, use body-only Y range (exclude bottom hemisphere)
2. Skip cubes where capsule is standing on top (gap < threshold)

**Key Logic**:
```cpp
if (grounded) {
    yRangeMin = feetPos.y + radius;  // body bottom, not hemisphere bottom
}
if (cubeTop <= feetPos.y + epsilon) {
    continue;  // standing on this cube, skip for XZ
}
```

---

### Bug 2: Side-Wall Penetration

**Commit**: `de40f02`

**Symptom**: Capsule pushed INTO walls instead of away from them.

**Root Cause**: Overlap normal in `CapsuleAABBOverlap` was inverted (pointing from capsule to AABB instead of AABB to capsule).

**Fix**:
1. Negate overlap normal direction
2. Increase MAX_SWEEPS: 2 -> 4

**Before**: `normal = diff / dist;` (capsule -> AABB)
**After**: `normal = -diff / dist;` (AABB -> capsule)

---

### Bug 3: Ineffective XZ Cleanup Loop

**Commit**: `7475f0c`

**Hypothesis**: AABB mode runs XZ cleanup each Y-iteration; Capsule mode didn't.

**Implementation**: Added capsule XZ cleanup inside Y-iteration loop.

**Result**: Bug persisted. This was not the root cause. Documented as failed fix for reference.

---

### Bug 4: Visual/Collision Radius Mismatch

**Commit**: `cbb0794`

**Symptom**: Visual overlap with cubes but HUD shows "Penetrations: 0".

**Root Cause**: Collision radius didn't match visual extent.
- Collision: `capsuleRadius = 0.8`
- Visual extent: ~1.4 (character model width)

**Fix**:
| Parameter | Before | After |
|-----------|--------|-------|
| capsuleRadius | 0.8 | 1.4 |
| capsuleHalfHeight | 1.7 | 1.1 |

Total height preserved: `2*1.4 + 2*1.1 = 5.0`

---

### Bug 5: Stuck-at-Wall Escape

**Commit**: `cbb0794`

**Symptom**: After touching wall, couldn't move in any direction (including away from wall).

**Root Cause**: Sweep TOI = 0 for all directions when exactly at contact. The slab method returned immediate hit even for movement away from wall.

**Fix**: If `tEnter <= 0` AND both tNear values are negative (already past both slabs), return no-hit to allow escape.

**Key Logic**:
```cpp
if (tEnter <= 0.0f) {
    // Already inside or past - check if we're moving out
    if (tNear.x < -epsilon && tNear.z < -epsilon) {
        return SweepHit{ 1.0f, {}, -1 };  // no hit, allow move
    }
}
```

---

## Evidence

**Screenshots**:
- `docs/notes/img/day3.11/cur.png` - Current state after fixes
- `docs/notes/img/day3.11/previus.png` - Previous state (bug visible)

**Debug Instrumentation**:
```cpp
#define DEBUG_Y_XZ_PEN
```

**Log Tags**:
| Tag | Purpose |
|-----|---------|
| `[MODE]` | Controller mode switch |
| `[RESPAWN]` | Reset state log |
| `[DEPEN]` | Depenetration iterations |
| `[SWEEP]` | Sweep TOI and normal |
| `[SLIDE]` | Wall slide computation |

---

## Summary Table

| Phase | Commit | Feature |
|-------|--------|---------|
| P0 | `1665692` | Mode toggle + reset |
| P1 | `fdf832d` | Capsule SSOT + HUD |
| P2 | `a100801` | Depenetration |
| P3 | `d765038` | XZ Sweep/Slide MVP |
| Fix | `348e6e4` | Y-overlap false hits |
| Fix | `de40f02` | Normal inversion |
| Fix | `7475f0c` | XZ cleanup (ineffective) |
| Fix | `cbb0794` | Radius match + escape |

---

## Key Contracts

1. **Feet-bottom anchor**: `posY` = lowest point of capsule (same as AABB mode)
2. **Determinism**: Collision candidates sorted + deduped by `cubeIdx`
3. **Mode toggle**: F6 key, persists across respawn
4. **Depen stats protected**: Not overwritten by other collision code paths
5. **SSOT geometry**: All capsule calculations derive from `MakeCapsuleFromFeet()`

---

## Files Modified

| File | Changes |
|------|---------|
| Engine/WorldState.h | ControllerMode enum, capsule params, depen stats |
| Engine/WorldState.cpp | ResolveOverlaps_Capsule, SweepCapsuleXZ, ClipVelocity |
| Renderer/DX12/Dx12Context.h | Mode display, capsule HUD fields |
| Renderer/DX12/ImGuiLayer.h | Capsule debug section |
| Renderer/DX12/ImGuiLayer.cpp | Mode toggle display, sweep/depen stats |
| DX12EngineLab.cpp | F6 key handler |
