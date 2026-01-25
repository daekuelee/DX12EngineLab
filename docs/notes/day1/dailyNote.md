# Day1 Debugging Narrative: 10k Instanced Cubes

## 1. Goal & Initial Symptom

**Goal**: Render 10,000 cubes in two modes (Instanced vs Naive) with runtime toggle, producing visually identical output in both modes with no GPU lifetime bugs. **Initial symptom**: "Exploding/giant triangles" - screen-filling wedges instead of the expected 100x100 cube grid. Success criteria required both rendering modes to match exactly, with the debug layer showing zero errors on the happy path.

---

## 2. Timeline

| Checkpoint | Commit | Observation |
|------------|--------|-------------|
| Initial 10k instancing scaffold | `df195e4` | Giant wedges fill screen, not cubes |
| Top-down orthographic camera | `34caf64` | Still wrong geometry - rules out perspective issues |
| Diagnostic instrumentation | `a74d067` | Viewport/scissor verified OK |
| Microtest A: bypass SRV | `d05787d` | Instanced==Naive when bypassing SRV -> SRV path suspect |
| Microtest B: raw SRV diagnostic | `bf93d05` | Color sentinel shows data is readable from GPU |
| Per-frame state tracking fix | `4ed54f0` | Fixes DX12 barrier error #527 |
| Restore StructuredBuffer + row_major | `5cac813` | **Primary fix** - matrices now interpreted correctly |
| DirectXMath perspective camera | `6ecda94` | Proper 3D perspective view implemented |
| Camera presets with hotkeys | `2661185` | Runtime camera switching for debugging |
| Scale cubes for gaps | `fa87515` | Visual separation achieved (0.7 scale) |
| Fix cross product order | `32ab876` | Outward-facing normals for lighting |
| SV_PrimitiveID for normals | `8122229` | Reliable face normals via primitive index |
| Index buffer winding fix | `d434845` | All 6 faces now visible with correct culling |

---

## 3. Root Cause(s) and Why It Happened

### Primary Root Cause: Matrix Layout Mismatch

- **CPU** writes matrices in row-major order (translation at indices [12..14])
- **HLSL** defaults to column-major interpretation without explicit annotation
- **Result**: Matrix appears transposed to GPU -> translation ends up in wrong place -> extreme vertex positions creating "explosion" effect

### DX12 Binding Chain (for reference)

```
TransformBuffer (D3D12_HEAP_TYPE_DEFAULT)
  -> SRV descriptor (StructuredBuffer<float4x4>, stride=64)
  -> Descriptor heap slot (per-frame slice to avoid stomp)
  -> GPU handle from heap start + offset
  -> SetGraphicsRootDescriptorTable(RP_TransformsTable, handle)
  -> Shader reads t0: Transforms[SV_InstanceID]
```

### Secondary Issues Discovered

1. **Per-frame barrier state tracking** - Global `s_firstFrame` flag didn't account for triple-buffering; each frame context needs its own state
2. **Cross product order** - `normalize(cross(ddy, ddx))` had reversed arguments, normals pointed inward
3. **ddx/ddy unreliable** - GPU computes derivatives per 2x2 quad, averages across face boundaries on small triangles

### Hypotheses Ruled Out

| Hypothesis | Evidence Against |
|------------|------------------|
| SRV format/stride wrong | Microtest B showed data readable as color sentinel |
| Descriptor heap binding wrong | Microtest A isolated issue to SRV interpretation, not binding |
| ViewProj matrix wrong | Issue persisted after switching to orthographic camera |

---

## 4. Fix Summary

