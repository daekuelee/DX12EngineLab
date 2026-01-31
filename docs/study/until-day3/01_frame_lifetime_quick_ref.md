# Frame Lifetime Quick Reference

## SSOT

Deep details: [until-day2/D_resource_lifetime.md](../until-day2/D_resource_lifetime.md)

---

## Contract Checklist (CPU↔GPU Lifetime)

### A. Frame Ring Invariants

- [ ] `FrameCount = 3` (triple buffered)
- [ ] `frameIndex = frameId % FrameCount` (NOT backbuffer index)
- [ ] Each FrameContext has: cmdAllocator, uploadAllocator, fenceValue, srvSlot
- [ ] Upload capacity = 1MB per frame (hardcoded)

### B. Fence Contracts

- [ ] Fence created once at init (initial value = 0)
- [ ] **EndFrame**: `++fenceCounter`, then `Signal(fence, fenceCounter)`
- [ ] **BeginFrame**: if `fenceValue != 0`, `WaitForFence(fenceValue)`
- [ ] Allocator reset ONLY after fence wait completes

### C. Safe Reuse Boundaries

| Resource | Safe After | Reason |
|----------|------------|--------|
| cmdAllocator | Fence wait | GPU done with all recorded commands |
| uploadAllocator | Fence wait | GPU finished copying data |
| transformsHandle | Always safe | Persistent, never reset |

### D. Critical Ordering

```
BeginFrame:
  1. Wait for fence (GPU done with frame N-3)
  2. Reset cmdAllocator
  3. Reset uploadAllocator
  4. Record new commands

EndFrame:
  1. ExecuteCommandLists
  2. ++fenceCounter
  3. Signal(fence, fenceCounter)
  4. Store fenceCounter in ctx.fenceValue
```

---

## Code Anchors

| Contract | Anchor |
|----------|--------|
| FrameCount | `FrameContextRing.h::FrameCount` (=3) |
| Frame index calc | `FrameContextRing.cpp:153` |
| Fence signal | `FrameContextRing.cpp::EndFrame():174-175` |
| Fence wait | `FrameContextRing.cpp::BeginFrame():157-160` |
| Allocator reset | `FrameContextRing.cpp::BeginFrame():163-164` |
| Upload capacity | `FrameContextRing.cpp:63` (1MB) |

---

## Failure Linkage

### → Cookbook #10: HUD Shows Stale Data

**Symptom**: CollisionStats on HUD don't match expected values.

**Causal chain**: If snapshot built from wrong frame's CollisionStats → stale HUD.

**Frame contract violated**: `BuildSnapshot()` must use current tick's stats, not cached.

**Check**: Verify frame index consistency between simulation and HUD update.

### → Cookbook #5: Pawn Explodes/Teleports (indirect)

**Symptom**: Pawn suddenly at extreme position.

**Causal chain**: If frame stomp occurs (allocator reset while GPU still reading) → transforms corrupted → extreme positions.

**Frame contract violated**: Fence wait must precede allocator reset.

**Check**: Verify fence wait path in BeginFrame.

### → Debug Layer: OBJECT_DELETED_WHILE_STILL_IN_USE

**Symptom**: D3D12 error in output window.

**Causal chain**: Resource released before GPU finished using it.

**Frame contract violated**: Fence not waited before resource destruction.

---

## Proof Steps

### Docs-Only Verification

```bash
# Verify signal/wait sites
grep -n "fenceValue" Renderer/DX12/FrameContextRing.cpp

# Verify allocator reset after wait
grep -n "Reset()" Renderer/DX12/FrameContextRing.cpp

# Verify FrameCount constant
grep -n "FrameCount" Renderer/DX12/FrameContextRing.h
```

### Runtime Verification

1. **Debug layer ON**: 0 errors expected in happy path
2. **F2 stomp test**: Expected flicker (intentional fence bypass proves fence is necessary)
3. **U key HUD**: Upload section shows `allocCalls`, `allocBytes` per frame
4. **VS Output**: Check for fence-related PROOF logs

### PIX Verification (Optional)

1. Take GPU Capture → open Timeline view
2. Verify `ExecuteCommandLists` → `Signal` ordering
3. Check fence value progression at frame boundaries
4. Confirm no overlapping frame work on GPU timeline

---

## Quick Debug Decision Tree

```
Symptom: Visual corruption / teleporting objects
  │
  ├─ Debug layer errors?
  │   └─ Yes → Read error, likely resource lifetime issue
  │
  ├─ F2 makes it worse?
  │   └─ Yes → Fence/sync issue confirmed
  │
  └─ HUD upload stats abnormal?
      └─ Yes → Upload allocator overrun or stomp
```

---

## See Also

- [D_resource_lifetime.md](../until-day2/D_resource_lifetime.md) - Full frame ring architecture
- [04_day3_failure_cookbook.md](../../onboarding/pass/04_day3_failure_cookbook.md) - Failure patterns
