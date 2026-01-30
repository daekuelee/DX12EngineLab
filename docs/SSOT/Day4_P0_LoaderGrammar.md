# Day4 P0 Loader Grammar (SSOT)

Authoritative grammar for scene file parsing.

---

## Base Scene Grammar

Base scene files define the world primitives. Each keyword must appear **exactly once**.

### Keywords

| Keyword | Parameters | Description |
|---------|------------|-------------|
| `GRID` | `sizeX sizeZ spacing originX originZ renderHalfExtent collisionHalfExtent` | Grid dimensions and extents |
| `FLOOR` | `posY halfExtentX halfExtentZ` | Floor plane |
| `KILLZONE` | `posY` | Kill zone Y threshold |

### Example: `assets/scenes/base/default.txt`

```
# Day4 P0 Default Base Scene
GRID 100 100 2.0 -100.0 -100.0 1.0 1.0
FLOOR 0.0 100.0 100.0
KILLZONE -50.0
```

### Validation Rules

1. All three keywords (GRID, FLOOR, KILLZONE) must be present
2. Each keyword can only appear once (duplicate = PARSE_ERROR)
3. Comments start with `#`
4. Empty lines are ignored

---

## Overlay Grammar

Overlay files define cell-keyed operations. Each operation targets a specific grid cell.

### Keywords

| Keyword | Parameters | Description |
|---------|------------|-------------|
| `DISABLE` | `ix iz tag` | Disable cell (hole/removed) |
| `MODIFY_TOP_Y` | `ix iz topY tag` | Set absolute top Y position |
| `REPLACE_PRESET` | `ix iz preset tag` | Replace with preset (T1/T2/T3) |

### Payload Details

- `DISABLE`: No payload (cell is removed)
- `MODIFY_TOP_Y`: `topYAbs` = absolute Y position (float)
- `REPLACE_PRESET`: `presetId` = T1=1, T2=2, T3=3

### Example: `assets/scenes/overlay/fixtures_test.txt`

```
DISABLE 10 20 test_hole
MODIFY_TOP_Y 30 40 5.0 raised
REPLACE_PRESET 52 54 T2 special
```

### Validation Rules

1. Cell indices must be within grid bounds (0 <= ix < sizeX, 0 <= iz < sizeZ)
2. Duplicate cell keys are **REJECTED** (first wins, debug break in Debug builds)
3. Preset names must be T1, T2, or T3
4. `tag` is required (for debugging/tracing)
5. Comments start with `#`
6. Empty lines are ignored

---

## Error Handling

### LoadStatus Codes

| Status | Description |
|--------|-------------|
| `OK` | File loaded successfully |
| `FILE_NOT_FOUND` | File does not exist at path or fallback |
| `PARSE_ERROR` | Syntax error, missing required field, or invalid value |

### Path Resolution

1. Try path as-is
2. If not found, try exe-relative `../../` fallback (for running from `x64/Debug/`)

### Error Reporting

`LoadResult` includes:
- `status`: LoadStatus code
- `errorMessage`: Human-readable error description
- `errorLine`: Line number where error occurred (0 if not line-specific)

---

## File Locations

| Type | Path Pattern |
|------|--------------|
| Base scenes | `assets/scenes/base/*.txt` |
| Overlay files | `assets/scenes/overlay/*.txt` |

---

## Grammar EBNF Summary

```ebnf
base_file     = { comment | grid_line | floor_line | killzone_line } ;
overlay_file  = { comment | disable_line | modify_line | preset_line } ;

comment       = "#" { any_char } newline ;
grid_line     = "GRID" uint uint float float float float float newline ;
floor_line    = "FLOOR" float float float newline ;
killzone_line = "KILLZONE" float newline ;

disable_line  = "DISABLE" uint uint tag newline ;
modify_line   = "MODIFY_TOP_Y" uint uint float tag newline ;
preset_line   = "REPLACE_PRESET" uint uint preset tag newline ;

preset        = "T1" | "T2" | "T3" ;
tag           = identifier ;
```
