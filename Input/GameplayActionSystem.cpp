/******************************************************************************
 * GameplayActionSystem.cpp - Action layer buffering (Day4 PR2.3)
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
 *   This prevents timer freezing on high-FPS frames and double-decay on low-FPS.
 *
 * CONTRACT
 *   - StageFrameIntent(): latches FrameIntent + buffers jump; flushes when ImGui blocks
 *   - BuildStepIntent(): produces StepIntent for fixed sim; decrements timers by fixedDt
 *   - FinalizeFrameIntent(): handles "0 fixed steps" edge-case; updates HUD debug
 *   - ResetAllState(): clears all buffers (WM_KILLFOCUS, respawn)
 *
 * PROOF POINTS
 *   [PROOF-STEP0-LATCH]       - stepCount==0 doesn't lose jump
 *   [PROOF-JUMP-ONCE]         - multiple steps -> jump fires once
 *   [PROOF-IMGUI-BLOCK-FLUSH] - ImGui capture flushes buffer
 ******************************************************************************/

#include "GameplayActionSystem.h"
#include <cstdio>
#include <cmath>  // fmaxf/fminf for clamp

#if defined(_DEBUG)
#include <Windows.h>
#endif

namespace GameplayActionSystem
{
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    static ActionConfig s_config;
    static ControlConfig s_controlConfig;
    static ActionDebugState s_debugState;

    // Jump buffer
    static bool s_jumpBuffered = false;
    static float s_jumpBufferTimer = 0.0f;

    // Coyote time
    static bool s_wasOnGroundLastStep = true;
    static float s_coyoteTimer = 0.0f;

    // [CACHE-PATTERN] Per-frame input cache (from BeginFrame)
    // All cached inputs grouped in one struct for consistency
    struct CachedFrameInput
    {
        float moveX = 0.0f;
        float moveZ = 0.0f;
        float yawAxis = 0.0f;
        bool sprintDown = false;
    };
    static CachedFrameInput s_cached = {};

    // Per-frame tracking
    static bool s_jumpFiredThisFrame = false;
    static bool s_blockedThisFrame = false;
    static bool s_bufferFlushedByBlock = false;

    // [LOOK-UNIFIED] Pending mouse accumulation (pixels, consumed on first step)
    static float s_pendingMouseDX = 0.0f;
    static float s_pendingMouseDY = 0.0f;

#if defined(_DEBUG)
    // PR2.5: Per-frame proof accumulators (reset in StageFrameIntent)
    static float s_proofSumKbYaw = 0.0f;
    static float s_proofSumMouseYaw = 0.0f;
    static uint32_t s_proofJumpFiredCount = 0;
#endif

    //-------------------------------------------------------------------------
    // Public API
    //-------------------------------------------------------------------------

    void Initialize()
    {
        s_config = ActionConfig{};
        s_controlConfig = ControlConfig{};
        ResetAllState();
    }

    void SetConfig(const ActionConfig& config)
    {
        s_config = config;
    }

    const ActionConfig& GetConfig()
    {
        return s_config;
    }

    void SetControlConfig(const ControlConfig& config)
    {
        s_controlConfig = config;
    }

    const ControlConfig& GetControlConfig()
    {
        return s_controlConfig;
    }

