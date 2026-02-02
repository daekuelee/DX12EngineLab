# DX12EngineLab Study Pack: until-day3

## Scope

Bridge docs for Day3+ debugging. References `until-day2/` as SSOT (Single Source of Truth).

## Quick Reference Files

| File | Purpose | Prereq |
|------|---------|--------|
| [01_frame_lifetime_quick_ref.md](01_frame_lifetime_quick_ref.md) | Frame sync checklist | [D_resource_lifetime.md](../until-day2/D_resource_lifetime.md) |
| [02_binding_abi_quick_ref.md](02_binding_abi_quick_ref.md) | Binding checklist | [C_binding_abi.md](../until-day2/C_binding_abi.md) |
| [03_fixedstep_pipeline_deep.md](03_fixedstep_pipeline_deep.md) | Fixed-step simulation contracts | None |
| [04_capsule_collision_deep.md](04_capsule_collision_deep.md) | Capsule collision contracts | [03_fixedstep_pipeline_deep.md](03_fixedstep_pipeline_deep.md) |

## SSOT (Do NOT Duplicate)

These files contain the authoritative deep details:

- **Frame ring architecture**: [until-day2/D_resource_lifetime.md](../until-day2/D_resource_lifetime.md)
- **Root signature formal model**: [until-day2/C_binding_abi.md](../until-day2/C_binding_abi.md)

## Failure Linkage

See [04_day3_failure_cookbook.md](../../onboarding/pass/04_day3_failure_cookbook.md) for symptom→suspect mapping.

## How to Use

1. **Read SSOT first** if unfamiliar with the architecture
2. **Use quick refs** as debugging checklists during Day3+ work
3. **Follow failure linkage** when symptoms match cookbook entries
4. **Run proof steps** to verify contracts hold
