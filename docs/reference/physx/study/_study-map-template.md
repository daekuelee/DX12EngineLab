# PhysX Study Map: <topic>

- Reference engine: PhysX
- Reference version/root:
- Related contract cards:
  - `docs/reference/physx/contracts/<topic>.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Explain what this topic teaches and why it matters for later DX12EngineLab audits.

## Raw Source Roots Checked

- `<root>`

## Reading Order

1. `<path>:<line>` - entry point or public API.
2. `<path>:<line>` - delegated implementation.
3. `<path>:<line>` - edge case or policy branch.

## Call Path

```text
<public API>
  -> <implementation function>
  -> <primitive or policy helper>
```

## Data / Result Flow

- Inputs:
- Intermediate state:
- Outputs:
- Failure or rejection path:

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `<type/function>` | `<path>:<line>` | `<short note>` |

## Contracts Produced

- `docs/reference/physx/contracts/<topic>.md`

## Terms To Remember

- `<term>`: `<short meaning>`

## EngineLab Comparison Questions

- Which EngineLab result field should be compared against this PhysX behavior?
- Which layer should own this policy in EngineLab?
- What deterministic test or probe would expose a mismatch?

## Next Reading Path

- `<related topic or file>`
