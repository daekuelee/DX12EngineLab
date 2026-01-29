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

        // Initialize world state first (so renderer can get fixture data)
        m_worldState.Initialize();

        // Initialize DX12 renderer with worldState for fixture transform overrides
        if (!m_renderer.Initialize(hwnd, &m_worldState))
        {
            return false;
        }
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
            // 1. Apply mouse look ONCE per frame (before fixed-step loop)
            if (!Renderer::ImGuiLayer::WantsMouse())
            {
                m_worldState.ApplyMouseLook(m_pendingMouseDeltaX, m_pendingMouseDeltaY);
            }
            m_pendingMouseDeltaX = 0.0f;
            m_pendingMouseDeltaY = 0.0f;

            // 2. Sample jump input with edge detection (must be on ground to trigger)
            bool spaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            bool jumpEdge = spaceDown && !m_prevJump && m_worldState.IsOnGround();
            m_prevJump = spaceDown;

            // 3. Sample keyboard inputs (may be blocked by ImGui)
            InputState input;
            if (Renderer::ImGuiLayer::WantsKeyboard())
            {
                // Zero pawn inputs when ImGui has keyboard focus
                input = InputState{};
                m_prevJump = false;  // Reset when ImGui has focus
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

            // Send pawn transform for character rendering
            m_renderer.SetPawnTransform(
                m_worldState.GetPawnPosX(),
                m_worldState.GetPawnPosY(),
                m_worldState.GetPawnPosZ(),
                m_worldState.GetPawnYaw()
            );
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

    void App::OnMouseMove(int x, int y)
    {
        if (!m_mouseInitialized)
        {
            // First mouse event - just record position, no delta
            m_lastMouseX = x;
            m_lastMouseY = y;
            m_mouseInitialized = true;
            return;
        }

        // Accumulate delta for this frame
        m_pendingMouseDeltaX += static_cast<float>(x - m_lastMouseX);
        m_pendingMouseDeltaY += static_cast<float>(y - m_lastMouseY);
        m_lastMouseX = x;
        m_lastMouseY = y;
    }

    void App::ToggleControllerMode()
    {
        m_worldState.ToggleControllerMode();
    }

    void App::ToggleStepUpGridTest()
    {
        m_worldState.ToggleStepUpGridTest();
    }
}
