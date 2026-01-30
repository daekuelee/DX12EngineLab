# Day4 P0 Map Policy

## Layering

The scene system uses a two-layer architecture:

1. **Base Layer** (`BaseSceneSource`): Contains static primitives (Grid, Floor, KillZone)
2. **Overlay Layer** (`OverlayOps`): Contains dynamic modifications (Add, Remove, Modify)

Resolution order: Base → Overlay (overlay operations apply on top of base)

## Key Policy

### CellKey Structure
```cpp
struct CellKey {
    uint16_t ix;  // X index [0, sizeX-1]
    uint16_t iz;  // Z index [0, sizeZ-1]
};
```

### Linear Index Formula
```
idx = iz * gridSizeX + ix
```

**Important**: `gridSizeX` is passed as a parameter, NOT read from a global constant.

### Range
- `ix`: [0, gridSizeX - 1]
- `iz`: [0, gridSizeZ - 1]
- `idx`: [0, totalCells - 1]

## Ordering Policy

Cells are ordered by linear index:
- Row-major order (X varies fastest)
- Example for 100x100 grid:
  - `{0, 0}` → 0
  - `{52, 54}` → 5452
  - `{99, 99}` → 9999

## Conflict Policy

### OverlayOps::TryAdd()

| Condition | Result | Side Effect (Debug) |
|-----------|--------|---------------------|
| Key not present | Returns `true` | None |
| Key already exists | Returns `false` | Log + DebugBreak (if `s_enableDebugBreak`) |

### Self-Test Mode

During self-tests, `ScopedDisableDebugBreak` RAII guard temporarily disables debug break and logging to allow testing the rejection behavior without interruption.

```cpp
{
    ScopedDisableDebugBreak guard;
    // TryAdd() will return false on duplicate but NOT break
}
// After scope: s_enableDebugBreak restored
```

## Validation

Self-test (`Scene::RunContractSelfTest()`) verifies:
1. Linear index calculation matches formula
2. Round-trip: `idx → CellKey → idx` preserves value
3. Size invariant: views resize to `totalCells`
4. Conflict rejection: duplicate TryAdd returns false
5. Base primitives: Grid, Floor, KillZone all present
