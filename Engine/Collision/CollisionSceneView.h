#pragma once
#include "../WorldTypes.h"
#include <vector>
#include <cstdint>

namespace Engine { namespace Collision {

    // FILE CONTRACT: CollisionSceneView
    // READS: WorldState spatial grid + cube geometry (via implementations)
    // WRITES: NOTHING. Implementations MUST NOT mutate WorldState.
    // INVARIANT: All methods are const. No side effects.
    class SceneView {
    public:
        virtual ~SceneView() = default;
        virtual std::vector<uint16_t> QueryCandidates(const AABB& broadphaseBox) const = 0;
        virtual AABB GetCubeAABB(uint16_t cubeIdx) const = 0;
    };

}} // namespace Engine::Collision
