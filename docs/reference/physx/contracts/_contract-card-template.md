# PhysX Contract Card Template

Use this template for raw-source-backed PhysX contract cards.
Keep one topic per contract card when possible.

```md
### Contract: <name>

- Reference engine: PhysX
- Reference version/root:
- Trust level: raw-source-verified | context-seeded | agent-inferred | user-approved | superseded | missing-source
- Review status: pending | accepted | rejected | superseded
- Source:
  - `path/to/file.cpp:line`
- Local context:
  - Optional DX12EngineLab class/function that will consume this contract.
- Inputs:
- Outputs:
- Invariant:
- Edge cases:
- Rejection/filtering rules:
- Ordering/tie-break rules:
- Why this matters:
- Possible EngineLab audit question:
```

## Trust Levels

- `raw-source-verified`: raw PhysX source was located and cited with file:line evidence.
- `context-seeded`: a context file suggested the claim, but raw source has not verified it.
- `agent-inferred`: an agent inferred behavior from surrounding code; do not use as final comparison evidence.
- `user-approved`: the user reviewed and accepted the contract for local design use.
- `superseded`: a newer accepted contract replaces this one.
- `missing-source`: source could not be found; record searched roots and search terms.

## Review Status

- `pending`: collected but not accepted as local design authority.
- `accepted`: accepted for comparison in audits.
- `rejected`: reviewed and not accepted.
- `superseded`: replaced by a newer contract card.
