# SceneQuery Audit Index

Updated: 2026-05-10

## Purpose

This file tracks SceneQuery audit progress and prevents repeated context reconstruction.
It is not a production-source change plan.

## Current Scope

The 2026-05-10 pass audits current working-tree SceneQuery code against the
raw-source-backed PhysX contracts under `docs/reference/physx/contracts/`.

Reports:

| Report | Status | Focus |
|---|---|---|
| `01-result-contract-audit.md` | drafted | `Hit`, `OverlapContact`, TOI, initial overlap fields |
| `02-bvh-broadphase-audit.md` | drafted | `StaticBVH`, sweep/overlap traversal, stack, false-negative risks |
| `03-narrowphase-geometry-audit.md` | drafted | capsule-triangle, capsule-box, overlap normal/depth semantics |
| `04-filter-hit-selection-audit.md` | drafted | `SweepFilter`, narrowphase/query filtering, closest-hit policy |
| `05-collisionworld-boundary-audit.md` | drafted | `CollisionWorldLegacy` query boundary, mask/remap/scratch ownership |
| `06-scenequery-audit-summary.md` | drafted | top risks, recommended next probes/fixes |
| `07-bvh4-readiness-audit.md` | drafted | BVH4 readiness, PhysX BV4 contract comparison, metrics/scalar backend sequencing |
| `08-scenequery-backend-harness-plan.md` | drafted | Backend harness contract, fixtures, metrics, PhysX BV4 alignment, future BVH4 plug-in rules |
| `09-scalar-bvh4-core.md` | drafted | Scalar BVH4 backend implementation, harness result, limitations, next session boundary |
| `10-bvh4-physx-gap-roadmap.md` | drafted | High-density EngineLab ScalarBVH4 vs PhysX BV4 gap matrix and multi-session roadmap |
| `11-bvh4-simd-child-test-prototype.md` | drafted | Gated SIMD-shaped BVH4 child AABB rejection prototype, metrics, and claim boundary |
| `12-bvh4-simd-soa-traversal-hardening.md` | drafted | Shared scalar/packet BVH4 traversal helper boundary and packet metric guard |

## Required Reference Contracts

| Contract | Reference source | Status | Notes |
|---|---|---|---|
| PhysX query filtering contract | `docs/reference/physx/contracts/query-filtering.md` | present | Used by `04-filter-hit-selection-audit.md`. |
| PhysX SceneQuery pipeline contract | `docs/reference/physx/contracts/scenequery-pipeline.md` | present | Used by BVH/filter/boundary audits. |
| PhysX sweep hit / TOI contract | `docs/reference/physx/contracts/sweep-toi-hit-normal.md` | present | Used by `01-result-contract-audit.md`. |
| PhysX initial overlap / MTD contract | `docs/reference/physx/contracts/initial-overlap-mtd.md` | present | Used by result/narrowphase audits. |
| PhysX capsule-triangle sweep contract | `docs/reference/physx/contracts/capsule-triangle-sweep.md` | present | Used by `03-narrowphase-geometry-audit.md`. |
| PhysX mesh sweep ordering contract | `docs/reference/physx/contracts/mesh-sweeps-ordering.md` | present | Used to classify BV4 as mesh-midphase reference, not top-level BVH direction. |
| PhysX BV4 layout/traversal contract | `docs/reference/physx/contracts/bv4-layout-traversal.md` | present | Used by `07-bvh4-readiness-audit.md` to classify BV4 as traversal/layout performance structure. |

## Audit Output Rules

- Each audit report must cite EngineLab `file:line` evidence.
- Each reference comparison must cite a reviewed contract card or raw reference `file:line`.
- If reference evidence is missing, mark the item `reference mining required`.
- Do not modify `Engine/Collision` during audit mode.

## Open Questions

- Does every caller treat `Hit.startPenetrating` as the initial-overlap source of truth rather than inferring from `t == 0`?
- Should `penetrationDepth` remain attached to sweep `Hit`, or should MTD/recovery be a separate result path?
- Are box-sweep normals acceptable as proxy/extrusion normals, or does KCC need reconstructed source-face normals?
- Should `SweepFilter` move out of primitive narrowphase and into a query/collector boundary?
- Does empty BVH traversal return immediately for all query types?
- Does `QueryScratch::stack[512]` have measured max usage on representative query batches?
- Can a future BVH4 traversal reuse one backend-independent collector instead of duplicating `BetterHit` and overlap top-k policy?
- Should `RunSceneQueryBackendBenchmark` get a manual ImGui/hotkey trigger, or remain API-only until scalar BVH4 exists?
- Should `CollisionWorldLegacy::queryMask` be implemented for solids or removed from the documented contract?
- Should BVH traversal share a backend-independent collector so overlap top-k and closest-hit retention cannot diverge by backend?
- Should `ScalarBVH4` remain a harness-only backend until collector semantics and benchmark coverage are backend-independent?
