# PhysX Reference AGENTS.md

## Scope

This directory stores reusable PhysX evidence for DX12EngineLab.
It is an evidence cache, not automatic design authority.

Contract cards become design authority only when a local context, audit, or contract document explicitly accepts them.
Study maps under `study/` are learning aids and do not become design authority by themselves.

## Required Evidence

Every behavioral contract must be backed by one of:

- raw checked-out PhysX source with file:line evidence
- a reviewed contract card that already cites raw source

Do not use these as final evidence:

- `context.txt`
- `PhysXFork/*.context.md`
- generated summaries
- prior agent notes
- memory of PhysX behavior

## Source Discovery

Keep source discovery in `source-discovery.md`.
Each entry should record:

- date
- engine
- version or root
- searched roots
- search terms
- located files
- missing files
- notes

If a source is missing, document the missing source instead of filling the gap by inference.

## Contract Cards

Write contract cards under `contracts/`.
Use `contracts/_contract-card-template.md` when available.

Each card must include:

- Reference engine
- Reference version/root
- Trust level: `raw-source-verified`, `context-seeded`, `agent-inferred`, `user-approved`, `superseded`, or `missing-source`
- Review status: `pending`, `accepted`, `rejected`, or `superseded`
- Source file:line evidence
- Inputs
- Outputs
- Invariant
- Edge cases
- Rejection/filtering rules
- Ordering/tie-break rules
- Why this matters
- Possible EngineLab audit question

## Separation From EngineLab Audit

Do not inspect or patch EngineLab production source while writing PhysX contract cards unless the user explicitly changes the scope.
Use local EngineLab symbol names only as optional context supplied by the user or an audit report.

The next session after contract mining may audit EngineLab SceneQuery against these cards.

## Study Maps

Write study maps under `study/`.
Use `study/_study-map-template.md` when available.

Study maps should explain:

- raw source reading order
- call path
- data/result flow
- key types and functions
- linked contract cards
- EngineLab comparison questions
- next reading path

Do not cite study maps as final audit evidence unless they point back to raw source file:line evidence or an accepted contract card.
