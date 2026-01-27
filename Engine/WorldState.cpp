#include "WorldState.h"
#include "../Renderer/DX12/Dx12Context.h"  // For HUDSnapshot
#include <cmath>
#include <cstdio>
#include <Windows.h>  // For OutputDebugStringA

using namespace DirectX;

namespace Engine
{
    void WorldState::Initialize()
    {
        // Reset pawn to spawn position
        m_pawn = PawnState{};
        m_pawn.posX = m_config.spawnX;
        m_pawn.posY = m_config.spawnY;
        m_pawn.posZ = m_config.spawnZ;
        m_pawn.yaw = 0.0f;
        m_pawn.pitch = 0.0f;
        m_pawn.onGround = false;  // Start in air, floor resolve will set true

        // Initialize camera at offset from pawn
        m_camera.eyeX = m_pawn.posX;
        m_camera.eyeY = m_pawn.posY + m_config.camOffsetUp;
        m_camera.eyeZ = m_pawn.posZ - m_config.camOffsetBehind;
        m_camera.fovY = m_config.baseFovY;

        m_sprintAlpha = 0.0f;
        m_jumpConsumedThisFrame = false;
        m_jumpQueued = false;

        // Reset respawn tracking
        m_respawnCount = 0;
        m_lastRespawnReason = nullptr;

        // Part 2: Build spatial grid for cube collision
        BuildSpatialGrid();

        // Day3.4: Collision geometry derivation proof log
        OutputDebugStringA("[CollisionInit] CubeLocalHalf=1.0\n");
        OutputDebugStringA("[CollisionInit] RenderScale: XZ=0.9 Y=3.0\n");
        char collisionBuf[256];
        sprintf_s(collisionBuf, "[CollisionInit] DerivedCollision: halfXZ=%.2f Y=[%.1f,%.1f]\n",
            m_config.cubeHalfXZ, m_config.cubeMinY, m_config.cubeMaxY);
        OutputDebugStringA(collisionBuf);
    }

    void WorldState::BeginFrame()
    {
        // Reset per-frame flags
        m_jumpConsumedThisFrame = false;
    }

    void WorldState::ApplyMouseLook(float deltaX, float deltaY)
    {
        // Mouse right -> yaw decreases -> turn right (matches camera yaw convention)
        m_pawn.yaw -= deltaX * m_config.mouseSensitivity;

        // Mouse down -> pitch decreases -> look down
        m_pawn.pitch -= deltaY * m_config.mouseSensitivity;

        // Clamp pitch
        if (m_pawn.pitch < m_config.pitchClampMin)
            m_pawn.pitch = m_config.pitchClampMin;
        if (m_pawn.pitch > m_config.pitchClampMax)
            m_pawn.pitch = m_config.pitchClampMax;
    }