    //-------------------------------------------------------------------------
    // StageFrameIntent
    //
    // PURPOSE:
    //   Latches per-frame intent from raw FrameInput. Caches movement axes,
    //   buffers jump press, and prepares state for subsequent BuildStepIntent calls.
    //
    // WHEN CALLED:
    //   Once per frame, AFTER GameplayInputSystem::ConsumeFrameInput(),
    //   BEFORE the fixed-step loop begins. Order: ConsumeFrameInput -> StageFrameIntent -> loop.
    //
    // SSOT OWNERSHIP (what this function mutates):
    //   - s_cached (movement/yaw cache for this frame)
    //   - s_jumpBuffered, s_jumpBufferTimer (latch jump intent)
    //   - s_blockedThisFrame, s_bufferFlushedByBlock (per-frame tracking)
    //   - s_jumpFiredThisFrame (reset to false)
    //
    // MUST NOT TOUCH:
    //   - s_coyoteTimer (except on ImGui flush)
    //   - s_wasOnGroundLastStep (owned by BuildStepIntent)
    //   - Any sim state (WorldState pawn, physics)
    //
    // EDGE CASES:
    //   - ImGui block: flush jump buffer + coyote timer immediately
    //     [PROOF-IMGUI-BLOCK-FLUSH] validates this via s_bufferFlushedByBlock flag
    //   - No jump pressed: s_jumpBuffered unchanged (may persist from buffer)
    //
    // PROOF TAGS:
    //   [PROOF-IMGUI-BLOCK-FLUSH] - set s_bufferFlushedByBlock when flushing
    //-------------------------------------------------------------------------
    void StageFrameIntent(const Engine::FrameInput& frame, bool imguiBlocksGameplay)
    {
        // Reset per-frame tracking
        s_jumpFiredThisFrame = false;
        s_blockedThisFrame = imguiBlocksGameplay;
        s_bufferFlushedByBlock = false;

#if defined(_DEBUG)
        s_proofSumKbYaw = 0.0f;
        s_proofSumMouseYaw = 0.0f;
        s_proofJumpFiredCount = 0;
#endif

        if (imguiBlocksGameplay)
        {
            // [PROOF-IMGUI-BLOCK-FLUSH] - flush all buffers when ImGui blocks
            if (s_jumpBuffered || s_coyoteTimer > 0.0f)
            {
                s_bufferFlushedByBlock = true;
#if defined(_DEBUG)
                OutputDebugStringA("[ActionSystem] [PROOF-IMGUI-BLOCK-FLUSH] Buffers flushed\n");
#endif
            }
            s_jumpBuffered = false;
            s_jumpBufferTimer = 0.0f;
            s_coyoteTimer = 0.0f;

            // [LOOK-UNIFIED] Flush pending mouse on ImGui block
            s_pendingMouseDX = 0.0f;
            s_pendingMouseDY = 0.0f;

            // Zero out movement cache
            s_cached = {};
            return;
        }

        // Cache movement for all steps this frame
        s_cached.moveX = frame.moveX;
        s_cached.moveZ = frame.moveZ;
        s_cached.yawAxis = frame.yawAxis;
        s_cached.sprintDown = frame.sprintDown;

        // [LOOK-UNIFIED] Accumulate pending mouse deltas (pixels)
        s_pendingMouseDX += frame.mouseDX;
        s_pendingMouseDY += frame.mouseDY;

        // Clamp to prevent extreme spikes (unit: pixels)
        const float maxPx = s_controlConfig.maxMousePixelsPerFrame;
#if defined(_DEBUG)
        float beforeX = s_pendingMouseDX, beforeY = s_pendingMouseDY;
#endif
        if (s_pendingMouseDX > maxPx) s_pendingMouseDX = maxPx;
        if (s_pendingMouseDX < -maxPx) s_pendingMouseDX = -maxPx;
        if (s_pendingMouseDY > maxPx) s_pendingMouseDY = maxPx;
        if (s_pendingMouseDY < -maxPx) s_pendingMouseDY = -maxPx;

#if defined(_DEBUG)
        // [PROOF-CLAMP] Only log when clamp actually changed values
        if (beforeX != s_pendingMouseDX || beforeY != s_pendingMouseDY)
        {
            static uint32_t s_clampProofCounter = 0;
            if (++s_clampProofCounter % 60 == 0)
            {
                char buf[128];
                sprintf_s(buf, "[PROOF-CLAMP] before=(%.0f,%.0f) after=(%.0f,%.0f)\n",
                    beforeX, beforeY, s_pendingMouseDX, s_pendingMouseDY);
                OutputDebugStringA(buf);
            }
        }
#endif

        // Latch jump intent if pressed this frame
        if (frame.jumpPressed)
        {
            s_jumpBuffered = true;
            s_jumpBufferTimer = s_config.jumpBufferDuration;
#if defined(_DEBUG)
            char buf[128];
            sprintf_s(buf, "[ActionSystem] Jump latched, buffer=%.3fs\n", s_jumpBufferTimer);
            OutputDebugStringA(buf);
#endif
        }
    }

