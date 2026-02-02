# Fixed-Step Simulation Pipeline Deep Study

## Purpose

Document the Day3 fixed-step simulation loop with refactor-safe contracts.
Explains accumulator, input sampling, TickFixed execution, and output snapshot flow.

---

## Mental Model

```
Frame Tick
    │
    ├─ frameDt = renderer.GetDeltaTime()
    ├─ accumulator += frameDt
    ├─ accumulator = min(accumulator, 0.25)  ← spiral-of-death cap
    │
    ├─ ApplyMouseLook(pending deltas)        ← ONCE per frame
    ├─ Sample keyboard → InputState
    │
    ├─ while (accumulator >= FIXED_DT):      ← 0..15 iterations
    │   ├─ TickFixed(input, FIXED_DT)
    │   │   ├─ Reset CollisionStats
    │   │   ├─ Apply rotation/movement
    │   │   ├─ Physics integration
    │   │   ├─ Collision resolution
    │   │   └─ Populate CollisionStats
    │   └─ accumulator -= FIXED_DT
    │
    ├─ TickFrame(frameDt)                    ← smooth camera/FOV
    ├─ BuildSnapshot() → HUDSnapshot         ← AFTER all TickFixed
    ├─ SetFrameCamera(viewProj)
    ├─ SetHUDSnapshot(snap)
    └─ Render()
```

---

## Code Anchors

| Component | Anchor | Notes |
|-----------|--------|-------|
| FIXED_DT constant | `App.h::FIXED_DT` | 1/60s (60Hz) |
| Accumulator add | `App.cpp::App::Tick` | `m_accumulator += frameDt` |
| Accumulator cap | `App.cpp::App::Tick` | `if (m_accumulator > 0.25f)` |
| Fixed-step loop | `App.cpp::App::Tick` | `while (m_accumulator >= FIXED_DT)` |
| TickFixed | `WorldState.cpp::TickFixed` | Physics tick entry |
| InputState | `WorldState.h::InputState` | Input struct |
| InputSampler | `InputSampler.h::Sample` | Keyboard polling |
| Mouse apply | `App.cpp::App::Tick` | `ApplyMouseLook()` before loop |
| Jump edge | `App.cpp::App::Tick` | `m_prevJump` tracking |
| CollisionStats | `WorldState.h::CollisionStats` | Output diagnostics |
| Stats reset | `WorldState.cpp::TickFixed` | `m_collisionStats = {}` |
| BuildSnapshot | `WorldState.cpp::BuildSnapshot` | Package outputs |
| Snapshot call | `App.cpp::App::Tick` | After fixed loop |

---

## Contracts

### Contract 1: Accumulator & Cap (Spiral-of-Death Prevention)

**Invariants:**
- [ ] Accumulator capped at 0.25 seconds
- [ ] Cap applied BEFORE fixed-step loop entry
- [ ] Prevents runaway simulation when frame time spikes

**Break Symptoms:**
- Simulation runs forever on lag spike (no cap)
- Game freezes then catches up with many physics steps (cap too high)
- Physics skips (cap too low, losing simulation time)

**Guardrails:**
- Cap value (0.25s) allows max 15 steps at 60Hz
- Must be >= 2*FIXED_DT to allow at least 2 steps

**Proof (docs-only):**
```bash
rg "0\.25" Engine/App.cpp  # Verify cap value
rg "m_accumulator" Engine/App.cpp  # Verify accumulation + cap sites
```

**Proof (runtime):**
1. Run normally: HUD shows smooth updates
2. Trigger lag (resize window): Observe max ~15 physics steps catch-up

---

### Contract 2: Max Fixed Steps Per Frame

**Invariants:**
- [ ] Max steps = floor(0.25 / FIXED_DT) = 15
- [ ] Each step consumes exactly FIXED_DT from accumulator
- [ ] Loop exits when accumulator < FIXED_DT

**Break Symptoms:**
- Determinism loss if step count varies unexpectedly
- Performance spike if too many steps allowed
- Physics desync in multiplayer (if added later)

**Guardrails:**
- FIXED_DT is constexpr (cannot change at runtime)
- Accumulator cap bounds worst-case step count

**Proof (docs-only):**
```bash
rg "FIXED_DT" Engine/App.h  # Verify constant = 1/60
rg "while.*m_accumulator" Engine/App.cpp  # Verify loop condition
```

---

### Contract 3: Input Sampling

**Invariants:**
- [ ] Mouse look applied ONCE per frame (before fixed loop)
- [ ] Keyboard sampled ONCE per frame into InputState
- [ ] Same InputState used for ALL TickFixed calls in frame
- [ ] Jump is edge-detected (rising edge only)

**Break Symptoms:**
- Double-jump per frame (jump not edge-detected)
- Mouse look jitter (applied per TickFixed instead of per frame)
- Input lag (sampled too early/late in frame)

**Guardrails:**
- `m_prevJump` tracks previous frame's jump state
- `ApplyMouseLook()` consumes and resets pending deltas

**Proof (docs-only):**
```bash
rg "ApplyMouseLook" Engine/App.cpp  # Verify before while loop
rg "m_prevJump" Engine/App.cpp  # Verify edge detection
rg "InputSampler::Sample" Engine/App.cpp  # Verify single call
```

