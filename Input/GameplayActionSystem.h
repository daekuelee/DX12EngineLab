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
    //   - Jump fires only when isFirstStep=true [PROOF-JUMP-ONCE]
    //   - Returns InputState ready for WorldState::TickFixed
    //-------------------------------------------------------------------------
    Engine::InputState BuildStepIntent(bool onGround, float fixedDt, bool isFirstStep);

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

    // Debug state accessor (called by App::Tick for HUD injection, NOT WorldState)
    const ActionDebugState& GetDebugState();

    // Reset (for WM_KILLFOCUS, respawn, etc.)
    void ResetAllState();
}