    //-------------------------------------------------------------------------
    // BuildStepIntent
    //
    // PURPOSE:
    //   Produces a StepIntent (InputState) for one fixed-step physics iteration.
    //   Evaluates buffered jump intent, coyote time, and emits per-step input.
    //   [LOOK-UNIFIED] Computes yawDelta/pitchDelta from pending mouse + keyboard yaw.
    //
    // WHEN CALLED:
    //   Once per fixed-step iteration, inside the while(accumulator >= FIXED_DT) loop.
    //   May be called 0-15 times per frame depending on accumulator.
    //
    // SSOT OWNERSHIP (what this function mutates):
    //   - s_jumpFiredThisFrame (set true when jump fires)
    //   - s_jumpBuffered, s_jumpBufferTimer (consumed when jump fires)
    //   - s_coyoteTimer (decremented by fixedDt per step)
    //   - s_wasOnGroundLastStep (updated at end for next step)
    //   - s_pendingMouseDX/DY (consumed on first step only)
    //
    // MUST NOT TOUCH:
    //   - OS input sampling (uses cached FrameIntent only)
    //   - Any sim state (WorldState pawn, physics) - caller applies InputState
    //   - Renderer state (isThirdPerson passed in to avoid layer violation)
    //
    // EDGE CASES:
    //   - stepIndex gating: jump can ONLY fire when stepIndex==0
    //     [PROOF-JUMP-ONCE] validates that multiple steps -> jump fires once
    //   - [PROOF-LOOK-ONCE] look deltas computed only when stepIndex==0
    //   - Coyote time: starts when leaving ground, consumed on jump
    //   - Timer decay: decremented by fixedDt each step (SSOT timing policy)
    //
    // PROOF TAGS:
    //   [PROOF-JUMP-ONCE] - jump fires only on first step of frame
    //   [PROOF-LOOK-ONCE] - look deltas computed only on first step of frame
    //   [PROOF-STEP0-LATCH] - buffer persists if no step runs (validated in Finalize)
    //-------------------------------------------------------------------------
    Engine::InputState BuildStepIntent(bool onGround, float fixedDt, uint32_t stepIndex, bool isThirdPerson)
    {
        // Derive isFirstStep from stepIndex (mathematically equivalent to App's previous flip pattern)
        const bool isFirstStep = (stepIndex == 0);

        Engine::InputState input;

        // Movement always passes through (if not blocked)
        if (!s_blockedThisFrame)
        {
            input.moveX = s_cached.moveX;
            input.moveZ = s_cached.moveZ;
            input.sprint = s_cached.sprintDown;

            // [LOOK-UNIFIED] Decomposed look delta:
            //   Keyboard yaw: CONTINUOUS RATE — integrated every step
            //   Mouse yaw/pitch: IMPULSE — consumed on step 0 only
            if (isThirdPerson)
            {
                const float rate = s_controlConfig.keyboardYawRateRadPerSec;
                float kbYaw = s_cached.yawAxis * rate * fixedDt;
                input.yawDelta = kbYaw;

                float mouseYaw = 0.0f;
                if (isFirstStep)
                {
                    const float sens = s_controlConfig.mouseSensitivityRadPerPixel;
                    mouseYaw = -(s_pendingMouseDX * sens);
                    input.yawDelta += mouseYaw;
                    input.pitchDelta = -(s_pendingMouseDY * sens);

                    // Consume pending mouse (once per frame)
                    s_pendingMouseDX = 0.0f;
                    s_pendingMouseDY = 0.0f;
                }

#if defined(_DEBUG)
                s_proofSumKbYaw += kbYaw;
                if (isFirstStep) s_proofSumMouseYaw = mouseYaw;
#endif
            }
        }

        // Coyote time logic: start timer when leaving ground
        if (s_wasOnGroundLastStep && !onGround && !s_blockedThisFrame)
        {
            s_coyoteTimer = s_config.coyoteTimeDuration;
#if defined(_DEBUG)
            char buf[128];
            sprintf_s(buf, "[ActionSystem] Coyote timer started: %.3fs\n", s_coyoteTimer);
            OutputDebugStringA(buf);
#endif
        }

        // Determine if jump can fire
        bool canJump = isFirstStep && s_jumpBuffered && !s_jumpFiredThisFrame && !s_blockedThisFrame;
        bool hasGround = onGround || (s_coyoteTimer > 0.0f);

        if (canJump && hasGround)
        {
            input.jump = true;
            s_jumpFiredThisFrame = true;
#if defined(_DEBUG)
            s_proofJumpFiredCount++;
#endif
            s_jumpBuffered = false;
            s_jumpBufferTimer = 0.0f;

#if defined(_DEBUG)
            char buf[128];
            if (!onGround && s_coyoteTimer > 0.0f)
            {
                sprintf_s(buf, "[ActionSystem] [PROOF-JUMP-ONCE] Jump fired (coyote=%.3fs)\n", s_coyoteTimer);
            }
            else
            {
                sprintf_s(buf, "[ActionSystem] [PROOF-JUMP-ONCE] Jump fired (onGround=true)\n");
            }
            OutputDebugStringA(buf);
#endif
            // Consume coyote time on jump
            s_coyoteTimer = 0.0f;
        }

        // Decrement timers (fixed-step deterministic)
        if (s_coyoteTimer > 0.0f)
        {
            s_coyoteTimer -= fixedDt;
            if (s_coyoteTimer < 0.0f) s_coyoteTimer = 0.0f;
        }

        if (s_jumpBufferTimer > 0.0f && !s_jumpFiredThisFrame)
        {
            s_jumpBufferTimer -= fixedDt;
            if (s_jumpBufferTimer <= 0.0f)
            {
                s_jumpBuffered = false;
                s_jumpBufferTimer = 0.0f;
#if defined(_DEBUG)
                OutputDebugStringA("[ActionSystem] Jump buffer expired (fixed-step)\n");
#endif
            }
        }

        s_wasOnGroundLastStep = onGround;

        return input;
    }