---

### Contract 4: Output Snapshot

**Invariants:**
- [ ] BuildSnapshot() called ONCE per frame, AFTER all TickFixed
- [ ] Snapshot captures final CollisionStats from last TickFixed
- [ ] Snapshot passed to renderer before Render() call

**Break Symptoms:**
- HUD shows stale data (snapshot called before TickFixed)
- HUD shows intermediate state (snapshot called mid-loop)
- Renderer uses wrong frame's data (snapshot not passed)

**Guardrails:**
- Call order in App::Tick is fixed: loop → TickFrame → BuildSnapshot → Set* → Render

**Proof (docs-only):**
```bash
rg -A5 "while.*m_accumulator" Engine/App.cpp  # Verify BuildSnapshot after loop
rg "SetHUDSnapshot" Engine/App.cpp  # Verify snapshot passed to renderer
```

---

### Contract 5: Toggle Interaction

**Invariants:**
- [ ] F6 (controller mode): Changes collision algorithm, not timing
- [ ] F7 (step grid): Rebuilds collision world, resets pawn position
- [ ] F8 (verbose HUD): Display-only, no simulation effect

**Break Symptoms:**
- F6 changes simulation speed (bug: algo affects timing)
- F7 causes physics glitch (world rebuild not atomic)
- F8 causes perf drop even when hidden (bug: always computing)

**Guardrails:**
- Toggles are pure state flips
- World rebuild clears and repopulates spatial grid atomically

**Proof (docs-only):**
```bash
rg "ToggleControllerMode" Engine/WorldState.cpp  # Verify just mode flip
rg "ToggleStepUpGridTest" Engine/WorldState.cpp  # Verify atomic rebuild
rg "IsHudVerboseEnabled" Renderer/DX12/ImGuiLayer.cpp  # Verify conditional compute
```

---

## Failure Linkage

### → Cookbook #10: HUD Shows Stale/Wrong Collision Data

**Causal chain:**
1. BuildSnapshot() called at wrong time (before TickFixed completes)
2. OR CollisionStats not reset at TickFixed start
3. → HUD displays previous tick's collision data

**Contract violated:** Output Snapshot (Contract 4)

**Proof:** Verify `m_collisionStats = {}` at TickFixed:304

### → Cookbook #4: Jump Doesn't Work / onGround Stuck

**Causal chain:**
1. Jump edge detection broken (m_prevJump not updated)
2. OR input.jump consumed by TickFixed but ignored due to onGround=false
3. → Player cannot jump or jumps unexpectedly

**Contract violated:** Input Sampling (Contract 3)

**Proof:** Check m_prevJump update after InputSampler::Sample

### → Cookbook #2: Pawn Clips (Fast Movement)

**Causal chain:**
1. Large frameDt → large accumulator
2. Single TickFixed moves pawn by walkSpeed * FIXED_DT
3. If walkSpeed * FIXED_DT > sweep distance, clipping occurs

**Contract violated:** Accumulator cap + sweep distance relationship

**Proof:** Verify walkSpeed * FIXED_DT < minimum obstacle thickness

---

## Proof Steps

### Docs-Only Verification
```bash
# 1. Verify accumulator cap
rg "0\.25" Engine/App.cpp

# 2. Verify FIXED_DT constant
rg "FIXED_DT" Engine/App.h

# 3. Verify fixed-step loop structure
rg -B2 -A4 "while.*m_accumulator" Engine/App.cpp

# 4. Verify input sampling order
rg -n "ApplyMouseLook|InputSampler|while.*m_accumulator" Engine/App.cpp

# 5. Verify BuildSnapshot after loop
rg -n "BuildSnapshot|SetHUDSnapshot" Engine/App.cpp
```

### Runtime Verification
1. **Normal operation**: Launch, move around, HUD updates smoothly
2. **Stress test**: Resize window rapidly → observe physics catches up (max 15 steps)
3. **F6**: Toggle controller mode → collision behavior changes, timing stable
4. **F7**: Toggle step grid → world rebuilds, pawn respawns
5. **F8**: Toggle verbose HUD → see Solver/Floor/MTV sections

---

## Refactor Checklist

When refactoring Tick/WorldState, preserve:

| Must Preserve | Reason |
|---------------|--------|
| Accumulator cap (0.25s) | Prevents spiral of death |
| FIXED_DT constexpr | Determinism guarantee |
| Input sample before loop | Consistent input across steps |
| Mouse apply once per frame | No jitter |
| Jump edge detection | No double-jump |
| CollisionStats reset per tick | Fresh diagnostics |
| BuildSnapshot after loop | Correct HUD data |
| Snapshot before Render | Renderer has current data |

**Safe to change:**
- Internal TickFixed physics (as long as stats populated)
- Toggle key bindings (F6/F7/F8)
- HUD layout (display-only)
- walkSpeed, gravity values (tuning)

---

## See Also

- [01_frame_lifetime_quick_ref.md](01_frame_lifetime_quick_ref.md) - Frame sync contracts
- [02_binding_abi_quick_ref.md](02_binding_abi_quick_ref.md) - Binding contracts
- [04_day3_failure_cookbook.md](../../onboarding/pass/04_day3_failure_cookbook.md) - Failure patterns
