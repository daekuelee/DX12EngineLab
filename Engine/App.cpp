/******************************************************************************
 * FILE CONTRACT — App.cpp (Day4 PR2.3)
 *
 * SCOPE
 *   Owns application lifecycle, fixed-step simulation loop, camera injection.
 *   Orchestrates RAW input -> Action layer -> Physics flow.
 *   Injects action debug state into HUD snapshot.
 *
 * TICK CONTRACT (ThirdPerson mode)
 *   1. ConsumeFrameInput exactly once per frame (RAW snapshot)
 *   2. GameplayActionSystem::StageFrameIntent (latch jump, cache movement, accum mouse)
 *   3. WorldState::BeginFrame (reset per-frame flags)
 *   4. Fixed-step loop:
 *      - onGround = IsOnGround() [state from PREVIOUS step]
 *      - InputState from GameplayActionSystem::BuildStepIntent (yawDelta/pitchDelta)
 *      - WorldState::TickFixed consumes InputState (applies yawDelta to sim yaw)
 *   5. GameplayActionSystem::FinalizeFrameIntent (handle stepCount==0 timer decay)
 *   6. C-2 preview: if stepCount==0, set presentation offset for camera
 *   7. Inject action debug into HUD snapshot (at App level, not WorldState)
 *
 * [LOOK-UNIFIED] ApplyMouseLook removed - all look input flows through BuildStepIntent
 *   Mouse + keyboard yaw are converted to yawDelta/pitchDelta with SSOT tuning in Action layer.
 *
 * INVARIANTS
 *   - onGround passed to BuildStepIntent is state BEFORE step executes
 *   - Jump fires on first step only (isFirstStep=true)
 *   - Look deltas computed on first step only [PROOF-LOOK-ONCE]
 *   - Action debug injection happens HERE (not WorldState::BuildSnapshot)
 *
 * PROOF POINTS
 *   [PROOF-JUMP-ONCE]         — isFirstStep flag controls jump firing
 *   [PROOF-LOOK-ONCE]         — isFirstStep flag controls look delta computation
 *   [PROOF-STEP0-LATCH]       — Action layer handles stepCount==0
 *   [PROOF-STEP0-LATCH-LOOK]  — Pending mouse persists, applied as C-2 preview
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

        // [DT-SSOT] Initialize frame clock
        m_frameClock.Init();

        return true;
    }

    /******************************************************************************
     * FUNCTION CONTRACT — App::Tick (ThirdPerson branch, Day4 PR2.3)
     *
     * PRECONDITIONS
     *   - m_initialized == true
     *   - GameplayInputSystem initialized
     *   - GameplayActionSystem initialized
     *
     * INPUT CONSUMPTION (Day4 PR2.3 flow - LOOK-UNIFIED)
     *   1. ConsumeFrameInput exactly once (RAW snapshot, clears edges)
     *   2. GameplayActionSystem::StageFrameIntent (latch jump, cache movement, accum mouse)
     *   3. WorldState::BeginFrame (reset m_jumpConsumedThisFrame)
     *   4. Fixed-step loop:
     *      - onGround = IsOnGround() [state from PREVIOUS step]
     *      - InputState from GameplayActionSystem::BuildStepIntent (yawDelta/pitchDelta)
     *      - WorldState::TickFixed consumes InputState
     *   5. GameplayActionSystem::FinalizeFrameIntent (stepCount==0 timer decay)
     *   6. C-2 preview: SetPresentationLookOffset when stepCount==0
     *   7. Inject action debug into HUD snapshot
     *
     * POSTCONDITIONS
     *   - WorldState updated via TickFixed/TickFrame
     *   - Camera injected to renderer (uses effective yaw = sim + presentation offset)
     *   - HUD snapshot with action debug sent to renderer
     ******************************************************************************/
    void App::Tick()
    {
        if (!m_initialized)
            return;

        /**************************************************************************
         * [DT-SSOT] Delta time computed once at Tick() start
         * All consumers use same frameDt; renderer never measures dt.
         **************************************************************************/
        m_frameClock.Update();
        float frameDt = m_frameClock.GetDeltaSeconds();

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

            // 2. Stage frame intent (jump buffer, movement cache, pending mouse)
            // [PROOF-IMGUI-BLOCK-FLUSH] - flushes buffers if imguiBlocksGameplay
            // [LOOK-UNIFIED] mouse deltas accumulated here for BuildStepIntent
            GameplayActionSystem::StageFrameIntent(frame, imguiBlocksGameplay);

            // 3. Reset per-frame flags (does NOT touch m_pawn.onGround)
            m_worldState.BeginFrame();

            // 4. Fixed-step loop with action system
            // [PROOF-JUMP-ONCE] — jump fires only when isFirstStep=true
            // [PROOF-LOOK-ONCE] — look deltas computed only when isFirstStep=true
            // [PROOF-STEP0-LATCH] — buffer persists if stepCount==0
            uint32_t stepCount = 0;
            bool isFirstStep = true;
            constexpr bool isThirdPerson = true;  // We're in the ThirdPerson branch

            while (m_accumulator >= FIXED_DT)
            {
                // onGround is state from PREVIOUS step (or last frame's final step)
                bool onGround = m_worldState.IsOnGround();

                // Build StepIntent from action layer
                // [LOOK-UNIFIED] yawDelta/pitchDelta computed inside
                InputState input = GameplayActionSystem::BuildStepIntent(
                    onGround,
                    FIXED_DT,
                    isFirstStep,
                    isThirdPerson
                );

                m_worldState.TickFixed(input, FIXED_DT);
                m_accumulator -= FIXED_DT;
                isFirstStep = false;
                stepCount++;
            }

            // 5. Finalize frame intent (handle stepCount==0 timer decay)
            GameplayActionSystem::FinalizeFrameIntent(stepCount, frameDt);

            // 6. C-2: Presentation-only preview when no fixed steps ran
            // [PROOF-STEP0-LATCH-LOOK] — pending mouse persists, applied as preview offset
            if (stepCount == 0 && !imguiBlocksGameplay)
            {
                float previewYaw, previewPitch;
                GameplayActionSystem::GetPendingLookPreviewRad(previewYaw, previewPitch);
                m_worldState.SetPresentationLookOffset(previewYaw, previewPitch);
            }
            else
            {
                m_worldState.ClearPresentationLookOffset();
            }

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

        /**************************************************************************
         * [CALL-ORDER] dt injection order guarantee:
         *   1. FrameClock.Update() - measure dt (done above)
         *   2. SetFrameDeltaTime(dt) - inject to renderer (here)
         *   3. Render() - uses injected dt via GetDeltaTime() (below)
         **************************************************************************/
        m_renderer.SetFrameDeltaTime(frameDt);

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
