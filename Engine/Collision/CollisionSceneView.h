#pragma once
#include "CollisionTypes.h"
#include "../WorldTypes.h"
#include <vector>
#include <cstdint>

namespace Engine { namespace Collision {

    // FILE CONTRACT: CollisionSceneView
    // READS: WorldState spatial grid + collider geometry (via implementations)
    // WRITES: NOTHING. Implementations MUST NOT mutate WorldState.
    // INVARIANT: All methods are const. No side effects.
    //
    // PR2.10: ColliderId identity system
    //   - QueryCandidates returns ColliderId (uint32_t); values 0..N map to cubes
    //   - GetColliderAABB: MUST NOT be called with kInvalidCollider or kFloorCollider
    //   - GetColliderProps: [SEAM-COLLIDER-PROPS-UNUSED] stub for PR3.x
    class SceneView {
    public:
        virtual ~SceneView() = default;
        virtual std::vector<ColliderId> QueryCandidates(const AABB& broadphaseBox) const = 0;
        virtual AABB GetColliderAABB(ColliderId id) const = 0;
        virtual ColliderProps GetColliderProps(ColliderId id) const = 0;
    };

}} // namespace Engine::Collision
