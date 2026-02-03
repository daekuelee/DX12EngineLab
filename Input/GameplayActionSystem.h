#pragma once
/******************************************************************************
 * FILE CONTRACT - GameplayActionSystem.h (Day4 PR2.2)
 *
 * SCOPE
 *   Owns action-layer buffering for gameplay actions.
 *   Translates FrameInput (RAW) into InputState (per-step).
 *
 * SSOT OWNERSHIP
 *   - Jump buffer state (latched intent + timer)
 *   - Coyote timer (grace period after leaving ground)
 *   - Per-frame cached movement intent
 *
 * DEPENDENCIES
 *   - Includes: Engine/FrameInput.h, Engine/InputState.h
 *   - Does NOT include: Engine/WorldState.h (decoupled from physics)
 *
 * INVARIANTS
 *   - Jump fires at most once per frame (first step only)
 *   - Movement applies to all fixed steps within frame
 *   - ImGui block flushes all buffers (no delayed action)
 *   - WM_KILLFOCUS triggers ResetAllState (no stale buffers on refocus)
 *
 * TIMING POLICY
 *   - stepCount > 0: timers decrement by fixedDt per step in ConsumeForFixedStep
 *   - stepCount == 0: timers decrement by frameDt once in EndFrame
 *   - NO double-decay: EndFrame explicitly guards with if (stepCount == 0)
 *   - Prevents infinite buffer on consecutive 0-step frames
 *
 * PROOF POINTS
 *   [PROOF-STEP0-LATCH]       - stepCount==0 doesn't lose jump
 *   [PROOF-JUMP-ONCE]         - multiple steps -> jump fires once
 *   [PROOF-IMGUI-BLOCK-FLUSH] - ImGui capture flushes buffer
 ******************************************************************************/

#include "../Engine/FrameInput.h"
#include "../Engine/InputState.h"
#include <cstdint>

namespace GameplayActionSystem
{
    //-------------------------------------------------------------------------
    // Tuning Constants (SSOT)
    //-------------------------------------------------------------------------
    struct ActionConfig
    {
        float jumpBufferDuration = 0.1f;   // seconds - how long to hold jump intent
        float coyoteTimeDuration = 0.08f;  // seconds - grace period after leaving ground
    };

    //-------------------------------------------------------------------------
    // Debug/Proof State (read-only snapshot for HUD)
    //-------------------------------------------------------------------------
    struct ActionDebugState
    {
        // Jump buffer proof
        bool jumpBuffered = false;         // Currently holding jump intent
        float jumpBufferTimer = 0.0f;      // Time remaining in buffer

        // Coyote time proof
        bool coyoteActive = false;         // Currently in coyote grace period
        float coyoteTimer = 0.0f;          // Time remaining in coyote

        // Frame counters for proof
        uint32_t stepsThisFrame = 0;           // [PROOF-JUMP-ONCE]
        bool jumpFiredThisFrame = false;       // [PROOF-JUMP-ONCE]

        // ImGui block proof
        bool blockedThisFrame = false;         // [PROOF-IMGUI-BLOCK-FLUSH]
        bool bufferFlushedByBlock = false;     // [PROOF-IMGUI-BLOCK-FLUSH]

        // Input passthrough (for HUD comparison)
        float moveX = 0.0f;
        float moveZ = 0.0f;
        float yawAxis = 0.0f;  // [TP-LOOK-KEYS] Keyboard yaw input
        bool sprintDown = false;
    };

    //-------------------------------------------------------------------------
    // Public API
    //-------------------------------------------------------------------------

    void Initialize();
    void SetConfig(const ActionConfig& config);
    const ActionConfig& GetConfig();

    /******************************************************************************
     * FUNCTION CONTRACT - BeginFrame
     *
     * PRECONDITIONS
     *   - Called exactly once per frame, AFTER ConsumeFrameInput
     *   - Called BEFORE any fixed-step iteration
     *
     * PARAMETERS
     *   - frame: FrameInput snapshot from GameplayInputSystem
     *   - imguiBlocksGameplay: true if ImGui wants keyboard OR mouse
     *
     * SIDE EFFECTS
     *   - If imguiBlocksGameplay: flush jump buffer, flush coyote timer
     *   - Else: latch jump intent if frame.jumpPressed, store movement
     *
     * PROOF
     *   [PROOF-IMGUI-BLOCK-FLUSH] - bufferFlushedByBlock set when flushing
     ******************************************************************************/
    void BeginFrame(const Engine::FrameInput& frame, bool imguiBlocksGameplay);

    /******************************************************************************
     * FUNCTION CONTRACT - ConsumeForFixedStep
     *
     * PRECONDITIONS
     *   - BeginFrame called this frame
     *   - Called once per fixed-step iteration
     *
     * PARAMETERS
     *   - onGround: current ground state from WorldState (state BEFORE this step)
     *   - fixedDt: fixed timestep (for timer decrement)
     *   - allowJumpThisStep: true only on first step of frame
     *
     * RETURNS
     *   - InputState ready for WorldState::TickFixed
     *
     * SIDE EFFECTS
     *   - Decrements coyote/jump buffer timers by fixedDt
     *   - If jump fires: clears jump buffer
     *   - If !onGround && wasOnGround: starts coyote timer
     *
     * PROOF
     *   [PROOF-JUMP-ONCE] - jump only fires when allowJumpThisStep=true
     *   [PROOF-STEP0-LATCH] - jump buffer persists if no step runs
     ******************************************************************************/
    Engine::InputState ConsumeForFixedStep(bool onGround, float fixedDt, bool allowJumpThisStep);

    /******************************************************************************
     * FUNCTION CONTRACT - EndFrame
     *
     * PRECONDITIONS
     *   - Called exactly once per frame, AFTER all fixed steps
     *
     * SIDE EFFECTS
     *   - If stepCount==0: decrement BOTH timers by frameDt (frame-rate timer)
     *   - Updates debug state for HUD
     *
     * PROOF
     *   [PROOF-STEP0-LATCH] - buffer persists across frames with 0 steps
     ******************************************************************************/
    void EndFrame(uint32_t stepCount, float frameDt);

    // Debug state accessor (called by App::Tick for HUD injection, NOT WorldState)
    const ActionDebugState& GetDebugState();

    // Reset (for WM_KILLFOCUS, respawn, etc.)
    void ResetAllState();
}
