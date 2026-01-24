# Day1 Implementation Prompt

## References (read before implementing)
1. `docs/contracts/Day1_InstancingVsNaive.md` — Contract (source of truth)
2. `docs/contracts/Day1_InstancingVsNaive_PLAN.md` — Approved plan
3. `docs/notes/Day1_Explore_Summary.md` — Repo reality
4. `ex.cpp` — Reference patterns (NOT compiled)

## Guardrails

1. **Do not touch .sln/.vcxproj** unless a slice explicitly requires it; propose patch first.
2. **Keep diffs small** and build green per slice.
3. **No large paste of ex.cpp** — port behavior carefully, one pattern at a time.
4. **Address known discrepancies D1-D4** as resolved in the plan:
   - D1: FrameCount = 3
   - D2: DATA_STATIC_WHILE_SET_AT_EXECUTE
   - D3: RenderScene owns m_firstFrame flag (single owner)
   - D4: IA/RS/OM set in Tick, not RecordDraw
5. **First slice (S0)** must fix Release|x64 libs if not already done.
6. **FrameId vs BackBufferIndex**: Use monotonic `m_frameId++` for frame resources, `GetCurrentBackBufferIndex()` for RTV only. Never conflate.

## Slice Execution Order

Execute slices S0 through S7 in order. After each slice:
- Verify Debug|x64 builds and links
- Verify Release|x64 builds and links
- Run exe and check acceptance criteria
- Check for validation errors in debug output

## Key Patterns from ex.cpp

- **Fence-gated reuse**: `WaitForFence(fc.fenceValue)` before resetting allocator
- **Per-frame SRV slot**: `fc.srvSlot = frameIndex` (not backbuffer index)
- **Root param enum**: `RP_FrameCB = 0`, `RP_TransformsTable = 1`
- **Embedded HLSL**: `D3DCompile()` with strings, not .hlsl files
- **Barrier pattern**: SRV → COPY_DEST → Copy → COPY_DEST → SRV (track initial state)

## Acceptance Checklist

- [ ] S0: Both configs build, app launches, window clears
- [ ] S1: FrameContextRing works, frameId cycles 0,1,2
- [ ] S2: PSO created, no compile errors
- [ ] S3: Single cube visible
- [ ] S4: 10k grid visible, sentinel works
- [ ] S5: Toggle switches mode, log shows draws count
- [ ] S6: CPU timing in log output
- [ ] S7: Proof toggles cause expected failures
