# SceneQuery Invariant Audit: BVH / Broadphase

Updated: 2026-05-10

## 1. Scope

This audit covers static BVH construction, sweep broadphase pruning, overlap
traversal, local stack use, and candidate determinism:

- `Engine/Collision/SceneQuery/SqBVH.h`
- `Engine/Collision/SceneQuery/SqBroadphase.h`
- `Engine/Collision/SceneQuery/SqQueryLegacy.h`
- `Engine/Collision/CollisionWorldLegacy.cpp`

Reference contracts:

- `docs/reference/physx/contracts/scenequery-pipeline.md`
- `docs/reference/physx/contracts/mesh-sweeps-ordering.md`
- raw PhysX traversal stack evidence:
  `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/src/SqAABBTreeQuery.h:177-211`

## 2. Query Result Contract

| Field | Meaning in current code | Evidence | Ambiguity | Required invariant |
|---|---|---|---|---|
| `StaticBVH::nodes` | Binary tree nodes, root index stored separately. | `SqBVH.h:39-57`, `SqBVH.h:179-186` | Empty BVH creates a default root with no explicit empty flag. | Empty BVH must never be traversed as an internal node. |
| `BVHNode::primCount` | Leaf marker when `primCount > 0`. | `SqBVH.h:39-42`, `SqQueryLegacy.h:157-160`, `SqQueryLegacy.h:308-311` | A node with `primCount == 0` can mean internal node or empty root. | Internal/empty/leaf states must be disjoint. |
| `QueryScratch::stack` | Fixed local traversal stack with 512 entries. | `SqQueryLegacy.h:46-51` | No overflow guard or measured max usage. | Stack overflow must be impossible by proof or handled by fallback/assert. |
| `AabbAabb_SweepInterval` | Swept AABB time-window refinement for moving capsule AABB vs static bounds. | `SqBroadphase.h:39-75`, `SqQueryLegacy.h:143-148`, `SqQueryLegacy.h:164-170`, `SqQueryLegacy.h:212-220` | Needs false-negative probes against narrowphase. | Broadphase pruning must be conservative: no narrowphase hit may be pruned. |

## 3. Hit Normal and TOI Semantics

BVH traversal does not create normals. It only restricts candidate nodes and
primitive refs by swept AABB time windows, then primitive narrowphase owns
normal/TOI. Evidence: `SqQueryLegacy.h:168-180`, `SqQueryLegacy.h:193-205`.

This layer should therefore be audited for false negatives, traversal safety,
and deterministic candidate inclusion, not for KCC walkability or wall/ground
policy.

## 4. Broadphase / Narrowphase Boundary

PhysX separates public SceneQuery, static/dynamic pruner traversal, exact
geometry dispatch, and result classification. Reference:
`scenequery-pipeline.md:29-34`, `scenequery-pipeline.md:40-46`.

Current EngineLab has one static BVH built from solid AABBs and triangles in
`CollisionWorldLegacy::BuildStatic`. Evidence:
`CollisionWorldLegacy.cpp:13-35`.

That is acceptable for the current small engine, but the boundary is narrower
than PhysX:

- No dynamic pruner.
- No cache.
- No touch/block classification.
- No all-hits/touch buffer.
- No query flag choosing static/dynamic traversal.

Those are extension gaps, not immediate BVH correctness bugs.

## 5. Policy Leakage

BVH itself does not own KCC policy. However, `SweepCapsuleClosestHit_Fast`
mixes traversal, primitive dispatch, query-level filtering, and closest-hit
selection in one function. Evidence: `SqQueryLegacy.h:126-228`.

This makes later collector/filter refactoring harder because there is no
separate callback boundary like PhysX `PrunerCallback::invoke`. Reference:
`scenequery-pipeline.md:33`.

## 6. Determinism Risks

### Issue: Empty BVH root can be traversed as an internal node

- Severity: P0
- EngineLab evidence: Empty BVH creates one default node with `root = 0`
  (`SqBVH.h:179-183`). Leaf detection is `if (node.primCount)` in sweep and
  overlap traversal (`SqQueryLegacy.h:157-160`, `SqQueryLegacy.h:308-311`).
  Internal traversal then reads/pushes `node.left` and `node.right`
  (`SqQueryLegacy.h:212-225`, `SqQueryLegacy.h:346-350`).
- Reference evidence: PhysX pruner traversal assumes a valid tree traversal
  boundary and uses managed stack traversal; this audit does not claim PhysX
  uses an equivalent empty-root representation. Reference:
  `scenequery-pipeline.md:31-33`.