    void WorldState::TickFixed(const InputState& input, float fixedDt)
    {
        // Reset collision stats for this tick
        m_collisionStats = CollisionStats{};

        // 1. Apply yaw rotation (axis-based)
        m_pawn.yaw += input.yawAxis * m_config.lookSpeed * fixedDt;

        // 2. Apply pitch rotation with clamping
        m_pawn.pitch += input.pitchAxis * m_config.lookSpeed * fixedDt;
        if (m_pawn.pitch < m_config.pitchClampMin) m_pawn.pitch = m_config.pitchClampMin;
        if (m_pawn.pitch > m_config.pitchClampMax) m_pawn.pitch = m_config.pitchClampMax;

        // 3. Compute forward/right vectors from pawn yaw (camera-relative movement)
        float cosYaw = cosf(m_pawn.yaw);
        float sinYaw = sinf(m_pawn.yaw);
        float forwardX = sinYaw;
        float forwardZ = cosYaw;
        float rightX = cosYaw;
        float rightZ = -sinYaw;

        // 4. Smooth sprint alpha toward target
        float targetSprint = input.sprint ? 1.0f : 0.0f;
        float sprintDelta = (targetSprint - m_sprintAlpha) * m_config.sprintSmoothRate * fixedDt;
        m_sprintAlpha += sprintDelta;
        if (m_sprintAlpha < 0.0f) m_sprintAlpha = 0.0f;
        if (m_sprintAlpha > 1.0f) m_sprintAlpha = 1.0f;

        // 5. Compute velocity from input and sprint
        float speedMultiplier = 1.0f + (m_config.sprintMultiplier - 1.0f) * m_sprintAlpha;
        float currentSpeed = m_config.walkSpeed * speedMultiplier;

        // Horizontal velocity from input
        m_pawn.velX = (forwardX * input.moveZ + rightX * input.moveX) * currentSpeed;
        m_pawn.velZ = (forwardZ * input.moveZ + rightZ * input.moveX) * currentSpeed;

        // 6. Apply gravity if not on ground
        if (!m_pawn.onGround)
        {
            m_pawn.velY -= m_config.gravity * fixedDt;
        }
        else
        {
            // On ground, reset vertical velocity (unless jumping)
            if (m_pawn.velY < 0.0f)
            {
                m_pawn.velY = 0.0f;
            }
        }

        // 7. Jump: only if on ground, input.jump is true, and not already consumed this frame
        if (m_pawn.onGround && input.jump && !m_jumpConsumedThisFrame)
        {
            m_pawn.velY = m_config.jumpVelocity;
            m_pawn.onGround = false;
            m_jumpQueued = true;  // Evidence flag
            m_jumpConsumedThisFrame = true;
            m_justJumpedThisTick = true;  // Day3.5: Prevent support query from clearing onGround
        }

        // Part 2: Axis-separated collision resolution
        // Day3.5: onGround is now determined by QuerySupport, not reset here

        // Day3.4: Apply velocity to get proposed position
        float newX = m_pawn.posX + m_pawn.velX * fixedDt;
        float newZ = m_pawn.posZ + m_pawn.velZ * fixedDt;
        float newY = m_pawn.posY + m_pawn.velY * fixedDt;

        // Day3.4: Iterative collision resolution (X→Z→Y per iteration)
        const int MAX_ITERATIONS = 8;
        const float CONVERGENCE_EPSILON = 0.001f;
        bool converged = false;

        for (int iter = 0; iter < MAX_ITERATIONS; ++iter)
        {
            float totalDelta = 0.0f;

            // X axis
            float prevX = newX;
            ResolveAxis(newX, newX, newY, newZ, Axis::X);
            totalDelta += fabsf(newX - prevX);

            // Z axis
            float prevZ = newZ;
            ResolveAxis(newZ, newX, newY, newZ, Axis::Z);
            totalDelta += fabsf(newZ - prevZ);

            // Y axis
            float prevY = newY;
            ResolveAxis(newY, newX, newY, newZ, Axis::Y);
            totalDelta += fabsf(newY - prevY);

            m_collisionStats.iterationsUsed = static_cast<uint8_t>(iter + 1);

            // Convergence check: if total correction is negligible, we've converged
            if (totalDelta < CONVERGENCE_EPSILON)
            {
                converged = true;
                break;
            }
        }

        // hitMaxIter = true ONLY if we ran all iterations AND did NOT converge
        m_collisionStats.hitMaxIter = (m_collisionStats.iterationsUsed == MAX_ITERATIONS && !converged);

        // Commit final position
        m_pawn.posX = newX;
        m_pawn.posY = newY;
        m_pawn.posZ = newZ;

        // Reset floor clamp flag before support query
        m_didFloorClampThisTick = false;

        // Day3.5: Query support (ALWAYS for HUD)
        SupportResult support = QuerySupport(m_pawn.posX, m_pawn.posY, m_pawn.posZ, m_pawn.velY);

        // Day3.6: Floor penetration recovery (handle overshoots beyond epsilon)
        if (support.source == SupportSource::NONE && m_pawn.velY <= 0.0f)
        {
            bool inFloorBounds = (m_pawn.posX >= m_config.floorMinX && m_pawn.posX <= m_config.floorMaxX &&
                                  m_pawn.posZ >= m_config.floorMinZ && m_pawn.posZ <= m_config.floorMaxZ);
            // If pawn is below floor and in bounds, force floor recovery
            if (inFloorBounds && m_pawn.posY < m_config.floorY)
            {
                float overshoot = m_config.floorY - m_pawn.posY;
                char buf[256];
                sprintf_s(buf, "[FLOOR_RECOVERY] posY=%.3f overshoot=%.3f velY=%.2f\n",
                    m_pawn.posY, overshoot, m_pawn.velY);
                OutputDebugStringA(buf);

                // Force floor support
                support.source = SupportSource::FLOOR;
                support.supportY = m_config.floorY;
                support.cubeId = -1;
                support.gap = overshoot;
            }
        }

        // Copy to collision stats for HUD
        m_collisionStats.supportSource = support.source;
        m_collisionStats.supportY = support.supportY;
        m_collisionStats.supportCubeId = support.cubeId;
        m_collisionStats.supportGap = support.gap;
        m_collisionStats.snappedThisTick = false;

        // Day3.5: Support application
        // Safety C: Rising case - clear onGround
        if (!m_justJumpedThisTick && m_pawn.velY > 0.0f)
        {
            m_pawn.onGround = false;
        }
        // Falling or standing case
        else if (!m_justJumpedThisTick && m_pawn.velY <= 0.0f)
        {
            if (support.source != SupportSource::NONE)
            {
                // Apply snap
                if (m_pawn.posY != support.supportY)
                {
                    m_pawn.posY = support.supportY;
                    m_collisionStats.snappedThisTick = true;
                    m_didFloorClampThisTick = true;
                }
                m_pawn.velY = 0.0f;
                m_pawn.onGround = true;
            }
            else
            {
                m_pawn.onGround = false;
            }
        }
        // If justJumped: don't touch onGround (already set false in jump)

        // Gap anomaly detection
        if (support.source == SupportSource::NONE && fabsf(m_pawn.posY - 3.0f) < 0.02f)
        {
            bool inFloorBounds = (m_pawn.posX >= m_config.floorMinX && m_pawn.posX <= m_config.floorMaxX &&
                                  m_pawn.posZ >= m_config.floorMinZ && m_pawn.posZ <= m_config.floorMaxZ);
            char buf[320];
            sprintf_s(buf, "[GAP_ANOMALY] px=%.2f pz=%.2f py=%.3f inFloor=%d gap=%.3f foot=[%.2f..%.2f] cand=%u\n",
                m_pawn.posX, m_pawn.posZ, m_pawn.posY, inFloorBounds ? 1 : 0, support.gap,
                m_pawn.posX - m_config.pawnHalfWidth, m_pawn.posX + m_config.pawnHalfWidth,
                support.candidateCount);
            OutputDebugStringA(buf);
        }

        m_justJumpedThisTick = false;  // Reset for next tick

        // Legacy floor collision (now logging-only)
        ResolveFloorCollision();

        // 10. KillZ check (respawn if below threshold)
        CheckKillZ();
    }

