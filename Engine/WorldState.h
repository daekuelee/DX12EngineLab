#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>
#include "InputState.h"

// Forward declare HUDSnapshot from Renderer namespace
namespace Renderer { struct HUDSnapshot; }

namespace Engine
{
    // Part 2: Axis enum for collision resolution
    enum class Axis : uint8_t { X, Y, Z };

    // Day3.5: Support source for onGround determination
    enum class SupportSource : uint8_t { FLOOR = 0, CUBE = 1, NONE = 2 };

    // Day3.11: Controller mode (SSOT in Engine, not Renderer)
    enum class ControllerMode : uint8_t { AABB = 0, Capsule = 1 };

    // Day3.5: Support query result
    struct SupportResult
    {
        SupportSource source = SupportSource::NONE;
        float supportY = -1000.0f;
        int32_t cubeId = -1;
        float gap = 0.0f;
        uint32_t candidateCount = 0;  // For gap anomaly log
    };

    // Part 2: Axis-Aligned Bounding Box
    struct AABB
    {
        float minX, minY, minZ;
        float maxX, maxY, maxZ;
    };

    // Day3.12 Phase 4B+: Extra collider types (future: ramps, trimesh)
    enum class ExtraColliderType : uint8_t { AABB = 0, Ramp = 1, TriMesh = 2 };

    struct ExtraCollider
    {
        ExtraColliderType type = ExtraColliderType::AABB;
        AABB aabb = {};
    };

    // Day3.12 Phase 4B+: ID space for extras layer
    static constexpr uint16_t EXTRA_BASE = 20000;
    static constexpr uint16_t MAX_EXTRA_COLLIDERS = 32;

    // Day3.11: Capsule geometry helper (feet-bottom anchor)
    struct CapsulePoints { float P0y, P1y; };

    inline CapsulePoints MakeCapsuleFromFeet(float feetY, float r, float hh)
    {
        return { feetY + r, feetY + r + 2.0f * hh };
    }

    // Part 2: Collision statistics for HUD display
    struct CollisionStats
    {
        uint32_t candidatesChecked = 0;   // Sum of spatial hash query results (across all ResolveAxis calls)
        uint32_t contacts = 0;            // Sum of AABB intersections (NOT deduplicated - same cube may be counted multiple times)
        uint32_t penetrationsResolved = 0;
        int32_t lastHitCubeId = -1;
        Axis lastAxisResolved = Axis::Y;
        // Day3.4: Iteration diagnostics
        uint8_t iterationsUsed = 0;       // 1-8 iterations before convergence or max
        float maxPenetrationAbs = 0.0f;   // Largest |penetration| observed this tick
        bool hitMaxIter = false;          // True ONLY if ran all 8 iterations AND did NOT converge
        // Day3.5: Support diagnostics
        SupportSource supportSource = SupportSource::NONE;
        float supportY = -1000.0f;
        int32_t supportCubeId = -1;
        bool snappedThisTick = false;
        float supportGap = 0.0f;
        // Day3.8: MTV debug fields (Issue A proof)
        float lastPenX = 0.0f;      // X penetration before resolution
        float lastPenZ = 0.0f;      // Z penetration before resolution
        uint8_t mtvAxis = 0;        // 0=X, 2=Z (which axis MTV chose)
        float mtvMagnitude = 0.0f;  // Magnitude of chosen penetration
        float centerDiffX = 0.0f;   // For sign determination proof
        float centerDiffZ = 0.0f;
        // Day3.9: Regression debug fields (reset each tick in TickFixed)
        bool xzStillOverlapping = false;  // After XZ push-out, does intersection persist?
        bool yStepUpSkipped = false;      // Was Y correction skipped by anti-step-up guard?
        float yDeltaApplied = 0.0f;       // Actual Y correction applied
        // Day3.11 Phase 2: Capsule depenetration diagnostics
        // CONTRACT: These fields are set by ResolveOverlaps_Capsule() at tick start.
        // Other collision code MUST NOT overwrite these fields.
        bool depenApplied = false;
        float depenTotalMag = 0.0f;
        bool depenClampTriggered = false;
        float depenMaxSingleMag = 0.0f;
        uint32_t depenOverlapCount = 0;
        uint32_t depenIterations = 0;
        // Day3.11 Phase 3: Capsule sweep diagnostics
        bool sweepHit = false;
        float sweepTOI = 1.0f;
        int32_t sweepHitCubeIdx = -1;
        uint32_t sweepCandCount = 0;
        float sweepReqDx = 0.0f, sweepReqDz = 0.0f;
        float sweepAppliedDx = 0.0f, sweepAppliedDz = 0.0f;
        float sweepSlideDx = 0.0f, sweepSlideDz = 0.0f;
        float sweepNormalX = 0.0f, sweepNormalZ = 0.0f;
        // Day3.12 Phase 4A: Y sweep diagnostics
        bool sweepYHit = false;
        float sweepYTOI = 1.0f;
        int32_t sweepYHitCubeIdx = -1;
        float sweepYReqDy = 0.0f;
        float sweepYAppliedDy = 0.0f;
        // Day3.12 Phase 4B: Step-up diagnostics
        bool stepTry = false;           // Did we attempt step-up?
        bool stepSuccess = false;       // Did step-up succeed?
        uint8_t stepFailMask = 0;       // Failure reason bits (StepFailMask enum)
        float stepHeightUsed = 0.0f;    // Actual height stepped
        int32_t stepCubeIdx = -1;       // Cube we stepped onto
    };

