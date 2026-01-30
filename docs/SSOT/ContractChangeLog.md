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
- `CellKey::FromWorld()` (PR#2)
- Loader implementation (PR#2)
- Resolve function (PR#3)
- Consumer switching (PR#4-5)
