#include "WorldState.h"
#include "../Renderer/DX12/Dx12Context.h"  // For HUDSnapshot
#include <cmath>
#include <cstdio>
#include <algorithm>  // For std::sort, std::unique
#include <Windows.h>  // For OutputDebugStringA

using namespace DirectX;

// Day3.11 Phase 2: Capsule geometry helpers
namespace
{
    DirectX::XMFLOAT3 ClosestPointOnSegment(
        const DirectX::XMFLOAT3& A, const DirectX::XMFLOAT3& B, const DirectX::XMFLOAT3& P)
    {
        float ABx = B.x - A.x, ABy = B.y - A.y, ABz = B.z - A.z;
        float APx = P.x - A.x, APy = P.y - A.y, APz = P.z - A.z;
        float dotABAB = ABx * ABx + ABy * ABy + ABz * ABz;
        if (dotABAB < 1e-8f) return A;
        float t = (APx * ABx + APy * ABy + APz * ABz) / dotABAB;
        t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
        return { A.x + t * ABx, A.y + t * ABy, A.z + t * ABz };
    }

    DirectX::XMFLOAT3 ClosestPointOnAABB(const DirectX::XMFLOAT3& P, const Engine::AABB& box)
    {
        auto clamp = [](float v, float lo, float hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); };
        return { clamp(P.x, box.minX, box.maxX), clamp(P.y, box.minY, box.maxY), clamp(P.z, box.minZ, box.maxZ) };
    }

    void ClosestPointsSegmentAABB(const DirectX::XMFLOAT3& segA, const DirectX::XMFLOAT3& segB,
        const Engine::AABB& box, DirectX::XMFLOAT3& outOnSeg, DirectX::XMFLOAT3& outOnBox)
    {
        DirectX::XMFLOAT3 onSeg = { (segA.x + segB.x) * 0.5f, (segA.y + segB.y) * 0.5f, (segA.z + segB.z) * 0.5f };
        for (int i = 0; i < 4; ++i) {
            DirectX::XMFLOAT3 onBox = ClosestPointOnAABB(onSeg, box);
            onSeg = ClosestPointOnSegment(segA, segB, onBox);
        }
        outOnSeg = onSeg;
        outOnBox = ClosestPointOnAABB(onSeg, box);
    }

    DirectX::XMFLOAT3 FindMinPenetrationAxis(const DirectX::XMFLOAT3& pt, const Engine::AABB& box, float& outDepth)
    {
        float d[6] = { pt.x - box.minX, box.maxX - pt.x, pt.y - box.minY, box.maxY - pt.y, pt.z - box.minZ, box.maxZ - pt.z };
        DirectX::XMFLOAT3 n[6] = { {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1} };
        int best = 0;
        for (int i = 1; i < 6; ++i) if (d[i] < d[best]) best = i;
        outDepth = d[best];
        return n[best];
    }

    struct CapsuleOverlapResult { bool hit; DirectX::XMFLOAT3 normal; float depth; };

    CapsuleOverlapResult CapsuleAABBOverlap(float feetY, float posX, float posZ, float r, float hh, const Engine::AABB& box)
    {
        CapsuleOverlapResult res = { false, {0,0,0}, 0.0f };
        DirectX::XMFLOAT3 segA = { posX, feetY + r, posZ };
        DirectX::XMFLOAT3 segB = { posX, feetY + r + 2.0f * hh, posZ };
        DirectX::XMFLOAT3 onSeg, onBox;
        ClosestPointsSegmentAABB(segA, segB, box, onSeg, onBox);
        float dx = onSeg.x - onBox.x, dy = onSeg.y - onBox.y, dz = onSeg.z - onBox.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > r * r) return res;
        res.hit = true;
        float dist = sqrtf(distSq);
        if (dist > 1e-6f) {
            float inv = 1.0f / dist;
            // FIX: Normal must point FROM capsule TOWARD box surface to push capsule OUT
            // dx,dy,dz = onSeg - onBox points FROM box TO capsule, so negate for push direction
            res.normal = { -dx * inv, -dy * inv, -dz * inv };
            res.depth = r - dist;
#ifdef DEBUG_CAPSULE_OVERLAP
            char buf[128];
            sprintf_s(buf, "[OVERLAP] dist=%.4f depth=%.4f n=(%.2f,%.2f,%.2f)\n",
                dist, res.depth, res.normal.x, res.normal.y, res.normal.z);
            OutputDebugStringA(buf);
#endif
        } else {
            res.normal = FindMinPenetrationAxis(onSeg, box, res.depth);
            res.depth += r;
        }
        return res;
    }

    // Day3.11 Phase 3: Slab method for segment-AABB sweep (XZ only)
    struct SweepResult { bool hit; float t; float normalX; float normalZ; };

    SweepResult SegmentAABBSweepXZ(float startX, float startZ, float dx, float dz,
                                   float boxMinX, float boxMaxX, float boxMinZ, float boxMaxZ)
    {
        SweepResult res = { false, 1.0f, 0.0f, 0.0f };
        const float EPS = 1e-8f;

        // X slab
        float invDx = (fabsf(dx) > EPS) ? (1.0f / dx) : (dx >= 0 ? 1e8f : -1e8f);
        float t1x = (boxMinX - startX) * invDx;
        float t2x = (boxMaxX - startX) * invDx;
        float tNearX = fminf(t1x, t2x);
        float tFarX = fmaxf(t1x, t2x);

        // Z slab
        float invDz = (fabsf(dz) > EPS) ? (1.0f / dz) : (dz >= 0 ? 1e8f : -1e8f);
        float t1z = (boxMinZ - startZ) * invDz;
        float t2z = (boxMaxZ - startZ) * invDz;
        float tNearZ = fminf(t1z, t2z);
        float tFarZ = fmaxf(t1z, t2z);

        float tEnter = fmaxf(tNearX, tNearZ);
        float tExit = fminf(tFarX, tFarZ);

        if (tEnter > tExit || tExit < 0.0f || tEnter > 1.0f)
            return res;

        res.hit = true;
        res.t = fmaxf(tEnter, 0.0f);

        // Determine hit normal (which axis entered last)
        if (tNearX > tNearZ)
            res.normalX = (dx > 0) ? -1.0f : 1.0f;
        else
            res.normalZ = (dz > 0) ? -1.0f : 1.0f;

        return res;
    }

    // Sweep capsule (both P0 and P1) against expanded AABB, return earliest hit
    // FIX: Added onGround parameter to use body-only Y-overlap when grounded
    SweepResult SweepCapsuleVsCubeXZ(float posX, float posZ, float feetY, float r, float hh,
                                      float dx, float dz, const Engine::AABB& cube, bool onGround)
    {
        // Expand cube by radius (Minkowski sum for XZ)
        float expMinX = cube.minX - r;
        float expMaxX = cube.maxX + r;
        float expMinZ = cube.minZ - r;
        float expMaxZ = cube.maxZ + r;

        // FIX: When grounded, use capsule BODY (excluding bottom hemisphere) for Y-overlap.
        // This prevents floor-level hemisphere from causing false wall hits.
        float capMinY, capMaxY;
        if (onGround)
        {
            // Grounded: only check body (above bottom hemisphere)
            capMinY = feetY + r;   // Start at center of bottom sphere
            capMaxY = feetY + 2.0f * r + 2.0f * hh;
        }
        else
        {
            // Airborne: use full capsule height
            capMinY = feetY;
            capMaxY = feetY + 2.0f * r + 2.0f * hh;
        }

        // Y overlap check
        if (capMaxY <= cube.minY || capMinY >= cube.maxY)
            return { false, 1.0f, 0.0f, 0.0f };  // No Y overlap, skip

        // Additional check: if grounded and standing ON TOP of this cube, skip
        // (feet at or above cube top = support, not wall)
        const float SUPPORT_EPS = 0.05f;
        if (onGround && feetY >= cube.maxY - SUPPORT_EPS)
            return { false, 1.0f, 0.0f, 0.0f };

        // Sweep P0 and P1 (both at same XZ since capsule is vertical)
        SweepResult res = SegmentAABBSweepXZ(posX, posZ, dx, dz, expMinX, expMaxX, expMinZ, expMaxZ);
        return res;
    }

    // Clip velocity onto plane (2D XZ version)
    void ClipVelocityXZ(float& dx, float& dz, float normalX, float normalZ)
    {
        const float OVERCLIP = 1.001f;
        float backoff = (dx * normalX + dz * normalZ) * OVERCLIP;
        dx -= normalX * backoff;
        dz -= normalZ * backoff;
        const float STOP_EPS = 0.001f;
        if (fabsf(dx) < STOP_EPS) dx = 0.0f;
        if (fabsf(dz) < STOP_EPS) dz = 0.0f;
    }
}

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

        // Day3.11 Phase 2: Capsule depenetration safety net
        if (m_controllerMode == ControllerMode::Capsule)
        {
            ResolveOverlaps_Capsule();
        }

        // 1. Apply yaw rotation (axis-based)
        m_pawn.yaw += input.yawAxis * m_config.lookSpeed * fixedDt;

        // 2. Apply pitch rotation with clamping
        m_pawn.pitch += input.pitchAxis * m_config.lookSpeed * fixedDt;
        if (m_pawn.pitch < m_config.pitchClampMin) m_pawn.pitch = m_config.pitchClampMin;
        if (m_pawn.pitch > m_config.pitchClampMax) m_pawn.pitch = m_config.pitchClampMax;

        // 3. Compute camera-relative movement vectors (pawnXZ - eyeXZ)
        float camFwdX = m_pawn.posX - m_camera.eyeX;
        float camFwdZ = m_pawn.posZ - m_camera.eyeZ;
        float fwdLen = sqrtf(camFwdX * camFwdX + camFwdZ * camFwdZ);

        // Len guard: fallback to pawn yaw if camera too close
        if (fwdLen < 0.001f)
        {
            camFwdX = sinf(m_pawn.yaw);
            camFwdZ = cosf(m_pawn.yaw);
        }
        else
        {
            camFwdX /= fwdLen;
            camFwdZ /= fwdLen;
        }

        // Right = cross(camFwd, up) where up=(0,1,0): (-fwdZ, 0, fwdX)
        float camRightX = -camFwdZ;
        float camRightZ = camFwdX;

        // Store for HUD proof + orthogonality check
        m_camera.dbgFwdX = camFwdX;
        m_camera.dbgFwdZ = camFwdZ;
        m_camera.dbgRightX = camRightX;
        m_camera.dbgRightZ = camRightZ;
        m_camera.dbgDot = camFwdX * camRightX + camFwdZ * camRightZ;  // Should be ~0

        // 4. Smooth sprint alpha toward target
        float targetSprint = input.sprint ? 1.0f : 0.0f;
        float sprintDelta = (targetSprint - m_sprintAlpha) * m_config.sprintSmoothRate * fixedDt;
        m_sprintAlpha += sprintDelta;
        if (m_sprintAlpha < 0.0f) m_sprintAlpha = 0.0f;
        if (m_sprintAlpha > 1.0f) m_sprintAlpha = 1.0f;

        // 5. Compute velocity from input and sprint
        float speedMultiplier = 1.0f + (m_config.sprintMultiplier - 1.0f) * m_sprintAlpha;
        float currentSpeed = m_config.walkSpeed * speedMultiplier;

        // Horizontal velocity from input (camera-relative)
        m_pawn.velX = (camFwdX * input.moveZ + camRightX * input.moveX) * currentSpeed;
        m_pawn.velZ = (camFwdZ * input.moveZ + camRightZ * input.moveX) * currentSpeed;

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
        float newX, newZ;
        float newY = m_pawn.posY + m_pawn.velY * fixedDt;

        // Day3.11 Phase 3: Capsule XZ uses sweep/slide
        bool capsuleZeroVelX = false, capsuleZeroVelZ = false;
        if (m_controllerMode == ControllerMode::Capsule)
        {
            float reqDx = m_pawn.velX * fixedDt;
            float reqDz = m_pawn.velZ * fixedDt;
            float appliedDx = 0.0f, appliedDz = 0.0f;
            SweepXZ_Capsule(reqDx, reqDz, appliedDx, appliedDz, capsuleZeroVelX, capsuleZeroVelZ);
            newX = m_pawn.posX + appliedDx;
            newZ = m_pawn.posZ + appliedDz;

            // FIX: Post-sweep XZ cleanup (single iteration)
            ResolveXZ_Capsule_Cleanup(newX, newZ, newY);

            // FIX: Higher-level velocity zeroing (after all sweep/slide logic)
            if (capsuleZeroVelX) m_pawn.velX = 0.0f;
            if (capsuleZeroVelZ) m_pawn.velZ = 0.0f;
        }
        else
        {
            newX = m_pawn.posX + m_pawn.velX * fixedDt;
            newZ = m_pawn.posZ + m_pawn.velZ * fixedDt;
        }

        // Day3.9: Store pawn AABB bottom before collision resolution (for anti-step-up)
        AABB pawnPre = BuildPawnAABB(newX, newY, newZ);
        float prevPawnBottom = pawnPre.minY;

        // Day3.4: Iterative collision resolution (X→Z→Y per iteration)
        const int MAX_ITERATIONS = 8;
        const float CONVERGENCE_EPSILON = 0.001f;
        bool converged = false;

        for (int iter = 0; iter < MAX_ITERATIONS; ++iter)
        {
            float totalDelta = 0.0f;

            // Day3.8: MTV-based XZ resolution (AABB mode only)
            if (m_controllerMode == ControllerMode::AABB)
            {
                float prevX = newX, prevZ = newZ;
                ResolveXZ_MTV(newX, newZ, newY);
                totalDelta += fabsf(newX - prevX) + fabsf(newZ - prevZ);
            }
            else if (m_controllerMode == ControllerMode::Capsule)
            {
                // Day3.11: XZ cleanup in iteration loop for Capsule mode
                // ResolveAxis(Y) can push capsule into wall XZ, need cleanup each iteration
                float prevX = newX, prevZ = newZ;
                ResolveXZ_Capsule_Cleanup(newX, newZ, newY);
                totalDelta += fabsf(newX - prevX) + fabsf(newZ - prevZ);
            }

            // Y axis (both modes)
            float prevY = newY;
            ResolveAxis(newY, newX, newY, newZ, Axis::Y, prevPawnBottom);
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
                m_pawn.posX - m_config.pawnHalfExtentX, m_pawn.posX + m_config.pawnHalfExtentX,
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
            m_lastRespawnReason = "KillZ";

            char buf[256];
            sprintf_s(buf, "[KILLZ] #%u at pos=(%.2f,%.2f,%.2f)\n",
                m_respawnCount, m_pawn.posX, m_pawn.posY, m_pawn.posZ);
            OutputDebugStringA(buf);

            RespawnResetControllerState();
        }
    }

    void WorldState::ToggleControllerMode()
    {
        m_controllerMode = (m_controllerMode == ControllerMode::AABB)
                           ? ControllerMode::Capsule : ControllerMode::AABB;
        const char* name = (m_controllerMode == ControllerMode::AABB) ? "AABB" : "Capsule";
        char buf[64];
        sprintf_s(buf, "[MODE] ctrl=%s\n", name);
        OutputDebugStringA(buf);
    }

    void WorldState::RespawnResetControllerState()
    {
        m_pawn.posX = m_config.spawnX;
        m_pawn.posY = m_config.spawnY;
        m_pawn.posZ = m_config.spawnZ;
        m_pawn.velX = m_pawn.velY = m_pawn.velZ = 0.0f;
        m_pawn.onGround = false;
        m_collisionStats = CollisionStats{};

        const char* modeName = (m_controllerMode == ControllerMode::AABB) ? "AABB" : "Capsule";
        char buf[128];
        sprintf_s(buf, "[RESPAWN] ctrl=%s stats_cleared=1 pos=(%.1f,%.1f,%.1f)\n",
                  modeName, m_pawn.posX, m_pawn.posY, m_pawn.posZ);
        OutputDebugStringA(buf);
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
        // Day3.7: Axis-aware footprint (X=1.4 for arms, Z=0.4 tight)
        AABB aabb;
        aabb.minX = px - m_config.pawnHalfExtentX;
        aabb.maxX = px + m_config.pawnHalfExtentX;
        aabb.minY = py;
        aabb.maxY = py + m_config.pawnHeight;
        aabb.minZ = pz - m_config.pawnHalfExtentZ;
        aabb.maxZ = pz + m_config.pawnHalfExtentZ;
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

        // Sign: push pawn away from cube center (negative direction if pawn is left/below)
        float sign = (centerPawn < centerCube) ? -1.0f : 1.0f;

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

    // Day3.8: MTV-based XZ resolution (Issue A fix)
    void WorldState::ResolveXZ_MTV(float& newX, float& newZ, float newY)
    {
        AABB pawn = BuildPawnAABB(newX, newY, newZ);
        auto candidates = QuerySpatialHash(pawn);
        m_collisionStats.candidatesChecked += static_cast<uint32_t>(candidates.size());

        float bestPenX = 0.0f, bestPenZ = 0.0f;
        int bestCubeIdx = -1;
        float bestCenterDiffX = 0.0f, bestCenterDiffZ = 0.0f;

        for (uint16_t cubeIdx : candidates)
        {
            AABB cube = GetCubeAABB(cubeIdx);
            if (!Intersects(pawn, cube)) continue;

            m_collisionStats.contacts++;

            float penX = ComputeSignedPenetration(pawn, cube, Axis::X);
            float penZ = ComputeSignedPenetration(pawn, cube, Axis::Z);

            float centerDiffX = ((pawn.minX + pawn.maxX) - (cube.minX + cube.maxX)) * 0.5f;
            float centerDiffZ = ((pawn.minZ + pawn.maxZ) - (cube.minZ + cube.maxZ)) * 0.5f;

            // Select cube with largest min-axis penetration (deepest contact)
            float minPen = fminf(fabsf(penX), fabsf(penZ));
            float bestMinPen = fminf(fabsf(bestPenX), fabsf(bestPenZ));

            if (minPen > bestMinPen || bestCubeIdx < 0)
            {
                bestPenX = penX;
                bestPenZ = penZ;
                bestCubeIdx = cubeIdx;
                bestCenterDiffX = centerDiffX;
                bestCenterDiffZ = centerDiffZ;
            }
        }

        if (bestCubeIdx >= 0)
        {
            // Store debug info
            m_collisionStats.lastPenX = bestPenX;
            m_collisionStats.lastPenZ = bestPenZ;
            m_collisionStats.centerDiffX = bestCenterDiffX;
            m_collisionStats.centerDiffZ = bestCenterDiffZ;

            // Day3.9: Separable-axis XZ push-out - apply BOTH axes to guarantee separation
            if (bestPenX != 0.0f)
            {
                newX += bestPenX;
                m_pawn.velX = 0.0f;
            }
            if (bestPenZ != 0.0f)
            {
                newZ += bestPenZ;
                m_pawn.velZ = 0.0f;
            }

            // Track dominant axis for HUD
            m_collisionStats.mtvAxis = (fabsf(bestPenX) >= fabsf(bestPenZ)) ? 0 : 2;
            m_collisionStats.mtvMagnitude = fmaxf(fabsf(bestPenX), fabsf(bestPenZ));
            m_collisionStats.lastAxisResolved = (fabsf(bestPenX) >= fabsf(bestPenZ)) ? Axis::X : Axis::Z;

            m_collisionStats.penetrationsResolved++;
            m_collisionStats.lastHitCubeId = bestCubeIdx;

            // Day3.9: Post-resolution proof - verify XZ separation achieved
            AABB pawnAfterXZ = BuildPawnAABB(newX, newY, newZ);
            AABB cubeCheck = GetCubeAABB(static_cast<uint16_t>(bestCubeIdx));
            m_collisionStats.xzStillOverlapping = Intersects(pawnAfterXZ, cubeCheck);

            if (fabsf(bestPenX) > m_collisionStats.maxPenetrationAbs)
                m_collisionStats.maxPenetrationAbs = fabsf(bestPenX);
            if (fabsf(bestPenZ) > m_collisionStats.maxPenetrationAbs)
                m_collisionStats.maxPenetrationAbs = fabsf(bestPenZ);
        }
    }

    void WorldState::ResolveAxis(float& posAxis, float currentPosX, float currentPosY, float currentPosZ, Axis axis, float prevPawnBottom)
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

            // Day3.9: Anti-step-up guard for Y axis
            if (axis == Axis::Y)
            {
                float cubeTop = cube.maxY;

                // Compute what the Y delta would be
                float penY = ComputeSignedPenetration(pawn, cube, Axis::Y);
                float deltaY = penY;  // Penetration is already signed correctly

                // wouldPushUp = the correction would move pawn upward
                bool wouldPushUp = (deltaY > 0.0f);

                // Only allow upward correction if truly landing from above:
                // - prevPawnBottom (AABB minY before collision) was at or above cubeTop
                // - AND velocity is downward or zero (falling/landing)
                bool wasAboveTop = (prevPawnBottom >= cubeTop - 0.01f);
                bool fallingOrLanding = (m_pawn.velY <= 0.0f);
                bool isLandingFromAbove = wasAboveTop && fallingOrLanding;

                if (wouldPushUp && !isLandingFromAbove)
                {
                    // Wall contact trying to push up - skip this cube for Y
                    m_collisionStats.yStepUpSkipped = true;
                    continue;
                }
            }

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
            posAxis += deepestPen;

            if (axis == Axis::Y)
            {
                m_collisionStats.yDeltaApplied = deepestPen;  // Signed penetration = signed delta
            }

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

    // Day3.11 Phase 2: Capsule depenetration safety net
    void WorldState::ResolveOverlaps_Capsule()
    {
        const int MAX_DEPEN_ITERS = 4;
        const float MIN_DEPEN_DIST = 0.001f;
        const float MAX_DEPEN_CLAMP = 1.0f;
        const float MAX_TOTAL_CLAMP = 2.0f;

        // Reset depen stats (these are protected from overwrite per contract)
        m_collisionStats.depenApplied = false;
        m_collisionStats.depenTotalMag = 0.0f;
        m_collisionStats.depenClampTriggered = false;
        m_collisionStats.depenMaxSingleMag = 0.0f;
        m_collisionStats.depenOverlapCount = 0;
        m_collisionStats.depenIterations = 0;

        float r = m_config.capsuleRadius;
        float hh = m_config.capsuleHalfHeight;

        for (int iter = 0; iter < MAX_DEPEN_ITERS; ++iter)
        {
            m_collisionStats.depenIterations = static_cast<uint32_t>(iter + 1);

            AABB capAABB;
            capAABB.minX = m_pawn.posX - r;  capAABB.maxX = m_pawn.posX + r;
            capAABB.minY = m_pawn.posY;       capAABB.maxY = m_pawn.posY + 2.0f * r + 2.0f * hh;
            capAABB.minZ = m_pawn.posZ - r;  capAABB.maxZ = m_pawn.posZ + r;

            std::vector<uint16_t> candidates = QuerySpatialHash(capAABB);
            std::sort(candidates.begin(), candidates.end());
            candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

            float pushX = 0.0f, pushY = 0.0f, pushZ = 0.0f;
            uint32_t overlapCount = 0;

            for (uint16_t idx : candidates)
            {
                AABB cube = GetCubeAABB(idx);
                CapsuleOverlapResult ov = CapsuleAABBOverlap(m_pawn.posY, m_pawn.posX, m_pawn.posZ, r, hh, cube);
                if (ov.hit && ov.depth > MIN_DEPEN_DIST)
                {
                    overlapCount++;
                    float clampedD = (ov.depth > MAX_DEPEN_CLAMP) ? MAX_DEPEN_CLAMP : ov.depth;
                    if (ov.depth > MAX_DEPEN_CLAMP) m_collisionStats.depenClampTriggered = true;
                    if (clampedD > m_collisionStats.depenMaxSingleMag) m_collisionStats.depenMaxSingleMag = clampedD;
                    pushX += ov.normal.x * clampedD;
                    pushY += ov.normal.y * clampedD;
                    pushZ += ov.normal.z * clampedD;
                }
            }
            m_collisionStats.depenOverlapCount = overlapCount;
            if (overlapCount == 0) break;

            float mag = sqrtf(pushX * pushX + pushY * pushY + pushZ * pushZ);
            if (mag < MIN_DEPEN_DIST) break;  // Negligible push, stop

            if (mag > MAX_TOTAL_CLAMP) {
                float s = MAX_TOTAL_CLAMP / mag;
                pushX *= s; pushY *= s; pushZ *= s;
                mag = MAX_TOTAL_CLAMP;
                m_collisionStats.depenClampTriggered = true;
            }

            m_pawn.posX += pushX;
            m_pawn.posY += pushY;
            m_pawn.posZ += pushZ;
            m_collisionStats.depenApplied = true;
            m_collisionStats.depenTotalMag += mag;

            char buf[128];
            sprintf_s(buf, "[DEPEN] iter=%d mag=%.4f clamp=%d cnt=%u\n",
                iter, mag, m_collisionStats.depenClampTriggered ? 1 : 0, overlapCount);
            OutputDebugStringA(buf);
        }

        if (m_collisionStats.depenApplied)
        {
            m_pawn.onGround = false;  // Avoid stale state
            char buf[128];
            sprintf_s(buf, "[DEPEN] DONE iters=%u total=%.4f clamp=%d pos=(%.2f,%.2f,%.2f)\n",
                m_collisionStats.depenIterations, m_collisionStats.depenTotalMag,
                m_collisionStats.depenClampTriggered ? 1 : 0,
                m_pawn.posX, m_pawn.posY, m_pawn.posZ);
            OutputDebugStringA(buf);
        }
    }

    // Day3.11 Phase 3: Capsule XZ sweep/slide
    // FIX: Added outZeroVelX/Z flags to defer velocity zeroing to caller
    void WorldState::SweepXZ_Capsule(float reqDx, float reqDz, float& outAppliedDx, float& outAppliedDz,
                                     bool& outZeroVelX, bool& outZeroVelZ)
    {
        const float SKIN_WIDTH = 0.01f;
        const int MAX_SWEEPS = 4;  // FIX: Corner contacts need 3+ iterations to fully resolve

        float r = m_config.capsuleRadius;
        float hh = m_config.capsuleHalfHeight;
        float feetY = m_pawn.posY;
        bool onGround = m_pawn.onGround;

        // Initialize output flags
        outZeroVelX = false;
        outZeroVelZ = false;

        // Reset sweep stats
        m_collisionStats.sweepHit = false;
        m_collisionStats.sweepTOI = 1.0f;
        m_collisionStats.sweepHitCubeIdx = -1;
        m_collisionStats.sweepCandCount = 0;
        m_collisionStats.sweepReqDx = reqDx;
        m_collisionStats.sweepReqDz = reqDz;
        m_collisionStats.sweepAppliedDx = 0.0f;
        m_collisionStats.sweepAppliedDz = 0.0f;
        m_collisionStats.sweepSlideDx = 0.0f;
        m_collisionStats.sweepSlideDz = 0.0f;
        m_collisionStats.sweepNormalX = 0.0f;
        m_collisionStats.sweepNormalZ = 0.0f;

        float dx = reqDx, dz = reqDz;
        float totalAppliedDx = 0.0f, totalAppliedDz = 0.0f;

        for (int sweep = 0; sweep < MAX_SWEEPS; ++sweep)
        {
            float deltaMag = sqrtf(dx * dx + dz * dz);
            if (deltaMag < 0.0001f) break;

            // Build swept AABB for broadphase
            float curX = m_pawn.posX + totalAppliedDx;
            float curZ = m_pawn.posZ + totalAppliedDz;
            AABB sweptAABB;
            sweptAABB.minX = fminf(curX - r, curX - r + dx);
            sweptAABB.maxX = fmaxf(curX + r, curX + r + dx);
            sweptAABB.minY = feetY;
            sweptAABB.maxY = feetY + 2.0f * r + 2.0f * hh;
            sweptAABB.minZ = fminf(curZ - r, curZ - r + dz);
            sweptAABB.maxZ = fmaxf(curZ + r, curZ + r + dz);

            std::vector<uint16_t> candidates = QuerySpatialHash(sweptAABB);
            std::sort(candidates.begin(), candidates.end());
            candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
            m_collisionStats.sweepCandCount = static_cast<uint32_t>(candidates.size());

            // Find earliest hit
            SweepResult earliest = { false, 1.0f, 0.0f, 0.0f };
            int earliestCubeIdx = -1;

            for (uint16_t cubeIdx : candidates)
            {
                AABB cube = GetCubeAABB(cubeIdx);
                SweepResult hit = SweepCapsuleVsCubeXZ(curX, curZ, feetY, r, hh, dx, dz, cube, onGround);
                if (hit.hit)
                {
                    if (hit.t < earliest.t || (!earliest.hit))
                    {
                        earliest = hit;
                        earliestCubeIdx = cubeIdx;
                    }
                    else if (fabsf(hit.t - earliest.t) < 1e-6f && cubeIdx < earliestCubeIdx)
                    {
                        earliest = hit;
                        earliestCubeIdx = cubeIdx;  // Tie-break by ID for determinism
                    }
                }
            }

            if (!earliest.hit)
            {
                // No collision, apply full remaining delta
                totalAppliedDx += dx;
                totalAppliedDz += dz;

                if (sweep == 0)
                {
                    char buf[128];
                    sprintf_s(buf, "[SWEEP] req=(%.3f,%.3f) cand=%u hit=0\n", reqDx, reqDz, m_collisionStats.sweepCandCount);
                    OutputDebugStringA(buf);
                }
                break;
            }

            // Hit detected
            m_collisionStats.sweepHit = true;
            m_collisionStats.sweepTOI = earliest.t;
            m_collisionStats.sweepHitCubeIdx = earliestCubeIdx;
            m_collisionStats.sweepNormalX = earliest.normalX;
            m_collisionStats.sweepNormalZ = earliest.normalZ;

            // Move to contact (with skin offset)
            // FIX: Clamp skin offset to never eat more than 50% of TOI
            // Prevents safeT=0 trap when TOI is small relative to SKIN_WIDTH/deltaMag
            float skinParam = SKIN_WIDTH / deltaMag;
            float clampedSkin = fminf(skinParam, earliest.t * 0.5f);
            float safeT = fmaxf(0.0f, earliest.t - clampedSkin);
            totalAppliedDx += dx * safeT;
            totalAppliedDz += dz * safeT;

            char buf[160];
            sprintf_s(buf, "[SWEEP] req=(%.3f,%.3f) cand=%u hit=1 toi=%.4f n=(%.2f,%.2f) cube=%d\n",
                reqDx, reqDz, m_collisionStats.sweepCandCount, earliest.t, earliest.normalX, earliest.normalZ, earliestCubeIdx);
            OutputDebugStringA(buf);

            // Compute slide for remaining motion
            float remainT = 1.0f - safeT;
            float remDx = dx * remainT;
            float remDz = dz * remainT;

            ClipVelocityXZ(remDx, remDz, earliest.normalX, earliest.normalZ);

            m_collisionStats.sweepSlideDx = remDx;
            m_collisionStats.sweepSlideDz = remDz;

            if (sweep == 0)
            {
                char slideBuf[128];
                sprintf_s(slideBuf, "[SLIDE] rem=(%.3f,%.3f) slide=(%.3f,%.3f)\n",
                    dx * remainT, dz * remainT, remDx, remDz);
                OutputDebugStringA(slideBuf);
            }

            // Continue with slide delta for next sweep iteration
            dx = remDx;
            dz = remDz;

            // FIX: Set flags for caller to zero velocity (instead of zeroing here)
            if (earliest.normalX != 0.0f) outZeroVelX = true;
            if (earliest.normalZ != 0.0f) outZeroVelZ = true;
        }

        m_collisionStats.sweepAppliedDx = totalAppliedDx;
        m_collisionStats.sweepAppliedDz = totalAppliedDz;
        outAppliedDx = totalAppliedDx;
        outAppliedDz = totalAppliedDz;
    }

    // Day3.11 Phase 3 Fix: XZ-only cleanup pass for residual penetrations
    void WorldState::ResolveXZ_Capsule_Cleanup(float& newX, float& newZ, float newY)
    {
        const float MAX_XZ_CLEANUP = 1.0f;  // Must cover capsuleRadius (0.8f) + margin
        const float MIN_CLEANUP_DIST = 0.001f;

        float r = m_config.capsuleRadius;
        float hh = m_config.capsuleHalfHeight;

        AABB capAABB;
        capAABB.minX = newX - r;  capAABB.maxX = newX + r;
        capAABB.minY = newY;       capAABB.maxY = newY + 2.0f * r + 2.0f * hh;
        capAABB.minZ = newZ - r;  capAABB.maxZ = newZ + r;

        std::vector<uint16_t> candidates = QuerySpatialHash(capAABB);
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

        float pushX = 0.0f, pushZ = 0.0f;

        for (uint16_t idx : candidates)
        {
            AABB cube = GetCubeAABB(idx);
            CapsuleOverlapResult ov = CapsuleAABBOverlap(newY, newX, newZ, r, hh, cube);
            if (ov.hit && ov.depth > MIN_CLEANUP_DIST)
            {
                char buf[160];
                sprintf_s(buf, "[CLEANUP_CUBE] idx=%d depth=%.4f n=(%.3f,%.3f,%.3f)\n",
                    idx, ov.depth, ov.normal.x, ov.normal.y, ov.normal.z);
                OutputDebugStringA(buf);
                // XZ components only
                pushX += ov.normal.x * ov.depth;
                pushZ += ov.normal.z * ov.depth;
            }
        }

        float mag = sqrtf(pushX * pushX + pushZ * pushZ);
        if (mag > MAX_XZ_CLEANUP)
        {
            float s = MAX_XZ_CLEANUP / mag;
            pushX *= s;
            pushZ *= s;
        }

        if (mag > MIN_CLEANUP_DIST)
        {
            newX += pushX;
            newZ += pushZ;
            char buf[96];
            sprintf_s(buf, "[XZ_CLEANUP] push=(%.4f,%.4f)\n", pushX, pushZ);
            OutputDebugStringA(buf);
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

        // Day3.7: Camera basis proof (Bug A)
        snap.camFwdX = m_camera.dbgFwdX;
        snap.camFwdZ = m_camera.dbgFwdZ;
        snap.camRightX = m_camera.dbgRightX;
        snap.camRightZ = m_camera.dbgRightZ;
        snap.camDot = m_camera.dbgDot;

        // Day3.7: Collision extent proof (Bug C)
        snap.pawnExtentX = m_config.pawnHalfExtentX;
        snap.pawnExtentZ = m_config.pawnHalfExtentZ;

        // Day3.8: MTV debug fields
        snap.mtvPenX = m_collisionStats.lastPenX;
        snap.mtvPenZ = m_collisionStats.lastPenZ;
        snap.mtvAxis = m_collisionStats.mtvAxis;
        snap.mtvMagnitude = m_collisionStats.mtvMagnitude;
        snap.mtvCenterDiffX = m_collisionStats.centerDiffX;
        snap.mtvCenterDiffZ = m_collisionStats.centerDiffZ;

        // Day3.9: Regression debug
        snap.xzStillOverlapping = m_collisionStats.xzStillOverlapping;
        snap.yStepUpSkipped = m_collisionStats.yStepUpSkipped;
        snap.yDeltaApplied = m_collisionStats.yDeltaApplied;

        // Day3.11: Controller mode
        snap.controllerMode = static_cast<uint8_t>(m_controllerMode);

        // Day3.11: Capsule geometry
        snap.capsuleRadius = m_config.capsuleRadius;
        snap.capsuleHalfHeight = m_config.capsuleHalfHeight;
        CapsulePoints cap = MakeCapsuleFromFeet(m_pawn.posY, m_config.capsuleRadius, m_config.capsuleHalfHeight);
        snap.capsuleP0y = cap.P0y;
        snap.capsuleP1y = cap.P1y;

        // Day3.11 Phase 2: Capsule depenetration
        snap.depenApplied = m_collisionStats.depenApplied;
        snap.depenTotalMag = m_collisionStats.depenTotalMag;
        snap.depenClampTriggered = m_collisionStats.depenClampTriggered;
        snap.depenMaxSingleMag = m_collisionStats.depenMaxSingleMag;
        snap.depenOverlapCount = m_collisionStats.depenOverlapCount;
        snap.depenIterations = m_collisionStats.depenIterations;

        // Day3.11 Phase 3: Capsule sweep
        snap.sweepHit = m_collisionStats.sweepHit;
        snap.sweepTOI = m_collisionStats.sweepTOI;
        snap.sweepHitCubeIdx = m_collisionStats.sweepHitCubeIdx;
        snap.sweepCandCount = m_collisionStats.sweepCandCount;
        snap.sweepReqDx = m_collisionStats.sweepReqDx;
        snap.sweepReqDz = m_collisionStats.sweepReqDz;
        snap.sweepAppliedDx = m_collisionStats.sweepAppliedDx;
        snap.sweepAppliedDz = m_collisionStats.sweepAppliedDz;
        snap.sweepSlideDx = m_collisionStats.sweepSlideDx;
        snap.sweepSlideDz = m_collisionStats.sweepSlideDz;
        snap.sweepNormalX = m_collisionStats.sweepNormalX;
        snap.sweepNormalZ = m_collisionStats.sweepNormalZ;

        return snap;
    }
}
