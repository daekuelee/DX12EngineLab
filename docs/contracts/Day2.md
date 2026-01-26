# Day2 Contract

## Header
- Date (KST): 2026-01-26
- Day: Day2
- Goal (one sentence): Create UploadArena as unified allocation front-door with diagnostic HUD instrumentation.

## Must (DoD)
- UploadArena module wraps FrameLinearAllocator as single entry point for dynamic uploads
- Per-frame metrics tracking: allocCalls, allocBytes, peakOffset, capacity
- HUD displays Upload Arena section when U key toggle is enabled
- Runtime behavior identical to Day1.7 (no functional changes)
- No new per-frame heap allocations

## Invariants (mechanism claims)
- UploadArena is pure passthrough + instrumentation (wrapper pattern)
- Instance-level metrics (m_frameMetrics/m_lastSnapshot), no static storage
- HUD stores by-value copy of snapshot (avoids lifetime coupling)
- Truthful mapCalls: constexpr 1 (persistent map reality)
- Diag mode gated: logging/HUD only when U toggle is ON
- Allocation calls preserve original alignment, order, and sizes

## Evidence
- Logs:
  - Path: Debug output
  - What it proves: "UploadDiag: ON/OFF" toggle messages
- Screenshot/Capture:
  - Path: captures/day2/
  - What it proves: HUD showing allocCalls=2, allocBytes~640KB, peakOffset/capacity %

## Expected Metrics
- allocCalls: 2 per frame (FrameCB + Transforms)
- allocBytes: ~640KB per frame (256B CB + 640000B transforms)
- peakOffset: ~640KB (both allocations fit in 1MB capacity)
- capacity: 1048576 (1MB per-frame allocator)

## Future Extension Points (Not in Current Scope)

UploadArena currently handles per-frame dynamic uploads:
- Frame constants (CB)
- Per-instance transforms

**Future per-frame upload candidates** (would go through UploadArena):
- Dynamic vertex/index buffer updates (animated meshes)
- Geometry streaming (LOD, open world chunks)
- Indirect draw argument buffers
- Compute-written buffers (GPU readback staging)

**NOT candidates** (remain separate):
- GeometryFactory: Init-time static geometry uploads (one-shot, synchronous)
- Texture streaming: Would need separate async upload queue

## Notes / Bugs / Risks
- peakOffset must be updated on every successful allocation (not just End())
- lastAllocOffset uses returned Allocation.offset, never re-computed
- Failed allocations (cpuPtr==nullptr) do not update metrics
- First-frame null guard: m_hasUploadMetrics flag prevents undefined HUD display

## Next
- Day2.5: Failure mode proof (upload=bad/arena toggle for intentional corruption)
- Day3: GPU culling infrastructure
