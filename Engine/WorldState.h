#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>

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
    };
    // Input state sampled each frame
    struct InputState
    {
        float moveX = 0.0f;       // Axis: -1 to +1 (strafe)
        float moveZ = 0.0f;       // Axis: -1 to +1 (forward/back)
        float yawAxis = 0.0f;     // Axis: -1 to +1 (rotation)
        float pitchAxis = 0.0f;   // Axis: -1 to +1 (look up/down)
        bool sprint = false;
        bool jump = false;        // True if jump triggered this frame
    };

    // Pawn physics state
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

    // Camera state (smoothed)
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
        // Total height = 2*r + 2*hh = 2*0.8 + 2*1.7 = 5.0 (matches pawnHeight)
        float capsuleRadius = 0.8f;
        float capsuleHalfHeight = 1.7f;
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

        // Mouse look (applied once per frame, before fixed-step)
        void ApplyMouseLook(float deltaX, float deltaY);

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

        // Part 2: Collision stats accessor
        const CollisionStats& GetCollisionStats() const { return m_collisionStats; }

    private:
        PawnState m_pawn;
        CameraState m_camera;
        MapState m_map;
        WorldConfig m_config;

        float m_sprintAlpha = 0.0f;         // 0-1 smoothed sprint blend
        bool m_jumpConsumedThisFrame = false;
        bool m_jumpQueued = false;          // Evidence: true for 1 frame after jump

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
        // Day3.11 Phase 3 Fix: XZ-only cleanup pass for residual penetrations
        void ResolveXZ_Capsule_Cleanup(float& newX, float& newZ, float newY);
    };
}
