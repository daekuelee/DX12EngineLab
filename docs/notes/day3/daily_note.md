# Day3 Daily Note

## Objective

Implement ECS-lite game simulation layer: fixed-step physics, third-person camera with sprint FOV, WASD movement, collision system with spatial hash broadphase, and floor/cube collision with KillZ respawn.

## Plan (from Day3 plan docs)

**Source**: `docs/contracts/day3/plan1.md`, `docs/contracts/day3/plan1-colisoncube.md`, `docs/contracts/day3/planbig.md`

### Plan -> Execution Mapping

| Plan Item | Status | Evidence |
|-----------|--------|----------|
| ECS-lite WorldState (InputState, PawnState, CameraState) | Done | `Engine/WorldState.h` |
| Fixed-step physics (60Hz) | Done | `TickFixed()` with accumulator cap |
| Camera-relative WASD + sprint | Done | `InputSampler.h`, `WorldState::TickFixed()` |
| Third-person follow camera | Done | `BuildViewProj()` with smooth offset |
| Jump with hitch-safe consumption | Done | `m_jumpConsumedThisFrame` flag |
| V key camera mode toggle | Done | `DX12EngineLab.cpp` |
| Spatial hash broadphase | Done | 100x100 grid, `QuerySpatialHash()` |
| AABB collision (axis-separated) | Done | X->Z->Y resolution |
| Floor collision with bounds | Done | +/-200 bounds, epsilon tolerance |
| KillZ respawn | Done | Y=-50 trigger |
| HUD proof fields | Done | ImGuiLayer collision/floor panels |

**Scope Boundaries**:
- No file loading / no scene graph
- Day2 infra internals unchanged (UploadArena, ResourceStateTracker, DescriptorRingAllocator)
- All GPU uploads via UploadArena
- Debug layer clean on happy path

## What I changed (scope)

**20 files changed, +1663/-18 lines**

### New Files
| File | Purpose |
|------|---------|
| `Engine/WorldState.h` | Pawn/Camera/Map state, collision config |
| `Engine/WorldState.cpp` | Fixed-step physics, collision resolution |
| `Engine/InputSampler.h` | Header-only GetAsyncKeyState sampling |
| `Renderer/DX12/CharacterPass.h` | Character render pass declaration |
| `Renderer/DX12/CharacterRenderer.h/cpp` | Character cube rendering |

### Modified Files
| File | Change |
|------|--------|
| `Engine/App.h/cpp` | WorldState integration, fixed-step loop |
| `Renderer/DX12/Dx12Context.h/cpp` | Camera injection, HUD snapshot API |
| `Renderer/DX12/ImGuiLayer.h/cpp` | World State + Collision + Floor debug panels |
| `Renderer/DX12/ToggleSystem.h` | CameraMode enum, debug toggles |
| `Renderer/DX12/GeometryPass.h` | MT1 validation hooks |
| `shaders/cube_vs.hlsl` | MT3 NaN/Inf shader check |
| `shaders/cube_ps.hlsl` | Magenta output for invalid transforms |

## Debugging narrative (chronological)

### Phase 1: ECS-lite Foundation (commit `4a8686b`)
- Added WorldState with fixed-step physics
- Implemented camera-relative WASD, sprint, jump
- Third-person camera follow with FOV smoothing
- **No issues** - worked on first run

### Phase 2: Character Rendering + Mouse Look (commits `fc39a47`, `caf6554`)
- Added CharacterPass for player cube rendering
- Implemented mouse look (yaw/pitch from mouse delta)
- **Bug encountered**: Descriptor ring wrap crash after ~1000 frames

**Evidence**: `docs/notes/img/day3/crashlog.txt`
```
[DescRing] RETIRE CONTRACT VIOLATION! rec.start=1024 != tail=3 fence=1022 count=1 head=6
```

**Fix**: Persistent SRV slot for character transforms instead of per-frame allocation.

### Phase 3: Collision System (commit `1c1dd19`)
- Added spatial hash broadphase (100x100 grid)
- AABB pawn vs cube collision
- Axis-separated resolution (X->Z->Y)
- Floor collision with bounds, KillZ respawn

**Bug 1**: Pawn pushed INTO cubes instead of away
- Root cause: Penetration sign inverted in `CalcAxisPenetration()`

### Phase 4: Penetration Sign Fix (commit `1b8b496`)

**Key diff** (`Engine/WorldState.cpp:289`):
```cpp
// BEFORE (wrong):
float sign = (centerPawn < centerCube) ? -1.0f : 1.0f;

// AFTER (correct):
float sign = (centerPawn < centerCube) ? 1.0f : -1.0f;
```

**Also fixed**:
- Floor bounds restored to +/-100 (was accidentally shrunk to +/-50)
- Spawn position moved to (0,0,0) gap between cubes
- Jump velocity increased for cube-top access

### Phase 5: onGround Toggle Bug (commit `6131c19`)

**Symptom**: `onGround` flickering ON/OFF every tick while standing still

**Evidence from logs**:
```
[FLOOR-B] pawnBot=-0.008 floorY=0.000 belowFloor=1 inBounds=1 didClamp=1 velY=0.00 onGround=1
[FLOOR-B] pawnBot= 0.000 floorY=0.000 belowFloor=0 inBounds=1 didClamp=0 velY=0.00 onGround=0
```

**Root cause**: Strict `<` comparison failed when pawn rested exactly at `floorY=0`

