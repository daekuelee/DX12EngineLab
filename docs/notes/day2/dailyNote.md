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
- Geometry uploads (mesh streaming)
- Compute writes (indirect args, counters)
- Texture streaming uploads
- OOM handling / fallback strategies
- Allocation profiling / timeline visualization
