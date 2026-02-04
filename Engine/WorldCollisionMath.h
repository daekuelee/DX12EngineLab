#pragma once

#include <cmath>
#include "WorldTypes.h"

//-------------------------------------------------------------------------
// CONTRACT: WorldCollisionMath.h - Stateless collision/math helpers
//
// READS: function parameters only
// WRITES: return values / out-params only
// FORBIDDEN: any access to WorldState members, globals, singletons, renderer
//
// PURPOSE:
//   Provide pure, testable math used by WorldState collision code.
//-------------------------------------------------------------------------

namespace Engine
{
    //-------------------------------------------------------------------------
    // CONTRACT: IntersectsAABB - Pure AABB intersection test
    //
    // READS: parameters a, b only
    // WRITES: return value only
    // INVARIANT: Strict intersection (open intervals - touching doesn't count)
    //-------------------------------------------------------------------------
    inline bool IntersectsAABB(const AABB& a, const AABB& b)
    {
        // Day3.5: Strict intersection (open intervals - touching doesn't count)
        return (a.minX < b.maxX && a.maxX > b.minX &&
                a.minY < b.maxY && a.maxY > b.minY &&
                a.minZ < b.maxZ && a.maxZ > b.minZ);
    }

    //-------------------------------------------------------------------------
    // CONTRACT: SignedPenetrationAABB - Pure signed penetration computation
    //
    // READS: parameters pawn, cube, axis only
    // WRITES: return value only
    // INVARIANT: Returns signed overlap; sign pushes pawn AWAY from cube center
    //-------------------------------------------------------------------------
    inline float SignedPenetrationAABB(const AABB& pawn, const AABB& cube, Axis axis)
    {
        // Center-based sign decision: push pawn AWAY from cube center
        float pawnMin, pawnMax, cubeMin, cubeMax;

        if (axis == Axis::X) {
            pawnMin = pawn.minX; pawnMax = pawn.maxX;
            cubeMin = cube.minX; cubeMax = cube.maxX;
        } else if (axis == Axis::Y) {
            pawnMin = pawn.minY; pawnMax = pawn.maxY;
            cubeMin = cube.minY; cubeMax = cube.maxY;
        } else { // Z
            pawnMin = pawn.minZ; pawnMax = pawn.maxZ;
            cubeMin = cube.minZ; cubeMax = cube.maxZ;
        }

        float centerPawn = (pawnMin + pawnMax) * 0.5f;
        float centerCube = (cubeMin + cubeMax) * 0.5f;
        float pawnHalf = (pawnMax - pawnMin) * 0.5f;
        float cubeHalf = (cubeMax - cubeMin) * 0.5f;

        // Overlap magnitude
        float overlap = (pawnHalf + cubeHalf) - fabsf(centerPawn - centerCube);

        // No penetration if overlap <= 0
        if (overlap <= 0.0f) return 0.0f;

        // Sign: push pawn away from cube center (negative direction if pawn is left/below)
        float sign = (centerPawn < centerCube) ? -1.0f : 1.0f;

        return sign * overlap;
    }
}