    // Day3.12 Phase 4B: Step-up failure reason bits
    enum StepFailMask : uint8_t {
        STEP_FAIL_NONE          = 0x00,
        STEP_FAIL_UP_BLOCKED    = 0x01,  // Ceiling within step height
        STEP_FAIL_FWD_BLOCKED   = 0x02,  // Forward probe still blocked
        STEP_FAIL_NO_GROUND     = 0x04,  // Down settle found no support
        STEP_FAIL_PENETRATION   = 0x08,  // Final pose has penetration
    };

    // Day4 PR2.2: InputState moved to Engine/InputState.h (lightweight header)
    // Used by GameplayActionSystem (producer) and WorldState::TickFixed (consumer)

    //-------------------------------------------------------------------------
    // CONTRACT: PawnState - Simulation-owned physics and control state
    //
    // OWNERSHIP:
    //   - WRITER: WorldState::TickFixed() exclusively
    //   - READERS: WorldState::TickFrame(), BuildViewProj(), BuildSnapshot()
    //
    // CONTROL VIEW (yaw/pitch):
    //   - Conceptual "ControlViewState" lives here as yaw/pitch fields
    //   - TickFrame NEVER writes yaw or pitch
    //   - Presentation offsets are ADDITIVE and applied in TickFrame only
    //-------------------------------------------------------------------------
    struct PawnState
    {
        float posX = 0.0f;
        float posY = 0.0f;
        float posZ = 0.0f;
        float velX = 0.0f;
        float velY = 0.0f;
        float velZ = 0.0f;
        float yaw = 0.0f;         // Radians
        float pitch = 0.0f;       // Radians
        bool onGround = true;
    };

    // Camera state (smoothed) - DEPRECATED: See RenderCameraState below
    struct CameraState
    {
        float eyeX = 0.0f;
        float eyeY = 8.0f;
        float eyeZ = -15.0f;
        float fovY = 0.785398163f;  // 45 degrees in radians
        // Day3.7: Camera basis debug fields for HUD proof
        float dbgFwdX = 0.0f, dbgFwdZ = 0.0f;
        float dbgRightX = 0.0f, dbgRightZ = 0.0f;
        float dbgDot = 0.0f;  // Orthogonality proof: should be ~0
    };

    //-------------------------------------------------------------------------
    // CONTRACT: MovementBasisDebug - Sim movement basis (TickFixed-computed)
    //
    // OWNERSHIP:
    //   - WRITER: WorldState::TickFixed() exclusively
    //   - READERS: WorldState::BuildSnapshot()
    //
    // PURPOSE:
    //   Camera-relative basis vectors used for Sim movement calculation.
    //   Computed from pawn-to-camera direction at physics rate.
    //   Stored for HUD proof display (Bug A orthogonality check).
    //
    // NOTE: This is the SIM movement basis, NOT the render camera direction.
    //   During step0 preview, the render camera may be rotated by presentation
    //   offset, but movement basis remains unchanged (only updated by TickFixed).
    //-------------------------------------------------------------------------
    struct MovementBasisDebug
    {
        float fwdX = 0.0f, fwdZ = 0.0f;    // Normalized pawn-to-camera direction (XZ)
        float rightX = 0.0f, rightZ = 0.0f; // Cross(fwd, up)
        float dot = 0.0f;                   // Orthogonality proof: should be ~0
    };

