#include "App.h"
#include "InputSampler.h"
#include "../Renderer/DX12/ToggleSystem.h"
#include "../Renderer/DX12/ImGuiLayer.h"

namespace Engine
{
    bool App::Initialize(HWND hwnd)
    {
        if (m_initialized)
            return false;

        m_hwnd = hwnd;

        // Initialize DX12 renderer
        if (!m_renderer.Initialize(hwnd))
        {
            return false;
        }

        // Initialize world state
        m_worldState.Initialize();
        m_accumulator = 0.0f;
        m_prevJump = false;

        m_initialized = true;
        return true;
    }

    void App::Tick()
    {
        if (!m_initialized)
            return;

        // Get frame delta time from renderer (from last frame)
        float frameDt = m_renderer.GetDeltaTime();

        // Accumulate time for fixed-step simulation
        m_accumulator += frameDt;

        // Clamp accumulator to avoid spiral of death (max 0.25s = ~15 fixed steps)
        if (m_accumulator > 0.25f) m_accumulator = 0.25f;

        // Check camera mode
        if (Renderer::ToggleSystem::GetCameraMode() == Renderer::CameraMode::ThirdPerson)
        {
            // Sample jump input with edge detection (must be on ground to trigger)
            bool spaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            bool jumpEdge = spaceDown && !m_prevJump && m_worldState.IsOnGround();
            m_prevJump = spaceDown;

            // Sample other inputs (may be blocked by ImGui)
            InputState input;
            if (Renderer::ImGuiLayer::WantsKeyboard())
            {
                // Zero pawn inputs when ImGui has keyboard focus
                input = InputState{};
            }
            else
            {
                input = InputSampler::Sample();
                input.jump = jumpEdge;
            }

            // Reset per-frame flags
            m_worldState.BeginFrame();

            // Fixed-step physics simulation
            while (m_accumulator >= FIXED_DT)
            {
                m_worldState.TickFixed(input, FIXED_DT);
                m_accumulator -= FIXED_DT;
            }

            // Variable-rate camera smoothing
            m_worldState.TickFrame(frameDt);

            // Build and inject camera
            float aspect = m_renderer.GetAspect();
            DirectX::XMFLOAT4X4 viewProj = m_worldState.BuildViewProj(aspect);
            m_renderer.SetFrameCamera(viewProj);

            // Build and send HUD snapshot
            m_renderer.SetHUDSnapshot(m_worldState.BuildSnapshot());
        }
        // else: Free camera mode - renderer uses its internal FreeCamera

        // Render frame
        m_renderer.Render();
    }

    void App::Shutdown()
    {
        if (!m_initialized)
            return;

        // Shutdown renderer first
        m_renderer.Shutdown();

        m_hwnd = nullptr;
        m_initialized = false;
    }
}
