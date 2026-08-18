# SceneQuery Invariant Audit: Filter / Hit Selection

Updated: 2026-05-10

## 1. Scope

This audit covers filter ownership, closest-hit selection, and stage-specific
policy leakage:

- `Engine/Collision/SceneQuery/SqTypes.h`
- `Engine/Collision/SceneQuery/SqNarrowphaseLegacy.h`
- `Engine/Collision/SceneQuery/SqQueryLegacy.h`

Reference contracts:

- `docs/reference/physx/contracts/query-filtering.md`
- `docs/reference/physx/contracts/scenequery-pipeline.md`

## 2. Query Result Contract

| API/Function | Current policy | Evidence | Ambiguity | Required invariant |
|---|---|---|---|---|
| `SweepFilter` | Dot predicate over hit normal and `refDir`; comments name `StepUp`, `StepMove`, `StepDown`. | `SqTypes.h:147-163` | KCC movement-stage policy is documented inside SceneQuery type. | SceneQuery filter should describe query mechanics, not movement semantics. |
| `PassNarrowfilter` | Applies initial-overlap rejection and dot predicate inside primitive helpers. | `SqNarrowphaseLegacy.h:63-80` | Raw rejected hits are lost before collector/query diagnostics. | Primitive narrowphase should be able to produce raw hits without stage policy. |
| `SweepCapsuleClosestHit_Fast` | Applies filter again after narrowphase and before `BetterHit`. | `SqQueryLegacy.h:172-189` | Double filtering can hide whether rejection happened in primitive or query layer. | Filtering should have one owner. |
| `BetterHit` | Chooses closest `t`, then feature class, type, index, feature id. | `SqQueryLegacy.h:59-72`, `SqQueryLegacy.h:193-205` | Good deterministic closest-block order, but not all-hits/touch-buffer semantics. | Equal-time closest selection must remain total and documented. |

## 3. Hit Normal and TOI Semantics

Current filter uses normal direction. That means hit selection depends on
normal semantics from narrowphase. If the normal is a proxy/extrusion normal,
filtering may reject or accept a candidate differently than source-surface
filtering would.

Evidence:

- `SweepFilter` is a normal dot predicate: `SqTypes.h:158-163`.
- Narrowphase can return proxy/extruded normals: `SqNarrowphaseLegacy.h:468-482`,
  `SqNarrowphaseLegacy.h:665-680`.

## 4. Broadphase / Narrowphase Boundary

PhysX separates data filter, prefilter, exact test, postfilter, block/touch
classification, any-hit early-out, and no-block overlap behavior at the query
boundary. Reference: `query-filtering.md:21-33`, `query-filtering.md:37-45`.

Current EngineLab has one closest-hit query and one normal-dot filter. It is
not wrong for a minimal engine, but it cannot express these separate concepts:

- raw all hits
- any hit
- touch versus block
- walkable-only collector
- approach-normal filtered closest hit
- post-hit KCC rejection with access to discarded raw hit stream

## 5. Policy Leakage

### Issue: `SweepFilter` encodes KCC stage semantics

- Severity: P1
- EngineLab evidence: `SweepFilter` comments explicitly name `StepUp`,
  `StepMove`, and `StepDown` usage (`SqTypes.h:151-154`).
- Reference evidence: PhysX query filtering is callback/filter-data/query-boundary
  policy, not primitive geometry ownership (`query-filtering.md:29-33`,
  `query-filtering.md:42-45`).
- Current behavior: SceneQuery looks like it owns KCC movement-stage filtering.
- Why it matters: KCC refactor can become blocked by SceneQuery API semantics
  that were designed around legacy StepUp/StepMove/StepDown behavior.
- Minimal test: A raw sweep collector should be able to report the first raw hit
  and the final stage-accepted hit separately.

### Issue: Filter is applied twice

- Severity: P1
- EngineLab evidence: `SweepCapsulePrim_TOI01` passes `filter` into primitive
  functions (`SqQueryLegacy.h:175-179`), and the query loop applies
  `Dot(n, filter.refDir) < filter.minDot` again (`SqQueryLegacy.h:182-189`).
- Reference evidence: PhysX has distinct prefilter and postfilter phases;
  ownership is explicit at query boundary (`query-filtering.md:29-33`).
- Current behavior: A hit can be rejected in primitive narrowphase before the
  query-level filter sees it.
- Why it matters: Debugging closest-hit problems becomes opaque; KCC cannot know
  whether a better raw hit existed but was rejected before collection.
- Minimal test: Instrument a sweep with two hits: closest raw hit rejected by
  normal predicate and later accepted hit. Probe must record first raw hit and
  accepted hit separately.

### Issue: closest-only result cannot answer all KCC stage questions

- Severity: P1
- EngineLab evidence: `SweepCapsuleClosestHit_Fast` maintains a single `best`
  hit (`SqQueryLegacy.h:134-148`, `SqQueryLegacy.h:193-205`). There is no
  all-hits collector or touch buffer.
- Reference evidence: PhysX distinguishes zero-touch-buffer closest block,
  touch hits, any-hit, and block-distance shrinking (`query-filtering.md:25-33`,
  `query-filtering.md:39-41`).
- Current behavior: KCC must encode stage-specific policy through a normal-dot
  closest filter.
- Why it matters: StepDown may need walkable ground, StepMove may need
  approach-blocking wall, and recovery may need all overlapping contacts. One
  closest filtered hit is not a complete policy surface.
- Minimal test: Scene with closer wall and slightly farther walkable ground;
  StepDown-style query should prove whether closest-only can recover walkable
  support.

## 6. Determinism Risks

`BetterHit` itself is deterministic. Evidence: `SqQueryLegacy.h:59-72`.

The determinism risk is upstream: if two raw hits exist but one is filtered in
narrowphase before the query loop can classify it, diagnostics cannot reproduce
the full hit stream. This is a visibility/ownership problem rather than a
non-deterministic sort bug.

## 7. Minimal Fix Direction

Do not implement from this audit alone.

Smallest next steps:

1. Introduce a probe-only raw sweep collector before changing behavior.
2. Record `firstRawHit`, `firstRejectedHit`, and `acceptedClosestHit` for KCC
   trace comparison.
3. Move toward one filter owner: preferably query/collector boundary, not
   primitive geometry helpers.
4. Do not implement all-hits as part of KCC StepUp/StepDown patching; first
   stabilize the result contract and probes.