    //-------------------------------------------------------------------------
    // CONTRACT: RenderCameraState - TickFrame-owned render camera data
    //
    // OWNERSHIP:
    //   - WRITER: Initialize() (once at startup), then TickFrame() exclusively
    //   - READERS: WorldState::BuildViewProj() (const), WorldState::BuildSnapshot()
    //
    // INVARIANTS:
    //   - TickFixed NEVER writes these fields (after Initialize)
    //   - BuildViewProj NEVER writes these fields (const method)
    //
    // DERIVATION (TickFrame):
    //   1. effectiveYaw = m_pawn.yaw + m_presentationYawOffset
    //   2. targetEye computed from effectiveYaw and m_pawn.pos
    //   3. eye smoothed toward targetEye (exponential)
    //   4. fov smoothed toward sprint target
    //-------------------------------------------------------------------------
    struct RenderCameraState
    {
        // Core render state (smoothed)
        float eyeX = 0.0f, eyeY = 8.0f, eyeZ = -15.0f;
        float fovY = 0.785398163f;  // 45 degrees

#if defined(_DEBUG)
        // PROOF fields (Debug-only, written by TickFrame for HUD validation)
        float effectiveYaw = 0.0f;    // sim yaw + presentationYawOffset
        float effectivePitch = 0.0f;  // sim pitch + presentationPitchOffset (clamped)
        float targetEyeX = 0.0f, targetEyeY = 0.0f, targetEyeZ = 0.0f;
#endif
    };

    // Map configuration
    struct MapState
    {
        const char* name = "TestYard";
        float groundY = 0.0f;
    };

    // Tuning constants
    struct WorldConfig
    {
        // Movement
        float walkSpeed = 30.0f;           // units/sec
        float sprintMultiplier = 2.0f;     // ratio
        float lookSpeed = 2.0f;            // rad/sec
        float mouseSensitivity = 0.003f;   // rad/pixel for mouse look

        // Pitch limits
        float pitchClampMin = -1.2f;       // rad (~-69 degrees)
        float pitchClampMax = 0.3f;        // rad (~17 degrees)

        // Physics
        float gravity = 30.0f;             // units/sec^2
        float jumpVelocity = 15.0f;        // units/sec (v²/2g = 225/60 = 3.75 max height)

        // Camera smoothing
        float sprintSmoothRate = 8.0f;     // 1/sec
        float camFollowRate = 10.0f;       // 1/sec
        float baseFovY = 0.785398163f;     // rad (45 degrees)
        float sprintFovY = 0.959931089f;   // rad (55 degrees)
        float fovSmoothRate = 6.0f;        // 1/sec

        // Camera offset from pawn
        float camOffsetBehind = 15.0f;     // units
        float camOffsetUp = 8.0f;          // units

        // Floor collision bounds (match rendered floor geometry)
        float floorMinX = -200.0f;
        float floorMaxX = 200.0f;
        float floorMinZ = -200.0f;
        float floorMaxZ = 200.0f;
        float floorY = 0.0f;

        // KillZ (respawn trigger)
        float killZ = -50.0f;

        // Spawn position (grid cell center, not boundary)
        float spawnX = 1.0f;
        float spawnY = 5.0f;  // Above floor (falls to Y=0)
        float spawnZ = 1.0f;

        // Part 2: Pawn AABB dimensions (Day3.7: axis-aware)
        // X extent: arms outer edge = offsetX(1.0) + scaleX(0.4) = 1.4
        // Z extent: keep tight depth
        float pawnHalfExtentX = 1.4f;  // Arms reach
        float pawnHalfExtentZ = 0.4f;  // Tight depth
        float pawnHeight = 5.0f;       // Total height (feet at posY, head at posY+height)

        // Part 2: Cube collision dimensions
        // - Mesh local half-extent = 1.0 (vertices at ±1)
        // - Render scale: XZ=0.9, Y=3.0, placed at Y=0 center
        // - Visual bounds: X/Z = ±0.9, Y = -3 to +3
        // - Collision X/Z = 1.0 * 0.9 = 0.9
        // - Collision Y = [0,3] (above-floor portion only - floor prevents Y<0)
        float cubeHalfXZ = 0.9f;
        float cubeMinY = 0.0f;
        float cubeMaxY = 3.0f;

        // Day3.11: Capsule SSOT (feet-bottom anchor)
        // Total height = 2*r + 2*hh = 2*1.4 + 2*1.1 = 5.0 (matches pawnHeight)
        // Radius matches pawnHalfExtentX (1.4) for visual consistency
        float capsuleRadius = 1.4f;
        float capsuleHalfHeight = 1.1f;