    //-------------------------------------------------------------------------
    // FinalizeFrameIntent
    //
    // PURPOSE:
    //   Handles the edge-case where stepCount==0 (no fixed steps ran this frame).
    //   When accumulator < FIXED_DT, the fixed loop runs 0 times, so timers would
    //   freeze without this fallback decay. Also updates HUD debug snapshot every frame.
    //
    // WHEN CALLED:
    //   Once per frame, AFTER the fixed-step loop completes.
    //   Order: StageFrameIntent -> loop(BuildStepIntent) -> FinalizeFrameIntent.
    //
    // SSOT OWNERSHIP (what this function mutates):
    //   - s_jumpBufferTimer, s_jumpBuffered (decay ONLY if stepCount==0)
    //   - s_coyoteTimer (decay ONLY if stepCount==0)
    //   - s_debugState (always updated for HUD)
    //
    // MUST NOT TOUCH:
    //   - Timers when stepCount>0 (already decayed in BuildStepIntent)
    //   - Any sim state (WorldState pawn, physics)
    //
    // EDGE CASES:
    //   - stepCount==0: accumulator < FIXED_DT means no sim steps ran.
    //     Without timer decay here, jump buffer/coyote would freeze on high-FPS.
    //     Decay by frameDt once to match real elapsed time.
    //   - stepCount>0: do NOT decay (would cause double-decay since BuildStepIntent
    //     already decremented by fixedDt per step)
    //
    // PROOF TAGS:
    //   [PROOF-STEP0-LATCH] - proves buffer persists across 0-step frames
    //-------------------------------------------------------------------------
    void FinalizeFrameIntent(uint32_t stepCount, float frameDt)
    {
        // CRITICAL: Only decay timers when no fixed steps ran (prevents double-decay)
        if (stepCount == 0)
        {
            // [PROOF-STEP0-LATCH] - decay both timers by frameDt
            if (s_jumpBufferTimer > 0.0f)
            {
                s_jumpBufferTimer -= frameDt;
                if (s_jumpBufferTimer <= 0.0f)
                {
                    s_jumpBuffered = false;
                    s_jumpBufferTimer = 0.0f;
#if defined(_DEBUG)
                    OutputDebugStringA("[ActionSystem] Jump buffer expired (frame-rate, stepCount=0)\n");
#endif
                }
                else
                {
#if defined(_DEBUG)
                    char buf[128];
                    sprintf_s(buf, "[ActionSystem] [PROOF-STEP0-LATCH] stepCount=0, bufferTimer=%.3f\n", s_jumpBufferTimer);
                    OutputDebugStringA(buf);
#endif
                }
            }

            if (s_coyoteTimer > 0.0f)
            {
                s_coyoteTimer -= frameDt;
                if (s_coyoteTimer < 0.0f) s_coyoteTimer = 0.0f;
            }

#if defined(_DEBUG)
            // [PROOF-STEP0-LATCH-LOOK] - log pending mouse when no steps ran
            if (s_pendingMouseDX != 0.0f || s_pendingMouseDY != 0.0f)
            {
                float previewYaw = -(s_pendingMouseDX * s_controlConfig.mouseSensitivityRadPerPixel);
                float previewPitch = -(s_pendingMouseDY * s_controlConfig.mouseSensitivityRadPerPixel);
                char buf[128];
                sprintf_s(buf, "[PROOF-STEP0-LATCH-LOOK] pending=(%.0f,%.0f)px preview=(%.4f,%.4f)rad\n",
                    s_pendingMouseDX, s_pendingMouseDY, previewYaw, previewPitch);
                OutputDebugStringA(buf);
            }
#endif
        }

        // Update debug state for HUD (always, regardless of stepCount)
        s_debugState.jumpBuffered = s_jumpBuffered;
        s_debugState.jumpBufferTimer = s_jumpBufferTimer;
        s_debugState.coyoteActive = (s_coyoteTimer > 0.0f);
        s_debugState.coyoteTimer = s_coyoteTimer;
        s_debugState.stepsThisFrame = stepCount;
        s_debugState.jumpFiredThisFrame = s_jumpFiredThisFrame;
        s_debugState.blockedThisFrame = s_blockedThisFrame;
        s_debugState.bufferFlushedByBlock = s_bufferFlushedByBlock;
        s_debugState.moveX = s_cached.moveX;
        s_debugState.moveZ = s_cached.moveZ;
        s_debugState.yawAxis = s_cached.yawAxis;
        s_debugState.sprintDown = s_cached.sprintDown;
        s_debugState.pendingMouseDX = s_pendingMouseDX;
        s_debugState.pendingMouseDY = s_pendingMouseDY;

#if defined(_DEBUG)
        // PR2.5: Hitch-only proof summary — instantly judgeable
        // Pass criterion: kbYawPerStep ≈ constant 0.03333 (= yawAxis * 2.0 * 1/60)
        if (stepCount > 1 && (s_proofSumKbYaw != 0.0f || s_proofSumMouseYaw != 0.0f || s_proofJumpFiredCount > 0))
        {
            float kbYawPerStep = s_proofSumKbYaw / static_cast<float>(stepCount);
            char buf[200];
            sprintf_s(buf, "[PROOF-LOOK-SPLIT] steps=%u kbYawSum=%.5f kbYawPerStep=%.5f mouseYaw=%.5f jumpFired=%u\n",
                stepCount, s_proofSumKbYaw, kbYawPerStep, s_proofSumMouseYaw, s_proofJumpFiredCount);
            OutputDebugStringA(buf);
        }
#endif
    }

