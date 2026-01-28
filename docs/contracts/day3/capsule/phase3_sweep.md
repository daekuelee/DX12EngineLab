# Phase 3: Capsule XZ Sweep/Slide MVP

## Summary
Implemented XZ sweep/slide for Capsule mode using slab-method ray-AABB intersection with Minkowski sum expansion.

## Scope
- XZ sweep/slide for Capsule mode only
- Prevents tunneling through thin geometry at high speeds
- Enables smooth wall sliding instead of hard stop
- Y-axis still uses existing path (gravity, support query)

## Algorithm
1. Compute requested `delta = (velX * dt, velZ * dt)` in XZ
2. Build swept AABB for broadphase
3. For each candidate cube (sorted for determinism):
   - Expand cube by capsule radius (Minkowski sum)
   - Sweep point against expanded AABB using slab method
   - Y overlap check: capsule Y range vs cube Y range
   - Take earliest t in [0,1]
4. If no hit: apply full delta
5. If hit: move to contact (t - skin), compute slide, do 1 more sweep

## Key Parameters
- `SKIN_WIDTH = 0.01f` - Offset from contact to prevent floating point issues
- `MAX_SWEEPS = 2` - Original sweep + 1 slide sweep
- `OVERCLIP = 1.001f` - Slide velocity clipping factor

## Files Modified
| File | Changes |
|------|---------|
| `Engine/WorldState.h` | Added sweep stats to CollisionStats, SweepXZ_Capsule() declaration |
| `Engine/WorldState.cpp` | Added sweep helpers, SweepXZ_Capsule(), TickFixed integration |
| `Renderer/DX12/Dx12Context.h` | Added sweep fields to HUDSnapshot |
| `Renderer/DX12/ImGuiLayer.h` | Added sweep fields to WorldStateFields |
| `Renderer/DX12/ImGuiLayer.cpp` | Added snapshot copy + HUD display |

## Invariants Maintained
- Runs ONLY when `ControllerMode::Capsule`
- Phase 2 depen runs FIRST (unchanged), sweep runs AFTER
- Candidates: `std::sort` + `std::unique` by cubeIdx (determinism)
- Max 2 sweeps per tick
- AABB mode: completely unchanged (existing ResolveXZ_MTV path)
- SSOT: r/hh from `m_config.capsuleRadius`/`capsuleHalfHeight` only

## HUD Display (Capsule Mode)
```
-- Sweep --
HIT cube=X           (yellow if hit)
TOI:0.XXXX n=(X,Z)   (time of impact + normal)
Cand:N               (candidate count)
Req:(dx,dz)          (requested delta)
App:(dx,dz)          (applied delta)
Slide:(dx,dz)        (slide component if any)
```

## Debug Log Lines
- `[SWEEP] req=(dx,dz) cand=N hit=0|1 toi=... n=(nx,nz) cube=...`
- `[SLIDE] rem=(dx,dz) slide=(dx,dz)`

## Verification Checklist
| ID | Test | Pass Criteria |
|----|------|---------------|
| T1 | Wall block | HUD: `HIT cube=X`, `TOI` in (0,1], applied < requested |
| T2 | Slide | HUD: `HIT` + non-zero `Slide:(dx,dz)`, forward component preserved |
| T3 | AABB baseline | No `[SWEEP]`/`[SLIDE]` logs in AABB mode |

## NOT in Scope (Phase 3)
- Y-axis sweep (gravity still uses existing path)
- Step-up logic
- Slope/grounding changes
- Full Quake-style multi-bump (MAX_BUMPS=4)
