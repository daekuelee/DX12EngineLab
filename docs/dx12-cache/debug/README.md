# Debug Index

Compact symptom lookup with links to detailed day packets.

---

## Quick Symptom Lookup

| Symptom | First Check | Day Packet |
|---------|-------------|------------|
| Exploding triangles | `row_major` declaration, `mul(v,M)` order | [Day1 IP-ExplodingTriangles](../../contracts/day1/Day1_Debug_IssuePacket_ExplodingTriangles.md) |
| Only top face renders | Index buffer winding order | [Day1 Debug Plans](../../contracts/day1/) |
| Cube shifted by floor offset | Floor VS reading Transforms[iid] | [Day1 VisualSpec](../../contracts/day1/Day1_Debug_GroundTruth_VisualSpec.md) |
| Descriptor stomp / flicker | Per-frame SRV slots, fence gating | See `binding-rules.md` |
| GPU hang / TDR | Root param type mismatch | See `binding-rules.md` |
| nvwgf2umx.dll crash | SetGraphicsRootConstantBufferView on TABLE slot | See `binding-rules.md` |
| Wrong face colors | Index buffer face order vs color array | [Day1 fixcube plans](../../contracts/day1/Day1_Debug_plan13-fixcube.md) |
| Z-fighting floor/cubes | Floor VS should NOT read transforms | Fixed in ShaderLibrary.cpp |
| row_major compile fail | Old HLSL syntax issue | [Day1 RuntimeLog](../../contracts/day1/Day1_Debug_RuntimeLog_RowMajorCompileFail.md) |

---

## Proof Toggle Reference

| Toggle | Purpose | Location |
|--------|---------|----------|
| `MICROTEST_MODE` | ByteAddressBuffer + color sentinel diagnostic | `RenderConfig.h` |

---

## Debug Layer Quick Enable

```cpp
#if defined(_DEBUG)
ComPtr<ID3D12Debug> debugController;
if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
{
    debugController->EnableDebugLayer();

    // Optional: GPU-based validation (slower but catches more)
    ComPtr<ID3D12Debug1> debugController1;
    if (SUCCEEDED(debugController.As(&debugController1)))
    {
        debugController1->SetEnableGPUBasedValidation(TRUE);
    }
}
#endif
```

See `pinned-sources.md > Diagnostics` for Debug Layer, GBV, DRED documentation.

---

## Day Packet Index

| Day | Focus | Key Packets |
|-----|-------|-------------|
| Day1 | Cube rendering, transforms | [Explore](../../contracts/day1/Day1_Debug_explore.md), [VisualSpec](../../contracts/day1/Day1_Debug_GroundTruth_VisualSpec.md), [ExplodingTriangles](../../contracts/day1/Day1_Debug_IssuePacket_ExplodingTriangles.md) |

---

## Adding New Issues

1. **Create detailed packet**: `docs/contracts/dayX/DayX_Debug_IssuePacket_<Name>.md`
   - Symptom description
   - Investigation steps
   - Root cause analysis
   - Fix applied
   - Verification evidence

2. **Add one-line entry**: Update the symptom table above
   - Symptom (brief)
   - First check (what to look at)
   - Link to packet

3. **Update binding-rules.md**: If issue reveals new DX12 rule