**Key diff** (`Engine/WorldState.cpp:177-190`):
```cpp
// BEFORE:
bool belowFloor = (pawnBottomY < m_config.floorY);
if (inFloorBounds && belowFloor) { ... }

// AFTER:
const float eps = 0.01f;  // 1cm tolerance
bool touchingFloor = (pawnBottomY <= m_config.floorY + eps);
if (inFloorBounds && touchingFloor && m_pawn.velY <= 0.0f) { ... }
```

### Phase 6: Floor Bounds Mismatch (commit `71a8dca`)

**Symptom**: Pawn falls through floor near grid edges

**Root cause**: Floor collision bounds (+/-100) didn't match rendered floor geometry (+/-200)

**Key diff** (`Engine/WorldState.h:96-103`):
```cpp
// BEFORE:
float floorMinX = -100.0f;
float floorMaxX = 100.0f;

// AFTER:
float floorMinX = -200.0f;
float floorMaxX = 200.0f;
```

## Contracts / Invariants (what must remain true)

1. **onGround contract**: Only set `true` by floor/cube collision resolution, never forced
2. **Jump consumption**: At most once per render frame via `m_jumpConsumedThisFrame` flag
3. **Axis-separated collision**: Resolve X, then Z, then Y (order matters for stability)
4. **Deepest penetration only**: Apply single deepest correction per axis (avoid jitter)
5. **Frame-scoped camera injection**: Reset `m_useInjectedCamera=false` at Render() start
6. **UploadArena unchanged**: All uploads still go through Day2 arena

## Evidence bundle

### Commits
| Hash | Message |
|------|---------|
| `4a8686b` | feat(gfx): Day3 - Add ECS-lite game simulation with third-person camera |
| `fc39a47` | feat(gfx): Day3 - Add split rendering with CharacterPass and mouse look |
| `caf6554` | fix(gfx): Day3 - Fix descriptor ring wrap crash with persistent character SRV |
| `1c1dd19` | feat(gfx): Day3 - Add collision system with spatial hash and rendering triage |
| `1b8b496` | fix(gfx): Day3 - Fix collision penetration sign and floor bounds |
| `7aa9840` | feat(gfx): Day3 - Add floor collision diagnostic logs and HUD panel |
| `6131c19` | fix(gfx): Day3 - Fix onGround toggle bug when standing on floor |
| `71a8dca` | fix(gfx): Day3 - Expand floor collision bounds to match rendered geometry |

### Diff stats
```
20 files changed, +1663/-18 lines
```

### Changed files
```
DX12EngineLab.cpp
DX12EngineLab.vcxproj
Engine/App.cpp
Engine/App.h
Engine/InputSampler.h
Engine/WorldState.cpp
Engine/WorldState.h
Renderer/DX12/CharacterPass.h
Renderer/DX12/CharacterRenderer.cpp
Renderer/DX12/CharacterRenderer.h
Renderer/DX12/Dx12Context.cpp
Renderer/DX12/Dx12Context.h
Renderer/DX12/GeometryPass.h
Renderer/DX12/ImGuiLayer.cpp
Renderer/DX12/ImGuiLayer.h
Renderer/DX12/RenderScene.h
Renderer/DX12/ToggleSystem.h
docs/contracts/Day2.md
shaders/cube_ps.hlsl
shaders/cube_vs.hlsl
```

### Key diffs

**1. Penetration sign fix** (`1b8b496` - `Engine/WorldState.cpp:289`):
```cpp
// Push pawn AWAY from cube center, not toward it
float sign = (centerPawn < centerCube) ? 1.0f : -1.0f;  // was -1.0f : 1.0f
```
*Why it matters*: Without this fix, collision pushed pawn INTO cubes causing stuck/clipping.

**2. Floor epsilon tolerance** (`6131c19` - `Engine/WorldState.cpp:177-181`):
```cpp
const float eps = 0.01f;
bool touchingFloor = (pawnBottomY <= m_config.floorY + eps);
if (inFloorBounds && touchingFloor && m_pawn.velY <= 0.0f) { ... }
```
*Why it matters*: Strict `<` comparison caused onGround to toggle off when resting exactly at floor.

**3. Floor bounds expansion** (`71a8dca` - `Engine/WorldState.h:99-103`):
```cpp
float floorMinX = -200.0f;  // was -100.0f
float floorMaxX = 200.0f;   // was 100.0f
```
*Why it matters*: Collision bounds must match rendered geometry or pawn falls through visible floor.

### Visual proof
- [x] Screenshot: HUD with collision stats visible - `docs/notes/img/day3/image1.png`
- [x] Screenshot: Third-person camera view - `docs/notes/img/day3/image2.png`
- [x] Crash log: Descriptor ring violation - `docs/notes/img/day3/crashlog.txt`
- [ ] Debug layer: 0 errors screenshot/log (TODO)
- [ ] Optional: PIX/RenderDoc capture id

## Risks / Follow-ups

1. **Tunneling at high speeds**: Current collision doesn't handle fast-moving objects; may need swept AABB or substeps for Y axis if jump velocity increases
2. **Spatial hash static**: Grid built once at init; if cubes become dynamic, need rebuild mechanism
3. **Floor/cube priority**: Floor collision runs after cube collision; edge cases at cube/floor boundaries may need tuning
4. **Epsilon sensitivity**: 1cm floor tolerance works for current scale; may need adjustment for different world scales
