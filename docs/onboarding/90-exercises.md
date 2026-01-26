# Practice Exercises

These exercises help you internalize the codebase patterns.

---

## Exercise 1: Toggle Exploration

**Goal**: Understand runtime toggles and their visual effects.

### Steps

1. Build and run the Debug configuration
2. Press each toggle key and observe:
   - **T**: Does draw count change in `EVIDENCE:` output?
   - **C**: Do cube colors change?
   - **G**: Do cubes disappear (floor stays)?
   - **U**: Does HUD appear with upload metrics?
   - **M**: Do corner markers appear?

### Verification

- T toggle: Visual should be identical, `cpu_record_ms` differs
- C toggle: Three distinct color modes
- U toggle: Shows `allocCalls: 2`, `allocBytes: ~655KB`

### Bonus

Press F1 and find instance 0 at position (150, 50, 150).

---

## Exercise 2: Debug Layer Practice

**Goal**: Learn to read and fix debug layer errors.

### Steps

1. Open `Dx12Context.cpp`
2. Find `RecordBarriersAndCopy()` function
3. Comment out one `FlushBarriers()` call:
   ```cpp
   m_stateTracker.Transition(transformsResource, D3D12_RESOURCE_STATE_COPY_DEST);
   // m_stateTracker.FlushBarriers(m_commandList.Get());  // COMMENTED
   ```
4. Build and run
5. Check Output window for `D3D12 ERROR`
6. Read the error message - what does it tell you?
7. Restore the line and verify fix

### Expected Error

Something like:
```
D3D12 ERROR: ID3D12CommandList::CopyBufferRegion:
The resource state ... is invalid for ... COPY_DEST
```

---

## Exercise 3: Trace Frame Lifecycle

**Goal**: Understand the render loop timing.

### Steps

1. Add a breakpoint in `Dx12Context::Render()` at the start
2. Run and let it hit the breakpoint
3. Step through these phases:
   - `BeginFrame()` - what happens to allocator?
   - `UpdateFrameConstants()` - what's the returned offset?
   - `UpdateTransforms()` - what's the returned offset?
   - `RecordBarriersAndCopy()` - what barriers are emitted?
4. Note the relationship between `m_frameId` and `frameResourceIndex`

### Questions to Answer

- How does `frameId % 3` rotate through contexts?
- When does the fence wait actually happen?
- What's the upload allocator offset after both allocations?

---

## Exercise 4: Descriptor Math

**Goal**: Understand descriptor heap layout.

### Steps

1. Find the SRV heap initialization log in Output window:
   ```
   === SRV HEAP INIT (DescRing) ===
   descRing capacity=1024 reserved=3 descSize=XX
   heapGpuStart=0xXXXXXXXX
   frame[0] srvSlot=0 CPU=0xXXX GPU=0xXXX
   frame[1] srvSlot=1 CPU=0xXXX GPU=0xXXX
   frame[2] srvSlot=2 CPU=0xXXX GPU=0xXXX
   ```
2. Calculate: What's the GPU handle for frame 1's SRV?
   - Formula: `heapGpuStart + (srvSlot * descSize)`
3. Verify your calculation matches the logged value

### Bonus

Look at `PROOF:` log output. Does `actual` offset match `exp` offset?

---

## Exercise 5: Add a Simple Toggle

**Goal**: Practice the extension pattern.

### Steps

1. Add new toggle to `ToggleSystem.h`:
   ```cpp
   private:
       static inline bool s_wireframeEnabled = false;
   public:
       static bool IsWireframeEnabled() { return s_wireframeEnabled; }
       static void ToggleWireframe() { s_wireframeEnabled = !s_wireframeEnabled; }
   ```

2. Add key handler in `App.cpp` (or window proc):
   ```cpp
   case 'R':  // R for wireframe
       ToggleSystem::ToggleWireframe();
       OutputDebugStringA(ToggleSystem::IsWireframeEnabled() ?
           "Wireframe ON\n" : "Wireframe OFF\n");
       break;
   ```

3. Build and run - press R, check Output window

### Verification

Output should show "Wireframe ON" and "Wireframe OFF" alternating.

Note: Actually implementing wireframe rendering requires PSO changes - this exercise just adds the toggle infrastructure.

---

## Exercise 6: Upload Arena Investigation

**Goal**: Understand upload memory layout.

### Steps

1. Enable upload diagnostics (press U)
2. Look at HUD values:
   - `allocCalls`: Should be 2
   - `allocBytes`: Should be ~655KB
   - `peakOffset`: Should match allocBytes

3. Calculate expected values:
   - FrameCB: 256 bytes (aligned to 256)
   - Transforms: 10,000 × 64 = 640,000 bytes
   - Total: 640,256 bytes

4. Does the HUD match your calculation?

### Bonus

Add a third allocation in `Dx12Context::Render()`:
```cpp
Allocation testAlloc = m_uploadArena.Allocate(1024, 256, "TestAlloc");
```

Check HUD - does `allocCalls` become 3?

---

## Exercise 7: Frame Index Bug Hunt

**Goal**: Understand why frame index matters.

### Steps

1. Press F2 to enable "stomp lifetime" test
2. Observe the scene - is there flickering?
3. Check `PROOF:` output - does it say "MISMATCH"?
4. Press F2 again to disable
5. Find the code that implements this in `Dx12Context::Render()`:
   ```cpp
   if (ToggleSystem::IsStompLifetimeEnabled())
   {
       srvFrameIndex = (frameResourceIndex + 1) % FrameCount; // Wrong frame!
   }
   ```

### Questions

- Why does using the wrong frame cause flickering?
- What would happen with 2 frame contexts instead of 3?

---

## Exercise 8: Add Debug Output

**Goal**: Practice diagnostic logging.

### Steps

1. Find `GeometryPass::Execute()` in the codebase
2. Add logging at the start:
   ```cpp
   OutputDebugStringA("GeometryPass: Execute()\n");
   ```
3. Build and run
4. Check Output window - does it spam every frame?
5. Change to throttled logging:
   ```cpp
   DiagnosticLogger::LogThrottled("GEO", "GeometryPass: Execute()\n");
   ```
6. Verify it logs once per second now

---

## Self-Assessment

After completing these exercises, you should be able to answer:

1. What's the frame resource selection formula?
2. How many upload allocations happen per frame?
3. Where do per-frame SRVs live in the descriptor heap?
4. What does the debug layer tell you about state errors?
5. How do you add a new runtime toggle?
6. Why does wrong frame index cause visual corruption?

---

## Further Exploration

Once comfortable, try these advanced exercises:

### A. Add a New Color Mode

1. Add `Checkerboard = 3` to `ColorMode` enum
2. Update shader to implement checkerboard pattern
3. Update `CycleColorMode()` to include new mode

### B. Add Per-Instance Data

1. Add per-instance color to transforms buffer
2. Update SRV to include new data
3. Modify shader to use per-instance color

### C. Implement Frustum Culling

1. Add CPU-side frustum test
2. Skip instances outside frustum
3. Measure draw call reduction

---

## Study Path

### Read Now
- [glossary.md](glossary.md) - Terminology reference

### Read When Stuck
- [70-debug-playbook.md](70-debug-playbook.md) - Error troubleshooting
- [80-how-to-extend.md](80-how-to-extend.md) - Implementation patterns

### Read Later
- Contract docs in `docs/contracts/` - Phase requirements
