# SSOT (Single Source of Truth) Documentation

This directory contains authoritative documentation for the Scene data model.

## Documents

| Document | Purpose |
|----------|---------|
| [Day4_P0_MapPolicy.md](Day4_P0_MapPolicy.md) | Layering, key, ordering, and conflict policies |
| [Day4_P0_DataModel.md](Day4_P0_DataModel.md) | Type schemas and structures |
| [Day4_P0_LoaderGrammar.md](Day4_P0_LoaderGrammar.md) | Loader grammar (placeholder for PR#2) |
| [ContractChangeLog.md](ContractChangeLog.md) | Append-only change log |
| [ProofTemplate.md](ProofTemplate.md) | PR proof template |

## Principles

1. **No Global Constants**: Grid dimensions live in `GridPrimitive`, not at namespace level
2. **Derive from Source**: Self-tests derive values from `base.GetGrid()->sizeX/sizeZ`
3. **Conflict Policy**: Duplicate keys are REJECTED (TryAdd returns false)
4. **Composition over Union**: `StaticObject` uses safe composition to avoid UB

## Directory Structure

```
assets/scenes/
├── base/       # Base scene files (PR#2+)
└── overlay/    # Overlay scene files (PR#2+)
```
