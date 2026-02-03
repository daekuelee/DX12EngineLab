/******************************************************************************
 * GameplayActionSystem.cpp - Action layer buffering (Day4 PR2.2)
 *
 * CONTRACT
 *   - BeginFrame() caches movement, latches jump intent, flushes on ImGui block
 *   - ConsumeForFixedStep() produces InputState per fixed-step, decays timers by fixedDt
 *   - EndFrame() handles stepCount==0 timer decay, updates debug state
 *   - ResetAllState() clears all buffers (WM_KILLFOCUS, respawn)
 *
 * TIMING POLICY
 *   - stepCount > 0: timers decay by fixedDt per step in ConsumeForFixedStep ONLY
 *   - stepCount == 0: timers decay by frameDt once in EndFrame ONLY
 *   - NO double-decay: EndFrame guards with if (stepCount == 0)
 *
 * PROOF POINTS
 *   [PROOF-STEP0-LATCH]       - stepCount==0 doesn't lose jump
 *   [PROOF-JUMP-ONCE]         - multiple steps -> jump fires once
 *   [PROOF-IMGUI-BLOCK-FLUSH] - ImGui capture flushes buffer
 ******************************************************************************/

#include "GameplayActionSystem.h"
#include "../Renderer/DX12/ToggleSystem.h"
#include <cstdio>

#if defined(_DEBUG)
#include <Windows.h>
#endif

namespace GameplayActionSystem
{
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    static ActionConfig s_config;
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
    static uint32_t s_stepsThisFrame = 0;
    static bool s_blockedThisFrame = false;
    static bool s_bufferFlushedByBlock = false;

    //-------------------------------------------------------------------------
    // Public API
    //-------------------------------------------------------------------------

    void Initialize()
    {
        s_config = ActionConfig{};
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

    void BeginFrame(const Engine::FrameInput& frame, bool imguiBlocksGameplay)
    {
        // Reset per-frame tracking
        s_jumpFiredThisFrame = false;
        s_stepsThisFrame = 0;
        s_blockedThisFrame = imguiBlocksGameplay;
        s_bufferFlushedByBlock = false;

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

            // Zero out movement cache
            s_cached = {};
            return;
        }

        // Cache movement for all steps this frame
        s_cached.moveX = frame.moveX;
        s_cached.moveZ = frame.moveZ;
        s_cached.yawAxis = frame.yawAxis;
        s_cached.sprintDown = frame.sprintDown;

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

    Engine::InputState ConsumeForFixedStep(bool onGround, float fixedDt, bool allowJumpThisStep)
    {
        Engine::InputState input;

        // Movement always passes through (if not blocked)
        if (!s_blockedThisFrame)
        {
            input.moveX = s_cached.moveX;
            input.moveZ = s_cached.moveZ;
            input.sprint = s_cached.sprintDown;

            // [MODE-GATE] Keyboard yaw ONLY in ThirdPerson mode
            // FreeCam Q/E uses separate GetAsyncKeyState path - must not drift pawn
            if (Renderer::ToggleSystem::GetCameraMode() == Renderer::CameraMode::ThirdPerson)
            {
                input.yawAxis = s_cached.yawAxis;

#if defined(_DEBUG)
                // [SIGN-PROOF] Throttled proof log when yawAxis active
                static uint32_t s_proofFrameCounter = 0;
                if (s_cached.yawAxis != 0.0f && (++s_proofFrameCounter % 60 == 0))
                {
                    char buf[128];
                    sprintf_s(buf, "[TP-LOOK-KEYS] yawAxis=%.1f mode=ThirdPerson\n", s_cached.yawAxis);
                    OutputDebugStringA(buf);
                }
#endif
            }
        }

        s_stepsThisFrame++;

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
        bool canJump = allowJumpThisStep && s_jumpBuffered && !s_jumpFiredThisFrame && !s_blockedThisFrame;
        bool hasGround = onGround || (s_coyoteTimer > 0.0f);

        if (canJump && hasGround)
        {
            input.jump = true;
            s_jumpFiredThisFrame = true;
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

    void EndFrame(uint32_t stepCount, float frameDt)
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
        }

        // Update debug state for HUD (always, regardless of stepCount)
        s_debugState.jumpBuffered = s_jumpBuffered;
        s_debugState.jumpBufferTimer = s_jumpBufferTimer;
        s_debugState.coyoteActive = (s_coyoteTimer > 0.0f);
        s_debugState.coyoteTimer = s_coyoteTimer;
        s_debugState.stepsThisFrame = s_stepsThisFrame;
        s_debugState.jumpFiredThisFrame = s_jumpFiredThisFrame;
        s_debugState.blockedThisFrame = s_blockedThisFrame;
        s_debugState.bufferFlushedByBlock = s_bufferFlushedByBlock;
        s_debugState.moveX = s_cached.moveX;
        s_debugState.moveZ = s_cached.moveZ;
        s_debugState.yawAxis = s_cached.yawAxis;
        s_debugState.sprintDown = s_cached.sprintDown;
    }

    const ActionDebugState& GetDebugState()
    {
        return s_debugState;
    }

    void ResetAllState()
    {
        s_jumpBuffered = false;
        s_jumpBufferTimer = 0.0f;
        s_wasOnGroundLastStep = true;
        s_coyoteTimer = 0.0f;
        s_cached = {};
        s_jumpFiredThisFrame = false;
        s_stepsThisFrame = 0;
        s_blockedThisFrame = false;
        s_bufferFlushedByBlock = false;
        s_debugState = ActionDebugState{};

#if defined(_DEBUG)
        OutputDebugStringA("[ActionSystem] ResetAllState\n");
#endif
    }

}  // namespace GameplayActionSystem
