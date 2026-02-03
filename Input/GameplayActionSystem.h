#pragma once
/******************************************************************************
 * FILE CONTRACT - GameplayActionSystem.h (Day4 PR2.3)
 *
 * SCOPE
 *   Owns action-layer buffering for gameplay actions.
 *   Translates FrameInput (RAW) into InputState (per-step).
 *   This is intent buffering/policy, NOT simulation.
 *
 * TERMINOLOGY
 *   - FrameInput: Raw per-frame sample from OS (mouse deltas, key states)
 *   - FrameIntent: Latched per-frame intent (cached movement, buffered jump)
 *   - StepIntent (InputState): Per-fixed-step packet consumed by sim
 *   - ActionSystem: Intent buffering/policy layer, NOT simulation
 *
 * SSOT TIMING POLICY (NO DOUBLE-DECAY)
 *   Timers (jumpBuffer, coyote) must decay exactly once per frame:
 *   - If stepCount > 0: decay ONLY in BuildStepIntent, by fixedDt per step
 *   - If stepCount == 0: decay ONLY in FinalizeFrameIntent, by frameDt once
 *   This prevents timer freezing on low-dt frames and double-decay on high-dt.
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
    // Control Config (SSOT for look sensitivity/rates)
    //
    // PURPOSE:
    //   Centralizes ALL look-related tuning parameters in the Action layer.
    //   This enables consistent behavior across mouse + keyboard look inputs
    //   and eliminates duplicated constants in WorldState.
    //
    // SSOT GUARANTEE:
    //   - mouseSensitivityRadPerPixel: ONLY source for pixel->radian conversion
    //   - keyboardYawRateRadPerSec: ONLY source for Q/E yaw rotation speed
    //   - maxMousePixelsPerFrame: ONLY source for mouse delta clamp
    //
    // CONSUMERS:
    //   - BuildStepIntent(): uses these to compute yawDelta/pitchDelta
    //   - GetPendingLookPreviewRad(): uses sens for C-2 preview calculation
    //
    // NOTE:
    //   WorldState::m_config.mouseSensitivity and lookSpeed are now DEAD CODE
    //   for ThirdPerson mode (kept only for FreeCam compatibility if needed).
    //-------------------------------------------------------------------------
    struct ControlConfig
    {
        float mouseSensitivityRadPerPixel = 0.003f;  // rad/pixel for mouse look
        float keyboardYawRateRadPerSec = 2.0f;       // rad/sec for Q/E yaw
        float maxMousePixelsPerFrame = 120.0f;       // clamp for large mouse jumps
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
        float yawAxis = 0.0f;  // [TP-LOOK-KEYS] Keyboard yaw input (cached)
        bool sprintDown = false;

        // [LOOK-UNIFIED] Pending mouse accumulation (for C-2 preview debug)
        float pendingMouseDX = 0.0f;
        float pendingMouseDY = 0.0f;
    };

    //-------------------------------------------------------------------------
    // Public API
    //-------------------------------------------------------------------------

    void Initialize();
    void SetConfig(const ActionConfig& config);
    const ActionConfig& GetConfig();
    void SetControlConfig(const ControlConfig& config);
    const ControlConfig& GetControlConfig();

    //-------------------------------------------------------------------------
    // StageFrameIntent - Latch per-frame intent from raw input
    //
    // CONTRACT:
    //   - Latches per-frame intent (movement cache) + buffers jump
    //   - Flushes all buffers when ImGui blocks gameplay
    //   - Does NOT touch sim state; does NOT sample OS input
    //   - Called once per frame BEFORE fixed-step loop
    //-------------------------------------------------------------------------
    void StageFrameIntent(const Engine::FrameInput& frame, bool imguiBlocksGameplay);

    //-------------------------------------------------------------------------
    // BuildStepIntent - Produce per-step intent for fixed sim
    //
    // CONTRACT:
    //   - Produces StepIntent (InputState) for one fixed-step iteration
    //   - Decrements timers by fixedDt (part of SSOT timing policy)
    //   - Never samples OS input; uses cached FrameIntent only
    //   - Jump fires only when stepIndex==0 [PROOF-JUMP-ONCE]
    //   - [LOOK-UNIFIED] Computes yawDelta/pitchDelta from pending mouse + keyboard
    //     yaw on first step only; subsequent steps get zero deltas
    //   - stepIndex: 0-based index of current step within frame (App passes stepCount
    //     BEFORE incrementing, so first call gets 0, second gets 1, etc.)
    //   - isThirdPerson: passed from App to avoid layer violation (Action->Renderer)
    //   - Returns InputState ready for WorldState::TickFixed
    //
    // PROOF TAGS:
    //   [PROOF-LOOK-ONCE] - look deltas computed only when stepIndex==0
    //-------------------------------------------------------------------------
    Engine::InputState BuildStepIntent(bool onGround, float fixedDt, uint32_t stepIndex, bool isThirdPerson);

    //-------------------------------------------------------------------------
    // FinalizeFrameIntent - Handle edge-case "0 fixed steps this frame"
    //
    // CONTRACT:
    //   - Handles the edge-case when accumulator < FIXED_DT (no fixed steps ran)
    //   - If stepCount==0: decrement timers by frameDt (prevents timer freeze)
    //   - If stepCount>0: timers already decayed in BuildStepIntent, do nothing
    //   - Updates HUD debug snapshot every frame regardless of stepCount
    //   - Without this, timers would freeze on high-FPS frames causing
    //     inconsistent jump buffer/coyote behavior across varying framerates
    //-------------------------------------------------------------------------
    void FinalizeFrameIntent(uint32_t stepCount, float frameDt);

    //-------------------------------------------------------------------------
    // GetPendingLookPreviewRad - C-2 presentation-only preview
    //
    // PURPOSE:
    //   Returns the pending mouse look intent converted to radians WITHOUT
    //   consuming or clearing the pending values. Used by App to set
    //   presentation-only camera offset when stepCount==0 (no fixed steps ran).
    //
    // CONTRACT:
    //   - Does NOT clear s_pendingMouseDX/DY (preview only, not consumption)
    //   - Returns zero if blocked by ImGui
    //   - Uses ControlConfig::mouseSensitivityRadPerPixel for conversion
    //   - Sign matches ApplyMouseLook convention: mouse right -> negative yaw
    //
    // SSOT BOUNDARY:
    //   - This is presentation-only; sim yaw/pitch is NOT modified
    //   - The offset is applied in WorldState::TickFrame for camera position only
    //
    // PROOF TAGS:
    //   [PROOF-STEP0-LATCH-LOOK] - pending persists across 0-step frames
    //-------------------------------------------------------------------------
    void GetPendingLookPreviewRad(float& outYaw, float& outPitch);

    // Debug state accessor (called by App::Tick for HUD injection, NOT WorldState)
    const ActionDebugState& GetDebugState();

    // Reset (for WM_KILLFOCUS, respawn, etc.)
    void ResetAllState();
}
