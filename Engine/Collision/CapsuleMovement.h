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
    // INVARIANTS:
    //   - NEVER mutates SceneView or any WorldState state
    //   - When enableYSweep=true: no ResolveAxis(Y) in iteration loop
    //   - Candidate ordering: sort + unique by cube index
    //   - Tie-break: earliest TOI; on tie within 1e-6f, lower cubeIdx wins
    //
    // DETERMINISM: Identical to previous WorldState inline code.

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

    // Main solver: Y sweep -> XZ sweep/slide -> cleanup -> step-up -> support snap.
    CapsuleMoveResult SolveCapsuleMovement(
        const SceneView& scene,
        const CapsuleMoveRequest& req,
        CollisionStats& stats);

#if defined(_DEBUG)
    // Equivalence harness: runs the iteration loop WITH ResolveAxis(Y)
    // (the old code path) and compares against the main solver's output.
    // When enableYSweep=true, ResolveAxis(Y) is a confirmed no-op,
    // so results must match exactly. Logs [LEGACY_COMPARE_DIFF] on mismatch.
    CapsuleMoveResult SolveCapsuleMovement_WithAxisY(
        const SceneView& scene,
        const CapsuleMoveRequest& req,
        CollisionStats& stats);
#endif

}} // namespace Engine::Collision
