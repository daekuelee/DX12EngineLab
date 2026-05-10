# PhysX Study Map: cct-query-recovery-mechanics

- Reference engine: PhysX
- Reference version/root: `.ref/PhysX_4.0/physx` CCT public headers, `.ref/PhysX_3.4/PhysX_3.4` CharacterKinematic implementation
- Related contract cards:
  - `docs/reference/physx/contracts/cct-query-recovery-mechanics.md`
- Trust note: Study maps are navigation aids, not accepted design authority.

## Purpose

Use this map to study how PhysX CCT movement is assembled from SceneQuery overlap candidate gathering, local swept-volume tests, up/side/down movement passes, and optional MTD overlap recovery.

## Raw Source Roots Checked

- `.ref/PhysX_4.0/physx/include/characterkinematic`
- `.ref/PhysX_3.4/PhysX_3.4/Include/characterkinematic`
- `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src`

## Reading Order

1. `.ref/PhysX_4.0/physx/include/characterkinematic/PxController.h:653` - public `PxController::move` contract and inputs.
2. `.ref/PhysX_4.0/physx/include/characterkinematic/PxController.h:261` - `PxControllerFilters` and overlap-based CCT-vs-world filtering.
3. `.ref/PhysX_4.0/physx/include/characterkinematic/PxControllerManager.h:211` - overlap recovery public knob and documented scope.
4. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:2216` - `Controller::move` setup and writeback.
5. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:1639` - `SweepTest::moveCharacter` up/side/down pass decomposition.
6. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:1346` - per-pass sweep loop.
7. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:1198` - cached candidate geometry update.
8. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterControllerCallbacks.cpp:909` - overlap query into PhysX scene shapes.
9. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:725` - exact sweep result selection.
10. `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:792` - recovery MTD computation.

## Call Path

```text
PxController::move
  -> Controller::move
  -> collect other CCTs and user obstacles
  -> SweepTest::moveCharacter
  -> initial temporal-box candidate update
  -> up pass: doSweepTest
  -> side pass: doSweepTest
  -> optional sensor pass
  -> down pass: doSweepTest
  -> Controller::move writeback

doSweepTest
  -> swept_volume.computeTemporalBox
  -> updateTouchedGeoms
  -> findTouchedGeometry
  -> PxScene::overlap(PxBoxGeometry temporal bounds)
  -> output touched shapes into geom stream
  -> CollideGeoms
  -> gSweepMap exact swept-volume helper
  -> optional computeMTD for initial overlap recovery
```

## Data / Result Flow

- Inputs: displacement, `minDist`, elapsed time, `PxControllerFilters`, optional obstacle context, contact offset, step offset, max jump height, precise sweep flag, and overlap recovery flag.
- Candidate state: temporal AABB, cached bounds, static/dynamic geom stream, world triangles, triangle indices, and user obstacle arrays.
- Sweep state: current position, target position, pass direction, `contactOffset`-expanded closest distance, touched actor, touched shape, and collision flags.
- Recovery state: zero-distance initial-overlap contact, contact-offset-inflated controller geometry, `computePenetration` MTD direction/depth, and corrected center position.
- Outputs: collision flags, updated controller center, optional kinematic actor target pose, touched actor/shape cache, and callbacks for shape, controller, or obstacle hits.

## Key Types And Functions

| Name | Source | Why it matters |
|---|---|---|
| `PxController::move` | `.ref/PhysX_4.0/physx/include/characterkinematic/PxController.h:653` | Public CCT movement boundary. |
| `PxControllerFilters` | `.ref/PhysX_4.0/physx/include/characterkinematic/PxController.h:261` | User filter data/callbacks for CCT world and CCT-vs-CCT filtering. |
| `CharacterControllerManager::setOverlapRecoveryModule` | `.ref/PhysX_4.0/physx/include/characterkinematic/PxControllerManager.h:211` | Public recovery control. |
| `Controller::move` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:2216` | Per-move setup, obstacle collection, CCT filtering, and writeback. |
| `SweepTest::moveCharacter` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:1639` | Up/side/down movement policy. |
| `SweepTest::doSweepTest` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:1346` | Iterative collide-and-slide sweep loop. |
| `updateTouchedGeoms` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:1198` | Cached broadphase candidate gathering. |
| `findTouchedGeometry` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterControllerCallbacks.cpp:909` | Scene overlap query and geom stream output. |
| `CollideGeoms` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:725` | Nearest exact contact selection. |
| `computeMTD` | `.ref/PhysX_3.4/PhysX_3.4/Source/PhysXCharacterKinematic/src/CctCharacterController.cpp:792` | Recovery correction path. |

## Contracts Produced

- `docs/reference/physx/contracts/cct-query-recovery-mechanics.md`

## Terms To Remember

- `contactOffset`: controller skin used in temporal bounds, sweep distance, and MTD recovery shape inflation.
- `geom stream`: CCT-local stream of touched scene shapes and generated mesh triangles.
- `temporal box`: overlap AABB covering current and target controller positions.
- `up/side/down pass`: PhysX CCT movement decomposition used for auto-step and grounding behavior.
- `overlap recovery`: zero-distance initial-overlap correction via MTD pose adjustment.
- `CCT-vs-CCT filter`: callback that controls whether other controllers are inserted as user obstacle volumes.

## EngineLab Comparison Questions

- Does local CCT movement gather candidates with an overlap AABB before exact sweeps?
- Does contact offset affect candidate bounds, sweep distance, and recovery shape inflation consistently?
- Is zero-distance initial overlap handled as pose correction rather than as normal velocity?
- Are CCT proxy shapes and triggers rejected before local swept-volume tests?
- Are other controllers filtered separately from scene shape filtering?
- Is the nearest-contact rule deterministic for the local geom stream order?

## Next Reading Path

- `scenequery-pipeline`: high-level SceneQuery traversal and exact geometry dispatch.
- `query-filtering`: prefilter, postfilter, block, touch, any-hit, and no-block semantics.
- `initial-overlap-mtd`: public `computePenetration` and sweep `eMTD` conventions.
- `sweep-toi-hit-normal`: primitive sweep TOI and normal writeback.
