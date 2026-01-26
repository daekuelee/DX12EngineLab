# Day2 Daily Note: UploadArena + Diagnostic Instrumentation

## Date
2026-01-26

## Summary
Created `UploadArena` as a unified allocation front-door with diagnostic HUD instrumentation.

## Before
- Upload paths were scattered: direct `ctx.uploadAllocator.Allocate()` calls in:
  - `UpdateFrameConstants()` - CB allocation
  - `UpdateTransforms()` - transforms buffer allocation
- No visibility into allocation patterns or usage metrics
- Difficult to diagnose upload buffer issues

## After
- All uploads go through `UploadArena.Allocate()` (single entry point)
- Per-frame metrics tracking:
  - `allocCalls`: number of allocations this frame
  - `allocBytes`: total bytes allocated this frame
  - `peakOffset`: high-water mark within allocator
  - `capacity`: allocator capacity
  - `lastAllocTag/Size/Offset`: most recent allocation details
- HUD displays metrics when U key toggle is enabled
- Runtime behavior unchanged (pure wrapper + instrumentation)

## Key Files Changed
| File | Change |
|------|--------|
| `Renderer/DX12/UploadArena.h` | NEW - metrics struct + arena class |
| `Renderer/DX12/UploadArena.cpp` | NEW - Begin/Allocate/End implementation |
| `Renderer/DX12/ToggleSystem.h` | Add IsUploadDiagEnabled toggle |
| `Renderer/DX12/Dx12Context.h` | Add m_uploadArena member |
| `Renderer/DX12/Dx12Context.cpp` | Wire arena in Render(), update allocation calls |
| `Renderer/DX12/ImGuiLayer.h` | Add SetUploadArenaMetrics method |
| `Renderer/DX12/ImGuiLayer.cpp` | Add Upload section to HUD |
| `DX12EngineLab.cpp` | Add 'U' key handler |

## Evidence
- Toggle: Press U key
- Debug output: "UploadDiag: ON" / "UploadDiag: OFF"
- HUD (when ON): Shows Upload Arena section with metrics

## Design Decisions
1. **Wrapper pattern**: UploadArena wraps (doesn't own) FrameLinearAllocator
2. **Instance metrics**: No static storage, supports multiple arenas if needed
3. **Snapshot pattern**: m_frameMetrics mutated during frame, m_lastSnapshot stable for HUD
4. **By-value copy to HUD**: Avoids lifetime coupling between arena and ImGuiLayer
5. **Truthful mapCalls**: Returns constexpr 1 (persistent map reality)

## Future Extension Points

**Per-frame upload candidates** (would go through UploadArena):
- Dynamic vertex/index buffer updates (animated meshes)
- Geometry streaming (LOD, open world chunks)
- Indirect draw argument buffers
- Compute writes (indirect args, counters)

**NOT candidates** (remain separate):
- GeometryFactory: Init-time static geometry uploads (one-shot, synchronous)
- Texture streaming: Would need separate async upload queue
- OOM handling / fallback strategies (future work)
- Allocation profiling / timeline visualization (future work)

## GeometryFactory Exclusion Rationale

GeometryFactory was audited and **intentionally excluded** from UploadArena unification:

| Aspect | GeometryFactory | UploadArena |
|--------|-----------------|-------------|
| When called | Init-time only | Every frame |
| Upload buffer lifetime | Temporary (destroyed after copy) | Ring-buffered (reused after fence) |
| GPU sync | Synchronous wait | Async (fence-gated reuse) |
| Allocator type | Ephemeral per-call | Persistent per-frame ring |
| Purpose | Static geometry upload | Dynamic CB/transform updates |

**Classification**: Type (A) init-time staging uploads, NOT type (B) per-frame dynamic uploads.

**Pattern**: Temporary upload buffer created per-call, synchronous fence wait, immediate release after GPU copy completes.

**Call sites**: Only during `RenderScene::Initialize()` (CreateCubeGeometry, CreateFloorGeometry, CreateMarkerGeometry) - never per-frame.

**Decision**: Forcing GeometryFactory through UploadArena would waste per-frame allocator space on init-time data and mix incompatible lifecycles. Current design is correct.