        // Day3.12 Phase 4A: Y sweep config
        bool enableYSweep = true;   // Toggle for Y sweep (fallback to ResolveAxis Y)
        float sweepSkinY = 0.01f;   // Skin width for Y sweep

        // Day3.12 Phase 4B: Step-up config
        bool enableStepUp = true;   // Toggle for step-up auto-climb
        float maxStepHeight = 0.3f; // Max obstacle height to climb

        // Day3.12 Phase 4B: Test fixture config
        bool enableStepUpTestFixtures = true;  // Test fixture toggle
        bool enableStepUpGridTest = false;     // Stair grid test (disables T1/T2/T3 when true)
    };

    class WorldState
    {
    public:
        WorldState() = default;
        ~WorldState() = default;

        void Initialize();
        void BeginFrame();                                        // Reset jump consumed flag
        void TickFixed(const InputState& input, float fixedDt);   // 60 Hz physics
        void TickFrame(float frameDt);                            // Variable dt smoothing
        DirectX::XMFLOAT4X4 BuildViewProj(float aspect) const;
        Renderer::HUDSnapshot BuildSnapshot() const;

        bool IsOnGround() const { return m_pawn.onGround; }
        float GetSprintAlpha() const { return m_sprintAlpha; }

        //---------------------------------------------------------------------
        // C-2 Presentation-Only Look Offset
        //
        // PURPOSE:
        //   When stepCount==0 (no fixed steps ran), the camera can still
        //   respond to mouse input via a presentation-only offset that does
        //   NOT mutate sim yaw/pitch. This prevents visual lag on high-FPS.
        //
        // SSOT BOUNDARY:
        //   - m_presentationYawOffset/m_presentationPitchOffset are applied
        //     in TickFrame() for camera positioning only
        //   - Sim yaw/pitch (m_pawn.yaw/pitch) are ONLY mutated in TickFixed
        //
        // CALL PATTERN:
        //   - App calls SetPresentationLookOffset() when stepCount==0
        //   - App calls ClearPresentationLookOffset() when stepCount>0
        //---------------------------------------------------------------------
        void SetPresentationLookOffset(float yawRad, float pitchRad);
        void ClearPresentationLookOffset();

        // Pawn position/yaw accessors for character rendering
        float GetPawnPosX() const { return m_pawn.posX; }
        float GetPawnPosY() const { return m_pawn.posY; }
        float GetPawnPosZ() const { return m_pawn.posZ; }
        float GetPawnYaw() const { return m_pawn.yaw; }

        // Respawn tracking accessors (Part 1)
        uint32_t GetRespawnCount() const { return m_respawnCount; }
        const char* GetLastRespawnReason() const { return m_lastRespawnReason; }

        // Day3.11: Controller mode
        ControllerMode GetControllerMode() const { return m_controllerMode; }
        void ToggleControllerMode();
        void RespawnResetControllerState();

        // Day3.12+: Step-up grid test toggle
        bool IsStepUpGridTestEnabled() const { return m_config.enableStepUpGridTest; }
        void ToggleStepUpGridTest();

        // Part 2: Collision stats accessor
        const CollisionStats& GetCollisionStats() const { return m_collisionStats; }

        // Day3.12 Phase 4B+: Fixture accessors for renderer transform overrides
        const WorldConfig& GetConfig() const { return m_config; }
        const std::vector<ExtraCollider>& GetExtras() const { return m_extras; }
        uint16_t GetFixtureT1Idx() const { return m_fixtureT1Idx; }
        uint16_t GetFixtureT2Idx() const { return m_fixtureT2Idx; }
        uint16_t GetFixtureT3StepIdx() const { return m_fixtureT3StepIdx; }

    private:
        PawnState m_pawn;
        // Day4 PR1: Split camera state into SSOT structs
        MovementBasisDebug m_movementBasis;  // Written by TickFixed only
        RenderCameraState m_renderCam;       // Written by TickFrame only
        MapState m_map;
        WorldConfig m_config;

        float m_sprintAlpha = 0.0f;         // 0-1 smoothed sprint blend
        bool m_jumpConsumedThisFrame = false;
        bool m_jumpQueued = false;          // Evidence: true for 1 frame after jump

        // C-2: Presentation-only look offset (does NOT mutate sim yaw/pitch)
        float m_presentationYawOffset = 0.0f;
        float m_presentationPitchOffset = 0.0f;