    void WorldState::ResolveFloorCollision()
    {
        // Day3.5: This function is now logging-only. Support/snap logic moved to QuerySupport.
        float pawnBottomY = m_pawn.posY;

        bool inFloorBounds = (m_pawn.posX >= m_config.floorMinX && m_pawn.posX <= m_config.floorMaxX &&
                              m_pawn.posZ >= m_config.floorMinZ && m_pawn.posZ <= m_config.floorMaxZ);

        // [FLOOR-C] Log when OUT of bounds
        if (!inFloorBounds) {
            char buf[256];
            sprintf_s(buf, "[FLOOR-C] OUT_OF_BOUNDS! posX=%.2f posZ=%.2f boundsX=[%.1f,%.1f] boundsZ=[%.1f,%.1f]\n",
                m_pawn.posX, m_pawn.posZ,
                m_config.floorMinX, m_config.floorMaxX,
                m_config.floorMinZ, m_config.floorMaxZ);
            OutputDebugStringA(buf);
        }
    }

    void WorldState::CheckKillZ()
    {
        if (m_pawn.posY < m_config.killZ)
        {
            m_respawnCount++;

            // Log with position BEFORE respawn
            char buf[256];
            sprintf_s(buf, "[KILLZ] #%u at pos=(%.2f,%.2f,%.2f) - respawning\n",
                m_respawnCount, m_pawn.posX, m_pawn.posY, m_pawn.posZ);
            OutputDebugStringA(buf);

            // Respawn at spawn point
            m_pawn.posX = m_config.spawnX;
            m_pawn.posY = m_config.spawnY;
            m_pawn.posZ = m_config.spawnZ;
            m_pawn.velX = m_pawn.velY = m_pawn.velZ = 0.0f;
            m_pawn.onGround = false;  // NOT true! Floor resolve will set it next tick
            m_lastRespawnReason = "KillZ";
        }
    }