- Current behavior: The empty root has `left == 0`, `right == 0`, and
  `primCount == 0`. If root bounds pass the broadphase test, traversal can push
  root as its own child.
- Why it matters: Empty collision worlds should return no-hit/no-contact. A
  self-push can create unbounded stack growth.
- Minimal test: Build `StaticBVH` with zero primitives and sweep/overlap a
  capsule whose broadphase AABB intersects the default root bounds at origin.
  Expected result: no hit/contact and `scratch.sp == 0` at return.

### Issue: Fixed traversal stack has no overflow guard

- Severity: P0
- EngineLab evidence: `QueryScratch` has `NodeTask stack[512]` and `sp`
  (`SqQueryLegacy.h:49-51`). Root and child pushes increment `sp` with no bounds
  check (`SqQueryLegacy.h:148`, `SqQueryLegacy.h:220`,
  `SqQueryLegacy.h:304`, `SqQueryLegacy.h:348-350`).
- Reference evidence: PhysX uses an inline traversal stack but resizes when
  capacity is reached (`.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/src/SqAABBTreeQuery.h:177-181`,
  `.ref/PhysX_3.4/PhysX_3.4/Source/SceneQuery/src/SqAABBTreeQuery.h:210-211`).
- Current behavior: For the current balanced median-split BVH, 512 is probably
  enough for 10k static cubes, but the invariant is not enforced.
- Why it matters: Stack overflow is memory corruption, not a mere missed
  optimization.
- Minimal test: Add a debug probe that records max stack usage for a 10k cube
  broad sweep and asserts it stays below capacity. Also test empty BVH to catch
  self-push before it grows.

### Issue: Empty BVH comment and query behavior disagree

- Severity: P1
- EngineLab evidence: `SqQueryLegacy.h` comments state `[PR3.6] Empty BVH:
  returns Hit{hit=false, t=1.0}` (`SqQueryLegacy.h:23`). Current query has no
  explicit `bvh.prims.empty()` guard before reading `bvh.nodes[bvh.root]`
  (`SqQueryLegacy.h:143-148`).
- Reference evidence: Not applicable; this is local contract mismatch.
- Current behavior: The intended contract exists only as a comment.
- Why it matters: Tests may assume empty BVH safety without code enforcing it.
- Minimal test: Empty BVH sweep and overlap probes.

### Issue: Overlap Top-K tie handling depends on traversal topology

- Severity: P1
- EngineLab evidence: Overlap keeps max 32 contacts (`SqQueryLegacy.h:250`,
  `SqQueryLegacy.h:290-291`). If full, it finds shallowest by depth only and
  evicts only when the new contact is strictly deeper (`SqQueryLegacy.h:327-340`).
  Final sort is deterministic (`SqQueryLegacy.h:353-354`).
- Reference evidence: PhysX touch hits are not promised sorted, while block hits
  shrink distance and become nearest-block state (`query-filtering.md:39-41`,
  `scenequery-pipeline.md:41-42`).
- Current behavior: Same-depth contacts beyond capacity are retained according
  to DFS/BVH topology, then sorted after the fact.
- Why it matters: KCC recovery at seams may see different contacts after a BVH
  rebuild even when world geometry is equivalent.
- Minimal test: Capsule overlapping more than 32 same-depth AABBs should produce
  a stable contact set independent of insertion order or BVH partitioning.

### Issue: BVH4 is not the immediate fix lane

- Severity: P2
- EngineLab evidence: Current broadphase is a top-level static binary BVH over
  AABB/OBB/Tri refs (`SqBVH.h:47-56`, `SqBVH.h:144-189`).
- Reference evidence: PhysX BV4 evidence in the existing contract is mesh
  midphase-specific (`mesh-sweeps-ordering.md:23-24`,
  `mesh-sweeps-ordering.md:36-38`).
- Current behavior: EngineLab does not yet have a mesh-midphase object
  equivalent to PhysX triangle mesh BV4.
- Why it matters: Moving to BVH4 now would add complexity before fixing empty
  root, stack safety, filter boundaries, and result contracts.
- Minimal test: Not a test-first change. First collect node visits, primitive
  tests, max stack, and broadphase false-negative probes.

## 7. Minimal Fix Direction

Do not implement from this audit alone.

Smallest safe follow-up patches:

1. Add early no-op guards for empty `bvh.prims` in sweep and overlap query
   entry points.
2. Add stack capacity guard or debug assertion plus max-stack telemetry.
3. Add a deterministic empty-BVH test before touching BVH4 or collector design.
4. Keep binary BVH for now; defer BVH4 until a separate mesh-midphase need is
   proven.