        // Respawn tracking (Part 1)
        uint32_t m_respawnCount = 0;
        const char* m_lastRespawnReason = nullptr;

        // Part 2: Collision stats (reset each tick)
        CollisionStats m_collisionStats;

        // Floor diagnostic flag (reset each tick, set in ResolveFloorCollision)
        bool m_didFloorClampThisTick = false;

        // Day3.5: Jump grace flag (prevents support query from clearing onGround on jump frame)
        bool m_justJumpedThisTick = false;

        // Day3.11: Controller mode
        ControllerMode m_controllerMode = ControllerMode::AABB;

        // Part 2: Spatial hash grid (100x100 cells, each cell contains cube index)
        // Built once at init - cubes don't move
        static constexpr int GRID_SIZE = 100;
        std::vector<uint16_t> m_spatialGrid[GRID_SIZE][GRID_SIZE];
        bool m_spatialGridBuilt = false;

        // Day3.12 Phase 4B: Test fixture indices (computed from world coords)
        uint16_t m_fixtureT1Idx = 0;
        uint16_t m_fixtureT2Idx = 0;
        uint16_t m_fixtureT3StepIdx = 0;

        // Day3.12 Phase 4B+: Extras layer for ceiling and future colliders
        std::vector<ExtraCollider> m_extras;
        void BuildExtraFixtures();
        void BuildStepUpGridTest();  // Day3.12: Stair grid test map
        void RegisterAABBToSpatialGrid(uint16_t id, const AABB& aabb);
        void ClearExtrasFromSpatialGrid();

        // Day3.12+: Track if step grid was ever built (for safe toggle)
        bool m_stepGridWasEverEnabled = false;

        // Private helpers
        void ResolveFloorCollision();
        void CheckKillZ();

        // Day3.5: Support query (checks floor and cubes for landing surface)
        SupportResult QuerySupport(float px, float py, float pz, float velY) const;

        // Part 2: Collision helpers
        void BuildSpatialGrid();
        int WorldToCellX(float x) const;
        int WorldToCellZ(float z) const;
        AABB BuildPawnAABB(float px, float py, float pz) const;
        AABB GetCubeAABB(uint16_t cubeIdx) const;
        bool Intersects(const AABB& a, const AABB& b) const;
        float ComputeSignedPenetration(const AABB& pawn, const AABB& cube, Axis axis) const;
        std::vector<uint16_t> QuerySpatialHash(const AABB& pawn) const;
        void ResolveAxis(float& posAxis, float currentPosX, float currentPosY, float currentPosZ, Axis axis, float prevPawnBottom = 0.0f);
        // Day3.8: MTV-based XZ resolution (Issue A fix)
        void ResolveXZ_MTV(float& newX, float& newZ, float newY);
        // Day3.11 Phase 2: Capsule depenetration
        void ResolveOverlaps_Capsule();
        // Day3.11 Phase 3: Capsule XZ sweep/slide
        void SweepXZ_Capsule(float reqDx, float reqDz, float& outAppliedDx, float& outAppliedDz,
                             bool& outZeroVelX, bool& outZeroVelZ);
        // Day3.12 Phase 4A: Capsule Y sweep
        void SweepY_Capsule(float reqDy, float& outAppliedDy);
        // Day3.11 Phase 3 Fix: XZ-only cleanup pass for residual penetrations
        void ResolveXZ_Capsule_Cleanup(float& newX, float& newZ, float newY);
        // Day3.11 Phase 3 Debug: Scan max XZ penetration depth (for instrumentation)
        float ScanMaxXZPenetration(float posX, float posY, float posZ);

        // Day3.12 Phase 4B: Step-up helpers
        // Probe Y sweep at arbitrary pose (no stats mutation)
        float ProbeY(float posX, float posY, float posZ, float reqDy, int& hitCubeIdx);
        // Probe XZ sweep at arbitrary pose (no stats mutation)
        float ProbeXZ(float posX, float posY, float posZ, float reqDx, float reqDz,
                      float& outNormalX, float& outNormalZ, int& hitCubeIdx);
        // Try step-up maneuver: up -> forward -> down settle
        bool TryStepUp_Capsule(float startX, float startY, float startZ,
                               float reqDx, float reqDz,
                               float& outX, float& outY, float& outZ);
        // Check if collision normal is wall-like (horizontal)
        bool IsWallLike(float normalX, float normalZ) const;
    };
}
