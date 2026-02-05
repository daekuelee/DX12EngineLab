#include "WorldState.h"
#include "Collision/CapsuleMovement.h"
#include "../Renderer/DX12/Dx12Context.h"  // For HUDSnapshot
#include <cmath>
#include <cstdio>
#include <algorithm>  // For std::sort, std::unique
#include <Windows.h>  // For OutputDebugStringA

using namespace DirectX;

namespace Engine
{

// PR2.8: SceneView adapter — bridges WorldState private spatial data to module
class WorldStateSceneAdapter : public Collision::SceneView {
    const WorldState& m_ws;
public:
    explicit WorldStateSceneAdapter(const WorldState& ws) : m_ws(ws) {}
    std::vector<uint16_t> QueryCandidates(const AABB& box) const override {
        return m_ws.QuerySpatialHash(box);
    }
    AABB GetCubeAABB(uint16_t idx) const override {
        return m_ws.GetCubeAABB(idx);
    }
};
    void WorldState::Initialize()
    {
        // Reset pawn to spawn position
        m_pawn = PawnState{};
        m_pawn.posX = m_config.spawnX;
        m_pawn.posY = m_config.spawnY;
        m_pawn.posZ = m_config.spawnZ;
        m_view.yaw = 0.0f;
        m_view.pitch = 0.0f;
        m_pawn.onGround = false;  // Start in air, floor resolve will set true

        // Initialize camera at offset from pawn
        m_renderCam.eyeX = m_pawn.posX;
        m_renderCam.eyeY = m_pawn.posY + m_config.camOffsetUp;
        m_renderCam.eyeZ = m_pawn.posZ - m_config.camOffsetBehind;
        m_renderCam.fovY = m_config.baseFovY;

        m_sprintAlpha = 0.0f;
        m_jumpConsumedThisFrame = false;
        m_jumpQueued = false;

        // Reset respawn tracking
        m_respawnCount = 0;
        m_lastRespawnReason = nullptr;

        // Part 2: Build spatial grid for cube collision
        BuildSpatialGrid();

        // Day3.12: Mutual exclusion - StepUpGridTest overrides T1/T2/T3 fixtures
        if (m_config.enableStepUpGridTest)
        {
            BuildStepUpGridTest();
            m_stepGridWasEverEnabled = true;
        }
        else if (m_config.enableStepUpTestFixtures)
        {
            // Compute grid indices from world coords: idx = gz*100 + gx, where gx=(x+99)/2
            auto worldToIdx = [](float wx, float wz) -> uint16_t {
                int gx = static_cast<int>((wx + 99.0f) / 2.0f);
                int gz = static_cast<int>((wz + 99.0f) / 2.0f);
                return static_cast<uint16_t>(gz * 100 + gx);
            };

            m_fixtureT1Idx = worldToIdx(5.0f, 9.0f);      // 5452
            m_fixtureT2Idx = worldToIdx(9.0f, 9.0f);      // 5454
            m_fixtureT3StepIdx = worldToIdx(15.0f, 9.0f); // 5457

            // Build extras (ceiling only)
            BuildExtraFixtures();

            // Log fixture info (collision Y = cubeMaxY + stepHeight)
            char buf[256];
            sprintf_s(buf, "[FIXTURE] T1_STEP gridIdx=%u world=(5,0,9) AABB Y=[0,3.3]\n", m_fixtureT1Idx);
            OutputDebugStringA(buf);
            sprintf_s(buf, "[FIXTURE] T2_WALL gridIdx=%u world=(9,0,9) AABB Y=[0,3.6]\n", m_fixtureT2Idx);
            OutputDebugStringA(buf);
            sprintf_s(buf, "[FIXTURE] T3_STEP gridIdx=%u world=(15,0,9) AABB Y=[0,3.5]\n", m_fixtureT3StepIdx);
            OutputDebugStringA(buf);
        }

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

    //-------------------------------------------------------------------------
    // C-2: Presentation-only look offset implementation
    //-------------------------------------------------------------------------
    void WorldState::SetPresentationLookOffset(float yawRad, float pitchRad)
    {
        m_presentationYawOffset = yawRad;
        m_presentationPitchOffset = pitchRad;
    }

    void WorldState::ClearPresentationLookOffset()
    {
        m_presentationYawOffset = 0.0f;
        m_presentationPitchOffset = 0.0f;
    }

    void WorldState::TickFixed(const InputState& input, float fixedDt)
    {
        // Reset collision stats for this tick
        m_collisionStats = CollisionStats{};

        // --- Pre-solver depenetration (PR2.8: module call) ---
        {
            WorldStateSceneAdapter sceneView(*this);
            Collision::CapsuleGeom geom = {
                m_config.capsuleRadius, m_config.capsuleHalfHeight,
                m_config.pawnHalfExtentX, m_config.pawnHalfExtentZ, m_config.pawnHeight
            };
            auto depen = Collision::DepenetrateInPlace(
                sceneView, geom, m_pawn.posX, m_pawn.posY, m_pawn.posZ, m_pawn.onGround);
            m_pawn.posX = depen.posX;
            m_pawn.posY = depen.posY;
            m_pawn.posZ = depen.posZ;
            m_pawn.onGround = depen.onGround;
            m_collisionStats.depenApplied = depen.depenApplied;
            m_collisionStats.depenTotalMag = depen.depenTotalMag;
            m_collisionStats.depenClampTriggered = depen.depenClampTriggered;
            m_collisionStats.depenMaxSingleMag = depen.depenMaxSingleMag;
            m_collisionStats.depenOverlapCount = depen.depenOverlapCount;
            m_collisionStats.depenIterations = depen.depenIterations;
        }

        // 1. Apply yaw rotation [LOOK-UNIFIED] pre-computed delta from Action layer
        m_view.yaw += input.yawDelta;

        // 2. Apply pitch rotation with clamping [LOOK-UNIFIED]
        m_view.pitch += input.pitchDelta;
        if (m_view.pitch < m_config.pitchClampMin) m_view.pitch = m_config.pitchClampMin;
        if (m_view.pitch > m_config.pitchClampMax) m_view.pitch = m_config.pitchClampMax;

        // 3. Compute movement basis from sim yaw [SIM-PURE]
        // CONTRACT: TickFixed must NEVER read m_renderCam or any presentation state.
        // Movement direction is derived purely from m_view.yaw (sim-owned, written at L342).
        float camFwdX = sinf(m_view.yaw);
        float camFwdZ = cosf(m_view.yaw);

        // Right = cross(camFwd, up) where up=(0,1,0): (-fwdZ, 0, fwdX)
        float camRightX = -camFwdZ;
        float camRightZ = camFwdX;

        // Store for HUD proof + orthogonality check (Day4: now in MovementBasisDebug)
        m_movementBasis.fwdX = camFwdX;
        m_movementBasis.fwdZ = camFwdZ;
        m_movementBasis.rightX = camRightX;
        m_movementBasis.rightZ = camRightZ;
        m_movementBasis.dot = camFwdX * camRightX + camFwdZ * camRightZ;  // Should be ~0

#if defined(_DEBUG)
        // [PROOF-SIM-PURE] Throttled proof: basis derived from sim yaw, orthogonality check
        static uint32_t s_basisLogCounter = 0;
        if (++s_basisLogCounter % 600 == 0)
        {
            char buf[128];
            sprintf_s(buf, "[PROOF-SIM-PURE] basisYaw=%.3f fwd=(%.3f,%.3f) dot=%.6f\n",
                m_view.yaw, camFwdX, camFwdZ, m_movementBasis.dot);
            OutputDebugStringA(buf);
        }
#endif

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

        // --- Main collision solver (PR2.8: module call) ---
        {
            WorldStateSceneAdapter sceneView(*this);
            Collision::CapsuleMoveRequest req = {};
            req.posX = m_pawn.posX;  req.posY = m_pawn.posY;  req.posZ = m_pawn.posZ;
            req.velX = m_pawn.velX;  req.velY = m_pawn.velY;  req.velZ = m_pawn.velZ;
            req.onGround = m_pawn.onGround;
            req.justJumped = m_justJumpedThisTick;
            req.fixedDt = fixedDt;
            req.geom = { m_config.capsuleRadius, m_config.capsuleHalfHeight,
                         m_config.pawnHalfExtentX, m_config.pawnHalfExtentZ, m_config.pawnHeight };
            req.enableYSweep = m_config.enableYSweep;
            req.enableStepUp = m_config.enableStepUp;
            req.maxStepHeight = m_config.maxStepHeight;
            req.sweepSkinY = m_config.sweepSkinY;
            req.floor = { m_config.floorY, m_config.floorMinX, m_config.floorMaxX,
                          m_config.floorMinZ, m_config.floorMaxZ };
            req.cubeHalfXZ = m_config.cubeHalfXZ;
            req.cubeMinY = m_config.cubeMinY;
            req.cubeMaxY = m_config.cubeMaxY;

            auto result = Collision::MoveCapsuleKinematic(sceneView, req, m_collisionStats);

#if defined(_DEBUG)
            if (m_config.enableYSweep)
            {
                // PR2.9: Compare MoveCapsuleKinematic vs WithAxisY (legacy path)
                CollisionStats legacyStats = {};
                auto legacyResult = Collision::SolveCapsuleMovement_WithAxisY(
                    sceneView, req, legacyStats);

                constexpr float POS_EPS = 1e-4f;
                constexpr float VEL_EPS = 0.01f;

                float posDiff = fabsf(result.posX - legacyResult.posX)
                              + fabsf(result.posY - legacyResult.posY)
                              + fabsf(result.posZ - legacyResult.posZ);
                float velDiff = fabsf(result.velX - legacyResult.velX)
                              + fabsf(result.velY - legacyResult.velY)
                              + fabsf(result.velZ - legacyResult.velZ);
                bool groundDiff = (result.onGround != legacyResult.onGround);

                if (posDiff > POS_EPS || velDiff > VEL_EPS || groundDiff)
                {
                    char buf[320];
                    sprintf_s(buf,
                        "[LEGACY_COMPARE_DIFF] pos=(%.4f,%.4f,%.4f)vs(%.4f,%.4f,%.4f) "
                        "vel=(%.3f,%.3f,%.3f)vs(%.3f,%.3f,%.3f) gnd=%d/%d\n",
                        result.posX, result.posY, result.posZ,
                        legacyResult.posX, legacyResult.posY, legacyResult.posZ,
                        result.velX, result.velY, result.velZ,
                        legacyResult.velX, legacyResult.velY, legacyResult.velZ,
                        result.onGround?1:0, legacyResult.onGround?1:0);
                    OutputDebugStringA(buf);
                }

                // Throttled summary every 600 ticks (10s @ 60Hz)
                static uint32_t s_compareCount = 0;
                static float s_maxPosDiff = 0.0f, s_maxVelDiff = 0.0f;
                static uint32_t s_diffCount = 0;
                s_compareCount++;
                if (posDiff > s_maxPosDiff) s_maxPosDiff = posDiff;
                if (velDiff > s_maxVelDiff) s_maxVelDiff = velDiff;
                if (posDiff > POS_EPS || velDiff > VEL_EPS || groundDiff) s_diffCount++;
                if (s_compareCount % 600 == 0)
                {
                    char buf[200];
                    sprintf_s(buf, "[LEGACY_COMPARE] maxPos=%.6f maxVel=%.4f diffs=%u total=%u\n",
                        s_maxPosDiff, s_maxVelDiff, s_diffCount, s_compareCount);
                    OutputDebugStringA(buf);
                    s_maxPosDiff = 0.0f; s_maxVelDiff = 0.0f; s_diffCount = 0;
                }
            }
#endif

            m_pawn.posX = result.posX;
            m_pawn.posY = result.posY;
            m_pawn.posZ = result.posZ;
            m_pawn.velX = result.velX;
            m_pawn.velY = result.velY;
            m_pawn.velZ = result.velZ;
            m_pawn.onGround = result.onGround;
            m_didFloorClampThisTick = m_collisionStats.snappedThisTick;
        }

        m_justJumpedThisTick = false;  // Reset for next tick

        // Legacy floor collision (now logging-only)
        ResolveFloorCollision();

        // 10. KillZ check (respawn if below threshold)
        CheckKillZ();

        // Day3.12 StepUpGridTest: Log on state change only
        if (m_config.enableStepUpGridTest)
        {
            static bool s_prevTry = false;
            static bool s_prevOk = false;
            static uint8_t s_prevMask = 0;

            bool changed = (m_collisionStats.stepTry != s_prevTry) ||
                           (m_collisionStats.stepSuccess != s_prevOk) ||
                           (m_collisionStats.stepFailMask != s_prevMask);

            if (changed)
            {
                char buf[256];
                sprintf_s(buf, "[STEP_GRID] pos=(%.2f,%.2f,%.2f) gnd=%d hit=%d try=%d ok=%d mask=0x%02X h=%.3f\n",
                    m_pawn.posX, m_pawn.posY, m_pawn.posZ,
                    m_pawn.onGround ? 1 : 0,
                    m_collisionStats.sweepHit ? 1 : 0,
                    m_collisionStats.stepTry ? 1 : 0,
                    m_collisionStats.stepSuccess ? 1 : 0,
                    m_collisionStats.stepFailMask,
                    m_collisionStats.stepHeightUsed);
                OutputDebugStringA(buf);

                s_prevTry = m_collisionStats.stepTry;
                s_prevOk = m_collisionStats.stepSuccess;
                s_prevMask = m_collisionStats.stepFailMask;
            }
        }
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

    void WorldState::ToggleStepUpGridTest()
    {
        bool newValue = !m_config.enableStepUpGridTest;

        // Clear existing extras from spatial grid before switching modes
        ClearExtrasFromSpatialGrid();

        if (newValue)
        {
            // Toggle ON: Build stair grid test
            m_config.enableStepUpGridTest = true;
            BuildStepUpGridTest();
            m_stepGridWasEverEnabled = true;
            OutputDebugStringA("[STEP_GRID] Toggle => 1 (stairs built)\n");
        }
        else
        {
            // Toggle OFF: Rebuild fixtures mode (T1/T2/T3 + ceiling)
            m_config.enableStepUpGridTest = false;
            if (m_config.enableStepUpTestFixtures)
            {
                BuildExtraFixtures();
                OutputDebugStringA("[STEP_GRID] Toggle => 0 (fixtures rebuilt)\n");
            }
            else
            {
                OutputDebugStringA("[STEP_GRID] Toggle => 0 (no fixtures)\n");
            }
        }

        // MODE_SNAPSHOT: Log exact state after toggle
        char snap[256];
        sprintf_s(snap, "[MODE_SNAPSHOT] fixtures=%d gridTest=%d stepUp=%d extrasCount=%zu\n",
            m_config.enableStepUpTestFixtures ? 1 : 0,
            m_config.enableStepUpGridTest ? 1 : 0,
            m_config.enableStepUp ? 1 : 0,
            m_extras.size());
        OutputDebugStringA(snap);
    }

    void WorldState::RespawnResetControllerState()
    {
        m_pawn.posX = m_config.spawnX;
        m_pawn.posY = m_config.spawnY;
        m_pawn.posZ = m_config.spawnZ;
        m_pawn.velX = m_pawn.velY = m_pawn.velZ = 0.0f;
        m_pawn.onGround = false;
        m_collisionStats = CollisionStats{};

        char buf[128];
        sprintf_s(buf, "[RESPAWN] ctrl=Capsule stats_cleared=1 pos=(%.1f,%.1f,%.1f)\n",
                  m_pawn.posX, m_pawn.posY, m_pawn.posZ);
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

    void WorldState::RegisterAABBToSpatialGrid(uint16_t id, const AABB& aabb)
    {
        int minCX = WorldToCellX(aabb.minX);
        int maxCX = WorldToCellX(aabb.maxX);
        int minCZ = WorldToCellZ(aabb.minZ);
        int maxCZ = WorldToCellZ(aabb.maxZ);

        for (int gz = minCZ; gz <= maxCZ; ++gz)
            for (int gx = minCX; gx <= maxCX; ++gx)
                m_spatialGrid[gz][gx].push_back(id);
    }

    void WorldState::ClearExtrasFromSpatialGrid()
    {
        // Remove all IDs >= EXTRA_BASE from spatial grid
        for (int gz = 0; gz < GRID_SIZE; ++gz)
        {
            for (int gx = 0; gx < GRID_SIZE; ++gx)
            {
                auto& cell = m_spatialGrid[gz][gx];
                cell.erase(
                    std::remove_if(cell.begin(), cell.end(),
                        [](uint16_t id) { return id >= EXTRA_BASE; }),
                    cell.end());
            }
        }
        m_extras.clear();
        OutputDebugStringA("[SPATIAL] Cleared all extras from grid\n");
    }

    void WorldState::BuildExtraFixtures()
    {
        m_extras.clear();
        // No extra fixtures in fixture mode (T1/T2/T3 are grid cubes, not extras)
    }

    void WorldState::BuildStepUpGridTest()
    {
        const float TREAD = 0.60f;
        const float WIDTH_H = 1.0f;
        const int STEPS = 5;
        const float BASE_Y = 3.0f;

        // Clear any existing extras (e.g., T3_CEIL from fixtures mode)
        m_extras.clear();

        // Capacity check (5 stairs * 5 steps = 25)
        if (25 > MAX_EXTRA_COLLIDERS) {
            OutputDebugStringA("[STEP_GRID] ERROR: Exceeds MAX_EXTRA_COLLIDERS!\n");
            return;
        }

        struct StairSpec { float ox, oz; bool dirX; float riser; const char* name; };
        StairSpec stairs[] = {
            {  5.0f,  5.0f, true,  0.20f, "A_Valid_X" },
            {  5.0f, 15.0f, false, 0.25f, "B_Valid_Z" },
            { 15.0f,  5.0f, true,  0.35f, "C_TooTall_X" },
            { 15.0f, 15.0f, false, 0.40f, "D_TooTall_Z" },
            { 25.0f, 10.0f, true,  0.20f, "E_X" },
        };

        char buf[256];
        OutputDebugStringA("[STEP_GRID] === Building Stair Grid ===\n");

        for (const auto& s : stairs)
        {
            for (int i = 0; i < STEPS; ++i)
            {
                ExtraCollider ec;
                ec.type = ExtraColliderType::AABB;

                if (s.dirX) {
                    ec.aabb.minX = s.ox + i * TREAD;
                    ec.aabb.maxX = s.ox + (i + 1) * TREAD;
                    ec.aabb.minZ = s.oz - WIDTH_H;
                    ec.aabb.maxZ = s.oz + WIDTH_H;
                } else {
                    ec.aabb.minX = s.ox - WIDTH_H;
                    ec.aabb.maxX = s.ox + WIDTH_H;
                    ec.aabb.minZ = s.oz + i * TREAD;
                    ec.aabb.maxZ = s.oz + (i + 1) * TREAD;
                }
                ec.aabb.minY = BASE_Y;
                ec.aabb.maxY = BASE_Y + (i + 1) * s.riser;

                uint16_t id = static_cast<uint16_t>(EXTRA_BASE + m_extras.size());
                m_extras.push_back(ec);
                RegisterAABBToSpatialGrid(id, ec.aabb);

                sprintf_s(buf, "[STEP_GRID] %s step=%d id=%u Y=[%.2f,%.2f]\n",
                    s.name, i, id, ec.aabb.minY, ec.aabb.maxY);
                OutputDebugStringA(buf);
            }
        }

        sprintf_s(buf, "[STEP_GRID] Total extras=%zu\n", m_extras.size());
        OutputDebugStringA(buf);
    }

    AABB WorldState::GetCubeAABB(uint16_t cubeIdx) const
    {
        // Day3.12 Phase 4B+: Extra colliders (ID >= EXTRA_BASE)
        if (cubeIdx >= EXTRA_BASE)
        {
            size_t idx = cubeIdx - EXTRA_BASE;
            if (idx < m_extras.size())
                return m_extras[idx].aabb;
            return {};  // Invalid extra ID
        }

        // Cube at grid (gx, gz):
        int gx = cubeIdx % GRID_SIZE;
        int gz = cubeIdx / GRID_SIZE;
        float cx = 2.0f * gx - 99.0f;  // Center X (matches transform generation)
        float cz = 2.0f * gz - 99.0f;  // Center Z

        AABB aabb;
        aabb.minX = cx - m_config.cubeHalfXZ;
        aabb.maxX = cx + m_config.cubeHalfXZ;
        aabb.minZ = cz - m_config.cubeHalfXZ;
        aabb.maxZ = cz + m_config.cubeHalfXZ;

        // Day3.12 Phase 4B+: Fixture height overrides (indices computed at init)
        // Collision Y must match render: CUBE_TOP (3.0) + stepHeight
        if (m_config.enableStepUpTestFixtures)
        {
            const float CUBE_TOP = m_config.cubeMaxY;  // 3.0

            if (cubeIdx == m_fixtureT1Idx)
            {
                aabb.minY = 0.0f;
                aabb.maxY = CUBE_TOP + 0.3f;  // 3.3 - matches render
                return aabb;
            }
            if (cubeIdx == m_fixtureT2Idx)
            {
                aabb.minY = 0.0f;
                aabb.maxY = CUBE_TOP + 0.6f;  // 3.6 - matches render
                return aabb;
            }
            if (cubeIdx == m_fixtureT3StepIdx)
            {
                aabb.minY = 0.0f;
                aabb.maxY = CUBE_TOP + 0.5f;  // 3.5 - matches render
                return aabb;
            }
        }

        aabb.minY = m_config.cubeMinY;
        aabb.maxY = m_config.cubeMaxY;
        return aabb;
    }

    // PR2.2: Intersects/ComputeSignedPenetration moved to WorldCollisionMath.h (stateless)

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

    // PR2.8: QuerySupport, ResolveAxis, ResolveOverlaps_Capsule, SweepXZ_Capsule,
    // SweepY_Capsule, ResolveXZ_Capsule_Cleanup, ScanMaxXZPenetration, IsWallLike,
    // ProbeY, ProbeXZ, TryStepUp_Capsule, BuildPawnAABB — all moved to
    // Engine/Collision/CapsuleMovement.cpp

    void WorldState::TickFrame(float frameDt)
    {
        //=========================================================================
        // BLOCK 1: PresentationInput
        // CONTRACT: Reads sim yaw/pitch + presentation offsets
        // WRITES: m_renderCam.effectiveYaw/Pitch (proof fields only)
        //=========================================================================
        float effectiveYaw = m_view.yaw + m_presentationYawOffset;
        float effectivePitch = m_view.pitch + m_presentationPitchOffset;
        if (effectivePitch < m_config.pitchClampMin) effectivePitch = m_config.pitchClampMin;
        if (effectivePitch > m_config.pitchClampMax) effectivePitch = m_config.pitchClampMax;

#if defined(_DEBUG)
        m_renderCam.effectiveYaw = effectiveYaw;
        m_renderCam.effectivePitch = effectivePitch;
#endif

        //=========================================================================
        // BLOCK 2: CameraRig
        // CONTRACT: Computes target eye, smooths camera position
        // WRITES: m_renderCam.eyeX/Y/Z, m_renderCam.targetEyeX/Y/Z
        //=========================================================================
        float cosYaw = cosf(effectiveYaw);
        float sinYaw = sinf(effectiveYaw);
        float targetEyeX = m_pawn.posX - sinYaw * m_config.camOffsetBehind;
        float targetEyeY = m_pawn.posY + m_config.camOffsetUp;
        float targetEyeZ = m_pawn.posZ - cosYaw * m_config.camOffsetBehind;

#if defined(_DEBUG)
        m_renderCam.targetEyeX = targetEyeX;
        m_renderCam.targetEyeY = targetEyeY;
        m_renderCam.targetEyeZ = targetEyeZ;
#endif

        float followAlpha = 1.0f - expf(-m_config.camFollowRate * frameDt);
        m_renderCam.eyeX += (targetEyeX - m_renderCam.eyeX) * followAlpha;
        m_renderCam.eyeY += (targetEyeY - m_renderCam.eyeY) * followAlpha;
        m_renderCam.eyeZ += (targetEyeZ - m_renderCam.eyeZ) * followAlpha;

        //=========================================================================
        // BLOCK 3: FOV Smooth
        // CONTRACT: Smooths FOV based on sprint alpha
        // WRITES: m_renderCam.fovY
        //=========================================================================
        float targetFov = m_config.baseFovY + (m_config.sprintFovY - m_config.baseFovY) * m_sprintAlpha;
        float fovAlpha = 1.0f - expf(-m_config.fovSmoothRate * frameDt);
        m_renderCam.fovY += (targetFov - m_renderCam.fovY) * fovAlpha;

        //=========================================================================
        // BLOCK 4: Evidence Cleanup
        //=========================================================================
        m_jumpQueued = false;

#if defined(_DEBUG)
        // [PROOF-CAM-SPLIT] Throttled log (every 120 frames, Debug-only)
        static uint32_t s_camLogCounter = 0;
        if (++s_camLogCounter % 120 == 0)
        {
            char buf[256];
            sprintf_s(buf, "[PROOF-CAM-SPLIT] simYaw=%.3f prevOff=%.3f effYaw=%.3f eye=(%.2f,%.2f,%.2f)\n",
                m_view.yaw, m_presentationYawOffset, m_renderCam.effectiveYaw,
                m_renderCam.eyeX, m_renderCam.eyeY, m_renderCam.eyeZ);
            OutputDebugStringA(buf);
        }
#endif
    }

    DirectX::XMFLOAT4X4 WorldState::BuildViewProj(float aspect) const
    {
        //=========================================================================
        // CONTRACT: Read-only view/proj matrix construction
        // READS: m_renderCam.eyeX/Y/Z, m_renderCam.fovY, m_pawn.posX/Y/Z
        // INVARIANT: NEVER writes any state (const method)
        // WARNING: DO NOT MODIFY MATH - only member access paths changed
        //=========================================================================
        XMFLOAT3 eye = { m_renderCam.eyeX, m_renderCam.eyeY, m_renderCam.eyeZ };
        XMFLOAT3 target = { m_pawn.posX, m_pawn.posY + 1.5f, m_pawn.posZ };  // Look at pawn center
        XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

        XMVECTOR eyeVec = XMLoadFloat3(&eye);
        XMVECTOR targetVec = XMLoadFloat3(&target);
        XMVECTOR upVec = XMLoadFloat3(&up);

        // Right-handed view and projection
        XMMATRIX view = XMMatrixLookAtRH(eyeVec, targetVec, upVec);
        XMMATRIX proj = XMMatrixPerspectiveFovRH(m_renderCam.fovY, aspect, 1.0f, 1000.0f);
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
        snap.yawDeg = m_view.yaw * RAD_TO_DEG;
        snap.pitchDeg = m_view.pitch * RAD_TO_DEG;
        snap.fovDeg = m_renderCam.fovY * RAD_TO_DEG;
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

        // Day3.7: Camera basis proof (Bug A) - now from MovementBasisDebug
        snap.camFwdX = m_movementBasis.fwdX;
        snap.camFwdZ = m_movementBasis.fwdZ;
        snap.camRightX = m_movementBasis.rightX;
        snap.camRightZ = m_movementBasis.rightZ;
        snap.camDot = m_movementBasis.dot;

#if defined(_DEBUG)
        // Day4: Camera split proof (Debug-only fields)
        snap.simYaw = m_view.yaw;
        snap.simPitch = m_view.pitch;
        snap.presentationYawOffset = m_presentationYawOffset;
        snap.presentationPitchOffset = m_presentationPitchOffset;
        snap.effectiveYaw = m_renderCam.effectiveYaw;
        snap.effectivePitch = m_renderCam.effectivePitch;
        snap.renderEyeX = m_renderCam.eyeX;
        snap.renderEyeY = m_renderCam.eyeY;
        snap.renderEyeZ = m_renderCam.eyeZ;
        snap.targetEyeX = m_renderCam.targetEyeX;
        snap.targetEyeY = m_renderCam.targetEyeY;
        snap.targetEyeZ = m_renderCam.targetEyeZ;
        snap.step0PreviewActive = (m_presentationYawOffset != 0.0f || m_presentationPitchOffset != 0.0f);
#endif

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

        // Day3.11: Controller mode (always Capsule, AABB removed)
        snap.controllerMode = static_cast<uint8_t>(ControllerMode::Capsule);

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

        // Day3.12 Phase 4A: Y sweep diagnostics
        snap.sweepYHit = m_collisionStats.sweepYHit;
        snap.sweepYTOI = m_collisionStats.sweepYTOI;
        snap.sweepYHitCubeIdx = m_collisionStats.sweepYHitCubeIdx;
        snap.sweepYReqDy = m_collisionStats.sweepYReqDy;
        snap.sweepYAppliedDy = m_collisionStats.sweepYAppliedDy;

        // Day3.12 Phase 4B: Step-up diagnostics
        snap.stepTry = m_collisionStats.stepTry;
        snap.stepSuccess = m_collisionStats.stepSuccess;
        snap.stepFailMask = m_collisionStats.stepFailMask;
        snap.stepHeightUsed = m_collisionStats.stepHeightUsed;
        snap.stepCubeIdx = m_collisionStats.stepCubeIdx;

        // Day3.12+: Step grid test toggle state
        snap.stepGridTestEnabled = m_config.enableStepUpGridTest;
        snap.stepGridWasEverEnabled = m_stepGridWasEverEnabled;

        return snap;
    }
}