    const ActionDebugState& GetDebugState()
    {
        return s_debugState;
    }

    //-------------------------------------------------------------------------
    // GetPendingLookPreviewRad
    //
    // PURPOSE:
    //   Returns the pending mouse look intent converted to radians WITHOUT
    //   consuming the pending values. Used for C-2 presentation-only preview.
    //
    // SSOT BOUNDARY:
    //   - Does NOT clear s_pendingMouseDX/DY (preview only, not consumption)
    //   - Returns zero if blocked by ImGui
    //   - Sign matches ApplyMouseLook: mouse right -> negative yaw
    //-------------------------------------------------------------------------
    void GetPendingLookPreviewRad(float& outYaw, float& outPitch)
    {
        if (s_blockedThisFrame)
        {
            outYaw = 0.0f;
            outPitch = 0.0f;
            return;
        }

        const float sens = s_controlConfig.mouseSensitivityRadPerPixel;
        outYaw = -(s_pendingMouseDX * sens);
        outPitch = -(s_pendingMouseDY * sens);
        // NOTE: Does NOT clear pending - this is preview only
    }

    void ResetAllState()
    {
        s_jumpBuffered = false;
        s_jumpBufferTimer = 0.0f;
        s_wasOnGroundLastStep = true;
        s_coyoteTimer = 0.0f;
        s_cached = {};
        s_jumpFiredThisFrame = false;
        s_blockedThisFrame = false;
        s_bufferFlushedByBlock = false;
        s_pendingMouseDX = 0.0f;
        s_pendingMouseDY = 0.0f;
        s_debugState = ActionDebugState{};

#if defined(_DEBUG)
        OutputDebugStringA("[ActionSystem] ResetAllState\n");
#endif
    }

}  // namespace GameplayActionSystem
