# PhysX Study Map: query-filtering

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx` public headers, `.ref/PhysX_3.4/PhysX_3.4` SceneQuery implementation
- Related contract cards:
  - `docs/reference/physx/contracts/query-filtering.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to study where scene-query filtering sits relative to geometry tests. Filtering decides whether a candidate reaches exact geometry testing, how a hit is classified as block/touch/none, and whether traversal stops early.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx`
- `.ref/PhysX_4.1/physx`
- `.ref/PhysX_3.4/PhysX_3.4`
- `.ref/PhysX34/PhysX_3.4`

## Reading Order

1. `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h:57` - query flags.
2. `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h:88` - `eNONE`, `eTOUCH`, `eBLOCK` semantics.
3. `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h:118` - `PxQueryFilterData` filtering order.
4. `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h:147` - `PxQueryFilterCallback` pre/post contract.
5. `.ref/PhysX_4.0/physx/include/PxQueryReport.h:231` - block and touch reporting buffers.
6. `.ref/PhysX_4.0/physx/include/PxScene.h:1300` - raycast/sweep/overlap public query notes.
7. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpQueryShared.h:54` - filter-data bitwise equation.
8. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:152` - prefilter implementation.
9. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:360` - default block/touch selection.
10. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:460` - postfilter, any-hit, no-block, touch/block storage.
11. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:682` - query validation and initial shrunk-distance state.

## Call Path

```text
PxScene::raycast/sweep/overlap(...)
  -> NpSceneQueries::multiQuery(...)
  -> broadphase/pruner candidate
  -> filterData bitwise equation
  -> optional preFilter before exact test
  -> exact geometry test
  -> optional postFilter after exact hit
  -> eANY_HIT / eNO_BLOCK / eTOUCH / eBLOCK reporting
```

## Data / Result Flow

- `PxQueryFilterData.data` is compared against shape query filter data before callbacks.
- `preFilter` can suppress the shape or choose provisional touch/block before exact testing.
- Exact geometry tests produce raycast/sweep/overlap hit data.
- `postFilter` can override the previous hit type after exact testing.
- `eANY_HIT` stores any non-none hit as `block` and stops traversal.
- `eNO_BLOCK` converts hits to touch, mainly avoiding overlap block ambiguity.
- Touches are clipped to the current nearest block distance.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `PxQueryFlag` | `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h:57` | Query traversal and filtering flags. |
| `PxQueryHitType` | `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h:108` | `eNONE`, `eTOUCH`, `eBLOCK` classification. |
| `PxQueryFilterData` | `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h:132` | Query filter words plus flags. |
| `PxQueryFilterCallback` | `.ref/PhysX_4.0/physx/include/PxQueryFiltering.h:169` | User pre/post filter API. |
| `PxHitCallback` | `.ref/PhysX_4.0/physx/include/PxQueryReport.h:231` | Block/touch result storage. |
| `applyFilterEquation` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpQueryShared.h:54` | Bitwise filter-data rejection. |
| `applyAllPreFiltersSQ` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:152` | Runtime prefilter path. |
| `MultiQueryCallback::invoke` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysX/src/NpSceneQueries.cpp:382` | Per-candidate filter and hit classification. |

## Contracts Produced

- `docs/reference/physx/contracts/query-filtering.md`

## Terms To Remember

- `filter equation`: bitwise AND between query and shape filter data.
- `preFilter`: before exact geometry test.
- `postFilter`: after exact geometry hit.
- `eNONE`: skip candidate.
- `eTOUCH`: report non-blocking touch if a touch buffer can receive it.
- `eBLOCK`: nearest blocking hit.
- `eANY_HIT`: early-out any non-none hit as block.
- `eNO_BLOCK`: convert blocks to touches.

## EngineLab Comparison Questions

- Does the local query layer run filter-data rejection before primitive tests?
- Can prefilter change only allowed hit flags per shape?
- Does postfilter override prefilter/default classification?
- Does any-hit stop on both touch and block?
- Are overlap blocks converted or warned according to no-block policy?
- Are touch hits clipped to the nearest block distance?

## Next Reading Path

- `scenequery-pipeline`: how public scene queries reach broadphase/pruner and geometry tests.
- `mesh-sweeps-ordering`: mesh candidate result selection below filtering.
- `geometry-query-api`: raw geometry query API below scene filtering.
