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

    // Part 2: Axis-Aligned Bounding Box
    struct AABB
    {
        float minX, minY, minZ;
        float maxX, maxY, maxZ;
    };

    // Part 2: Collision statistics for HUD display
    struct CollisionStats
    {
        uint32_t candidatesChecked = 0;
        uint32_t penetrationsResolved = 0;
        int32_t lastHitCubeId = -1;
        Axis lastAxisResolved = Axis::Y;
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
        float jumpVelocity = 12.0f;        // units/sec

        // Camera smoothing
        float sprintSmoothRate = 8.0f;     // 1/sec
        float camFollowRate = 10.0f;       // 1/sec
        float baseFovY = 0.785398163f;     // rad (45 degrees)
        float sprintFovY = 0.959931089f;   // rad (55 degrees)
        float fovSmoothRate = 6.0f;        // 1/sec

        // Camera offset from pawn
        float camOffsetBehind = 15.0f;     // units
        float camOffsetUp = 8.0f;          // units

        // Floor collision bounds (cube grid extents)
        float floorMinX = -100.0f;
        float floorMaxX = 100.0f;
        float floorMinZ = -100.0f;
        float floorMaxZ = 100.0f;
        float floorY = 0.0f;

        // KillZ (respawn trigger)
        float killZ = -50.0f;

        // Spawn position
        float spawnX = 0.0f;
        float spawnY = 5.0f;  // Slightly above floor
        float spawnZ = 0.0f;

        // Part 2: Pawn AABB dimensions
        float pawnHalfWidth = 0.4f;    // X/Z half-extent
        float pawnHeight = 5.0f;       // Total height (feet at posY, head at posY+height)

        // Part 2: Cube dimensions (scale 0.9, 3.0, 0.9 -> half-extents 0.45, 1.5, 0.45)
        // Cubes sit on floor (Y=0), top at Y=3.0
        float cubeHalfXZ = 0.45f;
        float cubeMinY = 0.0f;
        float cubeMaxY = 3.0f;
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

        // Part 2: Spatial hash grid (100x100 cells, each cell contains cube index)
        // Built once at init - cubes don't move
        static constexpr int GRID_SIZE = 100;
        std::vector<uint16_t> m_spatialGrid[GRID_SIZE][GRID_SIZE];
        bool m_spatialGridBuilt = false;

        // Private helpers
        void ResolveFloorCollision();
        void CheckKillZ();

        // Part 2: Collision helpers
        void BuildSpatialGrid();
        int WorldToCellX(float x) const;
        int WorldToCellZ(float z) const;
        AABB BuildPawnAABB(float px, float py, float pz) const;
        AABB GetCubeAABB(uint16_t cubeIdx) const;
        bool Intersects(const AABB& a, const AABB& b) const;
        float ComputeSignedPenetration(const AABB& pawn, const AABB& cube, Axis axis) const;
        std::vector<uint16_t> QuerySpatialHash(const AABB& pawn) const;
        void ResolveAxis(float& posAxis, float currentPosX, float currentPosY, float currentPosZ, Axis axis);
    };
}