    // ========================================================================
    // Part 2: Spatial Hash and Cube Collision
    // ========================================================================

    void WorldState::BuildSpatialGrid()
    {
        if (m_spatialGridBuilt) return;

        // Clear grid
        for (int gz = 0; gz < GRID_SIZE; ++gz)
            for (int gx = 0; gx < GRID_SIZE; ++gx)
                m_spatialGrid[gz][gx].clear();

        // Each cube at grid (gx, gz) maps to cell (gx, gz)
        // since cube centers align with cell centers
        for (int gz = 0; gz < GRID_SIZE; ++gz)
        {
            for (int gx = 0; gx < GRID_SIZE; ++gx)
            {
                uint16_t cubeIdx = static_cast<uint16_t>(gz * GRID_SIZE + gx);
                m_spatialGrid[gz][gx].push_back(cubeIdx);
            }
        }

        m_spatialGridBuilt = true;
        OutputDebugStringA("[Collision] Built spatial hash: 10000 cubes in 100x100 grid\n");
    }

    int WorldState::WorldToCellX(float x) const
    {
        // Cell size = 2.0 (cube spacing), grid origin = (-100, -100)
        int cell = static_cast<int>(floorf((x + 100.0f) / 2.0f));
        if (cell < 0) cell = 0;
        if (cell > GRID_SIZE - 1) cell = GRID_SIZE - 1;
        return cell;
    }

    int WorldState::WorldToCellZ(float z) const
    {
        int cell = static_cast<int>(floorf((z + 100.0f) / 2.0f));
        if (cell < 0) cell = 0;
        if (cell > GRID_SIZE - 1) cell = GRID_SIZE - 1;
        return cell;
    }

    AABB WorldState::BuildPawnAABB(float px, float py, float pz) const
    {
        // Pawn AABB: feet at posY, head at posY+height
        AABB aabb;
        aabb.minX = px - m_config.pawnHalfWidth;
        aabb.maxX = px + m_config.pawnHalfWidth;
        aabb.minY = py;
        aabb.maxY = py + m_config.pawnHeight;
        aabb.minZ = pz - m_config.pawnHalfWidth;
        aabb.maxZ = pz + m_config.pawnHalfWidth;
        return aabb;
    }

    AABB WorldState::GetCubeAABB(uint16_t cubeIdx) const
    {
        // Cube at grid (gx, gz):
        int gx = cubeIdx % GRID_SIZE;
        int gz = cubeIdx / GRID_SIZE;
        float cx = 2.0f * gx - 99.0f;  // Center X (matches transform generation)
        float cz = 2.0f * gz - 99.0f;  // Center Z

        AABB aabb;
        aabb.minX = cx - m_config.cubeHalfXZ;
        aabb.maxX = cx + m_config.cubeHalfXZ;
        aabb.minY = m_config.cubeMinY;
        aabb.maxY = m_config.cubeMaxY;
        aabb.minZ = cz - m_config.cubeHalfXZ;
        aabb.maxZ = cz + m_config.cubeHalfXZ;
        return aabb;
    }

