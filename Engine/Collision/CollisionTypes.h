#pragma once
#include "../WorldTypes.h"
#include <cstdint>

namespace Engine { namespace Collision {

    // PR2.10: Collider identity system
    using ColliderId = uint32_t;
    constexpr ColliderId kInvalidCollider = UINT32_MAX;
    constexpr ColliderId kFloorCollider   = UINT32_MAX - 1;

    enum class ColliderType : uint8_t { Box };

    struct ColliderProps {
        bool blocking;
        bool walkable;
        bool stepable;
    };

    struct CapsuleGeom {
        float radius;          // m_config.capsuleRadius (1.4)
        float halfHeight;      // m_config.capsuleHalfHeight (1.1)
        // AABB-equivalent dims (for broadphase + legacy path)
        float pawnHalfExtentX; // m_config.pawnHalfExtentX (1.4)
        float pawnHalfExtentZ; // m_config.pawnHalfExtentZ (0.4)
        float pawnHeight;      // m_config.pawnHeight (5.0)
    };

    struct FloorBounds {
        float floorY;
        float minX, maxX;
        float minZ, maxZ;
    };

    struct CapsuleMoveRequest {
        // Start state (snapshot from m_pawn)
        float posX, posY, posZ;
        float velX, velY, velZ;
        bool onGround;
        bool justJumped;
        float fixedDt;
        // Geometry
        CapsuleGeom geom;
        // Config
        bool enableYSweep;
        bool enableStepUp;
        float maxStepHeight;
        float sweepSkinY;
        // PR2.9: CCD placeholder — reserved for PR3.x, MUST be false
        bool enableCCD = false;
        // Floor
        FloorBounds floor;
        // Cube collision dims (for legacy ResolveAxis BuildPawnAABB)
        float cubeHalfXZ;
        float cubeMinY, cubeMaxY;
    };

    struct CapsuleMoveResult {
        float posX, posY, posZ;
        float velX, velY, velZ;
        bool onGround;
    };

}} // namespace Engine::Collision
