# PR Proof Template

Use this template for Day4 PR proofs.

---

## Build Proof

```bash
msbuild DX12EngineLab.sln /m /p:Configuration=Debug /p:Platform=x64
msbuild DX12EngineLab.sln /m /p:Configuration=Release /p:Platform=x64
```

- [ ] Debug build: 0 errors, 0 warnings (or existing warnings only)
- [ ] Release build: 0 errors, 0 warnings

## Runtime Proof

### Debug Run
- [ ] Self-test logs appear in Output window
- [ ] Logs match expected format:
  ```
  [SCENE_CONTRACT] === Contract Self-Test START ===
  [SCENE_CONTRACT] Grid from base: sizeX=100 sizeZ=100 totalCells=10000
  [SCENE_CONTRACT] Ordering: idx = iz * gridSizeX + ix verified OK
  [SCENE_CONTRACT] Round-trip idx->CellKey->idx verified OK
  [SCENE_CONTRACT] Size invariant: RenderView/CollisionView size == 10000 verified OK
  [SCENE_CONTRACT] Conflict policy: duplicate REJECT verified OK
  [SCENE_CONTRACT] Base primitives: Grid+Floor+KillZone present OK
  [SCENE_CONTRACT] === Contract Self-Test PASS ===
  ```
- [ ] Ends with `=== Contract Self-Test PASS ===`

### Release Run
- [ ] No self-test logs (Debug-only guard works)

### D3D12 Validation
- [ ] Debug layer: 0 errors on happy path

## Visual Proof

- [ ] Cube grid renders normally (no change)
- [ ] HUD displays normally (no change)
- [ ] Screenshot captured: `captures/day4_p0_prN_normal.png`

## Code Quality

- [ ] No global SSOT constants at namespace level
- [ ] CellKey methods take `gridSizeX` parameter
- [ ] StaticObject uses composition (kind + members)
- [ ] OverlayOps::TryAdd() single API
- [ ] RunContractSelfTest has static-once guard
- [ ] Self-test derives grid params from CreateDefaultBaseScene()
