# PhysX Study Map: scenequery-pipeline

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx` public headers, `.ref/PhysX_3.4/PhysX_3.4` SceneQuery implementation
- Related contract cards:
  - `docs/reference/physx/contracts/scenequery-pipeline.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to study the high-level SceneQuery path. The important split is public API -> shared `multiQuery` -> static/dynamic pruner traversal -> exact geometry dispatch -> block/touch reporting.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`
- `.ref/PhysX_3.4/PhysX_3.4`

## Reading Order

1. `.ref/PhysX_4.0/physx/include/PxScene.h:1319` - public raycast signature.
2. `.ref/PhysX_4.0/physx/include/PxScene.h:1352` - public sweep signature.
3. `.ref/PhysX_4.0/physx/include/PxScene.h:1377` - public overlap signature.
4. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:80` - raycast entry into `multiQuery`.
5. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:108` - sweep flag sanitation and `multiQuery`.
6. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:681` - shared validation and `shrunkDistance` setup.
7. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:740` - cache/static/dynamic pruner traversal.
8. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:189` - exact geometry dispatch.
9. `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/include/SqPruner.h:75` - pruner callback boundary.

## Call Path

```text
PxScene::raycast/sweep/overlap
  -> NpSceneQueries::raycast/sweep/overlap
  -> MultiQueryInput
  -> NpSceneQueries::multiQuery
  -> optional single-shape cache
  -> static pruner if eSTATIC
  -> dynamic pruner if eDYNAMIC
  -> MultiQueryCallback::invoke(candidate)
  -> filter + GeomQueryAny::geomHit
  -> touch/block/any-hit reporting
```

## Data / Result Flow

- Public API parameters are normalized into `MultiQueryInput`.
- `SceneQueryManager` owns static and dynamic pruners.
- Pruners emit payloads into `PrunerCallback::invoke`.
- `GeomQueryAny` calls raycast, sweep, or overlap geometry helpers.
- Result classification updates touches, block, and shrunk distance.
- Query returns `hasAnyHits()`.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `PxScene::raycast/sweep/overlap` | `.ref/PhysX_4.0/physx/include/PxScene.h:1319` | Public API boundary. |
| `NpSceneQueries::multiQuery` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:681` | Shared query pipeline. |
| `MultiQueryInput` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.h:67` | Shared ray/sweep/overlap input carrier. |
| `SceneQueryManager` | `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/include/SqSceneQueryManager.h:125` | Static/dynamic pruner owner. |
| `PrunerCallback::invoke` | `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/include/SqPruner.h:75` | Candidate callback boundary. |
| `GeomQueryAny::geomHit` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:189` | Exact raycast/sweep/overlap dispatch. |

## Contracts Produced

- `docs/reference/physx/contracts/scenequery-pipeline.md`

## Terms To Remember

- `multiQuery`: shared raycast/sweep/overlap pipeline.
- `pruner`: broadphase acceleration structure.
- `payload`: actor/shape identity emitted by the pruner.
- `shrunkDistance`: nearest-block/candidate distance bound.
- `GeomQueryAny`: exact query dispatch wrapper.
- `cache`: optional single-shape cache used before pruner traversal.

## EngineLab Comparison Questions

- Does local SceneQuery keep static and dynamic traversal as candidate generation only?
- Does exact geometry dispatch happen after filtering?
- Is `shrunkDistance` owned by query result classification, not primitive helpers?
- Does sweep normalize initial-overlap normal at the SceneQuery boundary?
- Are cache hits treated as blocking and isolated from regular filtering?

## Next Reading Path

- `query-filtering`: filter data, prefilter, postfilter, any-hit, no-block.
- `geometry-query-api`: raw geometry API under SceneQuery.
- `mesh-sweeps-ordering`: mesh candidate and callback behavior under exact dispatch.