| File | Commit | Change | Why It Works |
|------|--------|--------|--------------|
| `ShaderLibrary.cpp` | `5cac813` | Add `row_major` to ViewProj and Transforms matrices | CPU row-major layout matches HLSL interpretation |
| `Dx12Context.cpp`, `FrameContextRing.h` | `4ed54f0` | Per-frame `transformsState` field | Each frame context tracks its own barrier state independently |
| `ShaderLibrary.cpp` | `8122229` | Use `SV_PrimitiveID` for face index | `faceIndex = (primID % 12) / 2` gives deterministic normals |
| `Dx12Context.cpp`, `RenderScene.cpp`, `ShaderLibrary.cpp` | `d434845` | Fix index buffer winding order | All 6 cube faces use CW winding when viewed from outside |

---

## 5. Proof / Verification

### Debug Logs Confirming Fix
- Descriptor heap GPU base address logged at bind time
- Per-frame SRV slot allocation verified (no descriptor stomp across in-flight frames)
- Barrier state transitions logged per-frame showing correct COPY_DEST -> VERTEX_AND_CONSTANT_BUFFER

### Microtest Results
- **Microtest A** (bypass SRV): Instanced output matched Naive -> confirmed SRV data path was the issue
- **Microtest B** (ByteAddressBuffer): Color sentinel validated data was readable from GPU
- `MICROTEST_MODE` toggle preserved in codebase for future diagnostics

### Visual Verification
- 100x100 cube grid visible with proper 3D perspective
- All 6 faces render with distinct face-based lighting
- Instanced and Naive modes produce identical output
- F1 toggles sentinel mode, number keys switch camera presets

---

## 6. Takeaways / Invariants

### Invariants for Future DX12 Work

1. **Matrix layout matching**: CPU row-major requires explicit `row_major` annotation in HLSL
2. **Per-frame state tracking**: Each FrameContext owns its resource states (never use global flags for triple-buffered resources)
3. **Fence-gated reuse**: Wait for `CompletedFence >= FrameContext.fenceValue` before touching that frame's resources
4. **Per-frame descriptor slices**: Allocate separate heap regions per in-flight frame to avoid descriptor stomp
5. **GPU derivative limitations**: `ddx`/`ddy` unreliable for small triangles; use `SV_PrimitiveID` for deterministic face indexing
6. **Proof-driven debugging**: Isolated microtests pinpoint root cause faster than shotgun guessing
7. **Index buffer winding**: Consistent CW from outside for all faces; mismatch causes invisible faces
8. **MICROTEST_MODE toggle**: Preserve diagnostic capability in production code paths

### Next Step: Day1.5 Refactor Targets

- Upload arena suballocation (replace per-frame buffer creation)
- Explicit resource state tracker (centralized barrier management)
- GPU timestamp query integration for performance profiling

---

## 7. Link Index (Raw Evidence)

### Plans & Contracts
- [Day1_InstancingVsNaive.md](../../contracts/day1/Day1_InstancingVsNaive.md) - Original contract
- [Day1_InstancingVsNaive_PLAN.md](../../contracts/day1/Day1_InstancingVsNaive_PLAN.md) - Implementation plan
- [Day1_Debug_plan.md](../../contracts/day1/Day1_Debug_plan.md) - Initial debug plan
- [Day1_Debug_plan5-exp1.md](../../contracts/day1/Day1_Debug_plan5-exp1.md) - Microtest A plan

### Issue Packets
- [Day1_Debug_IssuePacket_ExplodingTriangles.md](../../contracts/day1/Day1_Debug_IssuePacket_ExplodingTriangles.md) - Primary symptom documentation
- [Day1_Debug_GroundTruth_VisualSpec.md](../../contracts/day1/Day1_Debug_GroundTruth_VisualSpec.md) - Expected vs actual visuals

### Runtime Logs
- [Day1_Debug_RuntimeLog_RowMajorCompileFail.md](../../contracts/day1/Day1_Debug_RuntimeLog_RowMajorCompileFail.md) - row_major syntax investigation

### Experiments
- [Day1_experiment1.md](../../contracts/day1/Day1_experiment1.md) - Microtest results and conclusions
- [Day1_Debug_explore.md](../../contracts/day1/Day1_Debug_explore.md) - Codebase exploration notes
