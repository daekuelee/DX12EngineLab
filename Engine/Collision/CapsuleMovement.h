#pragma once
#include "CollisionTypes.h"
#include "CollisionSceneView.h"

namespace Engine { namespace Collision {

    // FILE CONTRACT: CapsuleMovement
    //
    // SCOPE: Stateless capsule collision solver.
    // READS: SceneView (spatial queries), CapsuleMoveRequest (config + state)
    // WRITES: CapsuleMoveResult (pos/vel/onGround), CollisionStats& (diagnostics)
    //
    // PUBLIC API (PR2.9→2.10):
    //   - DepenetrateInPlace: pre-velocity overlap ejection
    //   - MoveCapsuleKinematic: the ONLY movement entry point WorldState calls
    //
    // ColliderId IDENTITY (PR2.10):
    //   - ColliderId (uint32_t): values 0..N for real colliders
    //   - kInvalidCollider (UINT32_MAX): no-hit sentinel
    //   - kFloorCollider (UINT32_MAX-1): floor-hit sentinel
    //   - LegacyIdxForStats: maps sentinels to -1/-2 for stats/log compatibility
    //
    // PHASE CALL GRAPH: A → B → C → D → E
    //   A: SweepY (vertical safe-move)
    //   B: SweepXZ + CleanupXZ (horizontal safe-move)
    //   C: TryStepUp (at most once)
    //   D: Convergence loop (CleanupXZ ± ResolveAxis)
    //   E: FinalizeSupport (QuerySupport + floor recovery + snap)
    //
    // INVARIANTS:
    //   - NEVER mutates SceneView or any WorldState state
    //   - When enableYSweep=true: no ResolveAxis(Y) in iteration loop
    //   - Candidate ordering: NormalizeCandidates (sort + unique by ColliderId)
    //   - Tie-break: earliest TOI; within kTOI_TieEpsilon, lower ColliderId wins
    //   - StepUp attempted at most once per tick
    //   - QuerySupport called exactly once per tick (via FinalizeSupport)
    //   - enableCCD reserved, must be false
    //
    // DETERMINISM: Identical to PR2.8 SolveCapsuleMovement output.

    // Pre-solver safety net: push capsule out of overlapping cubes.
    // Called BEFORE velocity computation in TickFixed.
    struct DepenResult {
        float posX, posY, posZ;
        bool onGround;
        // Diagnostics (mapped to m_collisionStats.depen* fields)
        bool depenApplied;
        float depenTotalMag;
        bool depenClampTriggered;
        float depenMaxSingleMag;
        uint32_t depenOverlapCount;
        uint32_t depenIterations;
    };

    DepenResult DepenetrateInPlace(
        const SceneView& scene,
        const CapsuleGeom& geom,
        float posX, float posY, float posZ,
        bool onGround);

    // PR2.9: Single public entry point for capsule movement.
    // Replaces SolveCapsuleMovement. This is the ONLY function WorldState
    // calls for movement resolution.
    //
    // CONTRACT:
    //   - No CCD (enableCCD must be false; asserted in debug)
    //   - StepUp attempted at most once per tick (asserted in debug)
    //   - QuerySupport called exactly once per tick (asserted in debug)
    //   - Shared TOI contract: kTOI_TieEpsilon tie-break, NormalizeCandidates ordering
    CapsuleMoveResult MoveCapsuleKinematic(
        const SceneView& scene,
        const CapsuleMoveRequest& req,
        CollisionStats& stats);

#if defined(_DEBUG)
    // Equivalence harness: runs iteration loop WITH ResolveAxis(Y)
    // and compares against MoveCapsuleKinematic output.
    // When enableYSweep=true, ResolveAxis(Y) is a confirmed no-op,
    // so results must match exactly. Logs [LEGACY_COMPARE_DIFF] on mismatch.
    CapsuleMoveResult SolveCapsuleMovement_WithAxisY(
        const SceneView& scene,
        const CapsuleMoveRequest& req,
        CollisionStats& stats);
#endif

}} // namespace Engine::Collision
