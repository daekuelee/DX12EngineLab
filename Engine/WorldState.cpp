#include "WorldState.h"
#include "../Renderer/DX12/Dx12Context.h"  // For HUDSnapshot
#include <cmath>

using namespace DirectX;

namespace Engine
{
    void WorldState::Initialize()
    {
        // Reset pawn to spawn position
        m_pawn = PawnState{};
        m_pawn.posX = 0.0f;
        m_pawn.posY = 0.0f;
        m_pawn.posZ = 0.0f;
        m_pawn.yaw = 0.0f;
        m_pawn.pitch = 0.0f;
        m_pawn.onGround = true;

        // Initialize camera at offset from pawn
        m_camera.eyeX = m_pawn.posX;
        m_camera.eyeY = m_pawn.posY + m_config.camOffsetUp;
        m_camera.eyeZ = m_pawn.posZ - m_config.camOffsetBehind;
        m_camera.fovY = m_config.baseFovY;

        m_sprintAlpha = 0.0f;
        m_jumpConsumedThisFrame = false;
        m_jumpQueued = false;
    }

    void WorldState::BeginFrame()
    {
        // Reset per-frame flags
        m_jumpConsumedThisFrame = false;
    }

    void WorldState::TickFixed(const InputState& input, float fixedDt)
    {
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
        }

        // 8. Integrate position
        m_pawn.posX += m_pawn.velX * fixedDt;
        m_pawn.posY += m_pawn.velY * fixedDt;
        m_pawn.posZ += m_pawn.velZ * fixedDt;

        // 9. Ground collision
        if (m_pawn.posY < m_map.groundY)
        {
            m_pawn.posY = m_map.groundY;
            m_pawn.velY = 0.0f;
            m_pawn.onGround = true;
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
        return snap;
    }
}
