/******************************************************************************
 * FILE CONTRACT — App.cpp (Day4 PR2.2)
 *
 * SCOPE
 *   Owns application lifecycle, fixed-step simulation loop, camera injection.
 *   Orchestrates RAW input -> Action layer -> Physics flow.
 *   Injects action debug state into HUD snapshot.
 *
 * TICK CONTRACT
 *   1. ConsumeFrameInput exactly once per frame (RAW snapshot)
 *   2. GameplayActionSystem::BeginFrame (latch jump, cache movement)
 *   3. ApplyMouseLook once per frame (orientation only)
 *   4. WorldState::BeginFrame (reset per-frame flags)
 *   5. Fixed-step loop:
 *      - onGround = IsOnGround() [state from PREVIOUS step]
 *      - InputState from GameplayActionSystem::ConsumeForFixedStep
 *      - WorldState::TickFixed consumes InputState
 *   6. GameplayActionSystem::EndFrame (handle stepCount==0 timer decay)
 *   7. Inject action debug into HUD snapshot (at App level, not WorldState)
 *
 * INVARIANTS
 *   - onGround passed to ConsumeForFixedStep is state BEFORE step executes
 *   - Jump fires on first step only (allowJumpThisStep=true)
 *   - Action debug injection happens HERE (not WorldState::BuildSnapshot)
 *
 * PROOF POINTS
 *   [PROOF-JUMP-ONCE]         — isFirstStep flag controls allowJumpThisStep
 *   [PROOF-STEP0-LATCH]       — Action layer handles stepCount==0
 *   [PROOF-IMGUI-BLOCK-FLUSH] — Action layer flushes on ImGui capture
 ******************************************************************************/

#include "App.h"
#include "FrameInput.h"
#include "../Input/GameplayInputSystem.h"
#include "../Input/GameplayActionSystem.h"
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

        // Initialize action system (Day4 PR2.2)
        GameplayActionSystem::Initialize();

        // Initialize DX12 renderer with worldState for fixture transform overrides
        if (!m_renderer.Initialize(hwnd, &m_worldState))
        {
            return false;
        }
        m_accumulator = 0.0f;

        m_initialized = true;
        return true;
    }

    /******************************************************************************
     * FUNCTION CONTRACT — App::Tick (ThirdPerson branch, Day4 PR2.2)
     *
     * PRECONDITIONS
     *   - m_initialized == true
     *   - GameplayInputSystem initialized
     *   - GameplayActionSystem initialized
     *
     * INPUT CONSUMPTION (Day4 PR2.2 flow)
     *   1. ConsumeFrameInput exactly once (RAW snapshot, clears edges)
     *   2. GameplayActionSystem::BeginFrame (latch jump, cache movement)
     *   3. ApplyMouseLook (already masked if imguiMouse)
     *   4. WorldState::BeginFrame (reset m_jumpConsumedThisFrame)
     *   5. Fixed-step loop:
     *      - onGround = IsOnGround() [state from PREVIOUS step]
     *      - InputState from GameplayActionSystem::ConsumeForFixedStep
     *      - WorldState::TickFixed consumes InputState
     *   6. GameplayActionSystem::EndFrame (stepCount==0 timer decay)
     *   7. Inject action debug into HUD snapshot
     *
     * POSTCONDITIONS
     *   - WorldState updated via TickFixed/TickFrame
     *   - Camera injected to renderer
     *   - HUD snapshot with action debug sent to renderer
     ******************************************************************************/
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
            // 1. Consume RAW input (exactly once)
            // [PROOF-STUCK-KEY], [PROOF-HOLD-KEY], [PROOF-MOUSE-SPIKE]
            bool imguiKeyboard = Renderer::ImGuiLayer::WantsKeyboard();
            bool imguiMouse = Renderer::ImGuiLayer::WantsMouse();
            bool imguiBlocksGameplay = imguiKeyboard || imguiMouse;

            FrameInput frame = GameplayInputSystem::ConsumeFrameInput(
                frameDt, imguiKeyboard, imguiMouse
            );

            // 2. Latch actions (jump buffer, movement cache)
            // [PROOF-IMGUI-BLOCK-FLUSH] - flushes buffers if imguiBlocksGameplay
            GameplayActionSystem::BeginFrame(frame, imguiBlocksGameplay);

            // 3. Apply mouse look ONCE (already masked if imguiMouse)
            m_worldState.ApplyMouseLook(frame.mouseDX, frame.mouseDY);

            // 4. Reset per-frame flags (does NOT touch m_pawn.onGround)
            m_worldState.BeginFrame();

            // 5. Fixed-step loop with action system
            // [PROOF-JUMP-ONCE] — jump fires only when allowJumpThisStep=true
            // [PROOF-STEP0-LATCH] — buffer persists if stepCount==0
            uint32_t stepCount = 0;
            bool isFirstStep = true;

            while (m_accumulator >= FIXED_DT)
            {
                // onGround is state from PREVIOUS step (or last frame's final step)
                bool onGround = m_worldState.IsOnGround();

                // Get InputState from action layer
                InputState input = GameplayActionSystem::ConsumeForFixedStep(
                    onGround,
                    FIXED_DT,
                    isFirstStep  // allowJumpThisStep
                );

                m_worldState.TickFixed(input, FIXED_DT);
                m_accumulator -= FIXED_DT;
                isFirstStep = false;
                stepCount++;
            }

            // 6. End frame (handle stepCount==0 timer decay)
            GameplayActionSystem::EndFrame(stepCount, frameDt);

            // Variable-rate camera smoothing
            m_worldState.TickFrame(frameDt);

            // Build and inject camera
            float aspect = m_renderer.GetAspect();
            DirectX::XMFLOAT4X4 viewProj = m_worldState.BuildViewProj(aspect);
            m_renderer.SetFrameCamera(viewProj);

            // 7. Build HUD snapshot and inject action debug state
            Renderer::HUDSnapshot snap = m_worldState.BuildSnapshot();

            // Inject action debug at orchestration layer (not WorldState::BuildSnapshot)
            const auto& actionDebug = GameplayActionSystem::GetDebugState();
            snap.actionJumpBuffered = actionDebug.jumpBuffered;
            snap.actionJumpBufferTimer = actionDebug.jumpBufferTimer;
            snap.actionCoyoteActive = actionDebug.coyoteActive;
            snap.actionCoyoteTimer = actionDebug.coyoteTimer;
            snap.actionStepsThisFrame = actionDebug.stepsThisFrame;
            snap.actionJumpFiredThisFrame = actionDebug.jumpFiredThisFrame;
            snap.actionBlockedByImGui = actionDebug.blockedThisFrame;
            snap.actionBufferFlushedByBlock = actionDebug.bufferFlushedByBlock;

            m_renderer.SetHUDSnapshot(snap);

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

    void App::ToggleControllerMode()
    {
        m_worldState.ToggleControllerMode();
    }

    void App::ToggleStepUpGridTest()
    {
        m_worldState.ToggleStepUpGridTest();
    }
}
