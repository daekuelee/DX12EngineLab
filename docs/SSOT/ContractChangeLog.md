# Contract Change Log

Append-only log of SSOT contract changes.

---

## 2026-01-30: Day4 P0 PR#1 - Initial Type Definitions

**Added:**
- `CellKey` with linear index formula `idx = iz * gridSizeX + ix`
- `GridPrimitive` with SSOT defaults (100x100, spacing=2.0)
- `FloorPrimitive`, `KillZonePrimitive`
- `StaticObject` using composition (not union)
- `OverlayOps` with REJECT conflict policy
- `BaseSceneSource` with accessors
- `RenderView`, `CollisionView` placeholder types
- Contract self-test (`RunContractSelfTest`)

**Design Decisions:**
- No global SSOT constants at namespace level
- CellKey methods take `gridSizeX` parameter
- StaticObject uses composition for UB safety
- Single `TryAdd()` API (no test variant)

**Deferred:**
- `CellKey::FromWorld()` (PR#3)
- Resolve function (PR#3)
- Consumer switching (PR#4-5)

---

## 2026-01-30: Day4 P0 PR#2 - Scene Loader + Grammar

**Added:**
- `SceneIO.h/cpp` with `LoadBaseSceneFromFile` and `LoadOverlayOpsFromFile` APIs
- `LoadStatus` enum (OK, FILE_NOT_FOUND, PARSE_ERROR)
- `LoadResult` struct with error details and line number
- `OverlayOpPayload` struct (topYAbs, presetId)
- Asset files: `base/default.txt`, `overlay/empty.txt`, `overlay/fixtures_test.txt`
- Loader Contract section in self-test

**Modified:**
- `OverlayOpType` enum: Add/Remove/Modify -> Disable/ModifyTopY/ReplacePreset
- `OverlayOp`: Added `payload` and `sourceLine` fields
- `TryAdd()`: Now logs BOTH first and second sources on duplicate

**Grammar:**
- Base: GRID (7 params), FLOOR (3 params), KILLZONE (1 param)
- Overlay: DISABLE, MODIFY_TOP_Y, REPLACE_PRESET (all cell-keyed with tag)
- Preset mapping: T1=1, T2=2, T3=3

**Deferred:**
- Resolve(base, overlay) -> ResolvedViews (PR#3)
- Consumer switching (PR#4-5)
