#pragma once

#include <DirectXMath.h>

// Forward declare HUDSnapshot from Renderer namespace
namespace Renderer { struct HUDSnapshot; }

namespace Engine
{
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

    private:
        PawnState m_pawn;
        CameraState m_camera;
        MapState m_map;
        WorldConfig m_config;

        float m_sprintAlpha = 0.0f;         // 0-1 smoothed sprint blend
        bool m_jumpConsumedThisFrame = false;
        bool m_jumpQueued = false;          // Evidence: true for 1 frame after jump
    };
}