    bool WorldState::Intersects(const AABB& a, const AABB& b) const
    {
        // Day3.5: Strict intersection (open intervals - touching doesn't count)
        return (a.minX < b.maxX && a.maxX > b.minX &&
                a.minY < b.maxY && a.maxY > b.minY &&
                a.minZ < b.maxZ && a.maxZ > b.minZ);
    }

    float WorldState::ComputeSignedPenetration(const AABB& pawn, const AABB& cube, Axis axis) const
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

        // Sign: push pawn away from cube center
        float sign = (centerPawn < centerCube) ? 1.0f : -1.0f;

        return sign * overlap;
    }

    std::vector<uint16_t> WorldState::QuerySpatialHash(const AABB& pawn) const
    {
        std::vector<uint16_t> candidates;

        // Find cell range for pawn AABB
        int minCellX = WorldToCellX(pawn.minX);
        int maxCellX = WorldToCellX(pawn.maxX);
        int minCellZ = WorldToCellZ(pawn.minZ);
        int maxCellZ = WorldToCellZ(pawn.maxZ);

        // Gather all cubes in overlapping cells
        for (int gz = minCellZ; gz <= maxCellZ; ++gz)
        {
            for (int gx = minCellX; gx <= maxCellX; ++gx)
            {
                const auto& cell = m_spatialGrid[gz][gx];
                for (uint16_t idx : cell)
                {
                    candidates.push_back(idx);
                }
            }
        }

        return candidates;
    }

    SupportResult WorldState::QuerySupport(float px, float py, float pz, float velY) const
    {
        SupportResult result;
        const float SUPPORT_EPSILON = 0.05f;
        float pawnBottom = py;

        // Early-out when rising (still return result for HUD)
        if (velY > 0.0f) return result;

        // Build query AABB with Y expanded
        AABB queryAABB = BuildPawnAABB(px, py, pz);
        queryAABB.minY -= SUPPORT_EPSILON;
        queryAABB.maxY += SUPPORT_EPSILON;

        // 1. Check floor support
        bool inFloorBounds = (px >= m_config.floorMinX && px <= m_config.floorMaxX &&
                              pz >= m_config.floorMinZ && pz <= m_config.floorMaxZ);
        if (inFloorBounds && fabsf(pawnBottom - m_config.floorY) < SUPPORT_EPSILON &&
            pawnBottom >= m_config.floorY - SUPPORT_EPSILON)  // Safety: not below floor
        {
            result.source = SupportSource::FLOOR;
            result.supportY = m_config.floorY;
            result.cubeId = -1;
            result.gap = fabsf(pawnBottom - m_config.floorY);
        }

        // 2. Check cube support (pick highest)
        float pawnMinX = queryAABB.minX;
        float pawnMaxX = queryAABB.maxX;
        float pawnMinZ = queryAABB.minZ;
        float pawnMaxZ = queryAABB.maxZ;

        auto candidates = QuerySpatialHash(queryAABB);
        result.candidateCount = static_cast<uint32_t>(candidates.size());

        for (uint16_t cubeIdx : candidates)
        {
            AABB cube = GetCubeAABB(cubeIdx);

            // XZ footprint overlap (inclusive)
            bool xzOverlap = (pawnMinX <= cube.maxX && pawnMaxX >= cube.minX &&
                              pawnMinZ <= cube.maxZ && pawnMaxZ >= cube.minZ);
            if (!xzOverlap) continue;

            float cubeTop = cube.maxY;
            float dist = fabsf(pawnBottom - cubeTop);

            // Safety B: Only support from above (pawnBottom >= cubeTop - SUPPORT_EPSILON)
            if (pawnBottom < cubeTop - SUPPORT_EPSILON) continue;

            // Safety A: Select cube if better than current
            if (dist < SUPPORT_EPSILON && (result.source == SupportSource::NONE || cubeTop > result.supportY))
            {
                result.source = SupportSource::CUBE;
                result.supportY = cubeTop;
                result.cubeId = cubeIdx;
                result.gap = dist;
            }
        }

        return result;
    }

    void WorldState::ResolveAxis(float& posAxis, float currentPosX, float currentPosY, float currentPosZ, Axis axis)
    {
        // Build pawn AABB at proposed position
        float px = (axis == Axis::X) ? posAxis : currentPosX;
        float py = (axis == Axis::Y) ? posAxis : currentPosY;
        float pz = (axis == Axis::Z) ? posAxis : currentPosZ;
        AABB pawn = BuildPawnAABB(px, py, pz);

        auto candidates = QuerySpatialHash(pawn);
        m_collisionStats.candidatesChecked += static_cast<uint32_t>(candidates.size());

        // Find deepest penetration cube (stability: avoid multi-cube jitter)
        float deepestPen = 0.0f;
        int deepestCubeIdx = -1;
        float deepestCubeTop = 0.0f;

        for (uint16_t cubeIdx : candidates)
        {
            AABB cube = GetCubeAABB(cubeIdx);
            if (!Intersects(pawn, cube)) continue;

            // Day3.4: Count actual intersections (summed, not deduplicated)
            m_collisionStats.contacts++;

            float pen = ComputeSignedPenetration(pawn, cube, axis);

            // Day3.4: Track max penetration for diagnostics
            if (fabsf(pen) > m_collisionStats.maxPenetrationAbs)
                m_collisionStats.maxPenetrationAbs = fabsf(pen);

            if (fabsf(pen) > fabsf(deepestPen))
            {
                deepestPen = pen;
                deepestCubeIdx = cubeIdx;
                deepestCubeTop = cube.maxY;
            }
        }

        // Apply only the deepest correction
        if (deepestCubeIdx >= 0 && deepestPen != 0.0f)
        {
            posAxis -= deepestPen;
            m_collisionStats.penetrationsResolved++;
            m_collisionStats.lastHitCubeId = deepestCubeIdx;
            m_collisionStats.lastAxisResolved = axis;

            // Debug log for collision proof
            char buf[128];
            const char* axisName = (axis == Axis::X) ? "X" : (axis == Axis::Y) ? "Y" : "Z";
            sprintf_s(buf, "[Collision] cube=%d axis=%s pen=%.3f\n", deepestCubeIdx, axisName, deepestPen);
            OutputDebugStringA(buf);

            // Zero velocity on this axis
            if (axis == Axis::X) m_pawn.velX = 0.0f;
            if (axis == Axis::Z) m_pawn.velZ = 0.0f;
            if (axis == Axis::Y)
            {
                // Day3.5: onGround is now determined by QuerySupport, not here
                m_pawn.velY = 0.0f;
            }
        }
    }

    void WorldState::TickFrame(float frameDt)
    {
        // 1. Compute target camera position (behind and above pawn)
        float cosYaw = cosf(m_pawn.yaw);
        float sinYaw = sinf(m_pawn.yaw);
        float targetEyeX = m_pawn.posX - sinYaw * m_config.camOffsetBehind;
        float targetEyeY = m_pawn.posY + m_config.camOffsetUp;
        float targetEyeZ = m_pawn.posZ - cosYaw * m_config.camOffsetBehind;

        // Smooth camera toward target
        float followAlpha = 1.0f - expf(-m_config.camFollowRate * frameDt);
        m_camera.eyeX += (targetEyeX - m_camera.eyeX) * followAlpha;
        m_camera.eyeY += (targetEyeY - m_camera.eyeY) * followAlpha;
        m_camera.eyeZ += (targetEyeZ - m_camera.eyeZ) * followAlpha;

        // 2. Smooth FOV toward target (based on sprint)
        float targetFov = m_config.baseFovY + (m_config.sprintFovY - m_config.baseFovY) * m_sprintAlpha;
        float fovAlpha = 1.0f - expf(-m_config.fovSmoothRate * frameDt);
        m_camera.fovY += (targetFov - m_camera.fovY) * fovAlpha;

        // 3. Clear jumpQueued after one render frame (evidence display)
        m_jumpQueued = false;
    }

    DirectX::XMFLOAT4X4 WorldState::BuildViewProj(float aspect) const
    {
        // Camera looks at pawn position
        XMFLOAT3 eye = { m_camera.eyeX, m_camera.eyeY, m_camera.eyeZ };
        XMFLOAT3 target = { m_pawn.posX, m_pawn.posY + 1.5f, m_pawn.posZ };  // Look at pawn center
        XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

        XMVECTOR eyeVec = XMLoadFloat3(&eye);
        XMVECTOR targetVec = XMLoadFloat3(&target);
        XMVECTOR upVec = XMLoadFloat3(&up);

        // Right-handed view and projection
        XMMATRIX view = XMMatrixLookAtRH(eyeVec, targetVec, upVec);
        XMMATRIX proj = XMMatrixPerspectiveFovRH(m_camera.fovY, aspect, 1.0f, 1000.0f);
        XMMATRIX viewProj = XMMatrixMultiply(view, proj);

        XMFLOAT4X4 result;
        XMStoreFloat4x4(&result, viewProj);
        return result;
    }

    Renderer::HUDSnapshot WorldState::BuildSnapshot() const
    {
        // Compute speed from horizontal velocity
        float speed = sqrtf(m_pawn.velX * m_pawn.velX + m_pawn.velZ * m_pawn.velZ);

        // Convert radians to degrees for HUD display
        constexpr float RAD_TO_DEG = 57.2957795131f;

        Renderer::HUDSnapshot snap = {};
        snap.mapName = m_map.name;
        snap.posX = m_pawn.posX;
        snap.posY = m_pawn.posY;
        snap.posZ = m_pawn.posZ;
        snap.velX = m_pawn.velX;
        snap.velY = m_pawn.velY;
        snap.velZ = m_pawn.velZ;
        snap.speed = speed;
        snap.onGround = m_pawn.onGround;
        snap.sprintAlpha = m_sprintAlpha;
        snap.yawDeg = m_pawn.yaw * RAD_TO_DEG;
        snap.pitchDeg = m_pawn.pitch * RAD_TO_DEG;
        snap.fovDeg = m_camera.fovY * RAD_TO_DEG;
        snap.jumpQueued = m_jumpQueued;

        // Part 1: Respawn tracking
        snap.respawnCount = m_respawnCount;
        snap.lastRespawnReason = m_lastRespawnReason;

        // Part 2: Collision stats
        snap.candidatesChecked = m_collisionStats.candidatesChecked;
        snap.penetrationsResolved = m_collisionStats.penetrationsResolved;
        snap.lastHitCubeId = m_collisionStats.lastHitCubeId;
        snap.lastAxisResolved = static_cast<uint8_t>(m_collisionStats.lastAxisResolved);

        // Day3.4: Iteration diagnostics
        snap.iterationsUsed = m_collisionStats.iterationsUsed;
        snap.contacts = m_collisionStats.contacts;
        snap.maxPenetrationAbs = m_collisionStats.maxPenetrationAbs;
        snap.hitMaxIter = m_collisionStats.hitMaxIter;

        // Day3.5: Support diagnostics
        snap.supportSource = static_cast<uint8_t>(m_collisionStats.supportSource);
        snap.supportY = m_collisionStats.supportY;
        snap.supportCubeId = m_collisionStats.supportCubeId;
        snap.snappedThisTick = m_collisionStats.snappedThisTick;
        snap.supportGap = m_collisionStats.supportGap;

        // Floor diagnostics
        snap.inFloorBounds = (m_pawn.posX >= m_config.floorMinX && m_pawn.posX <= m_config.floorMaxX &&
                              m_pawn.posZ >= m_config.floorMinZ && m_pawn.posZ <= m_config.floorMaxZ);
        snap.didFloorClamp = m_didFloorClampThisTick;
        snap.floorMinX = m_config.floorMinX;
        snap.floorMaxX = m_config.floorMaxX;
        snap.floorMinZ = m_config.floorMinZ;
        snap.floorMaxZ = m_config.floorMaxZ;
        snap.floorY = m_config.floorY;

        return snap;
    }
}
