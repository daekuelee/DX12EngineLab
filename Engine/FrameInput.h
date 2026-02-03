#pragma once
/******************************************************************************
 * FILE CONTRACT — FrameInput.h (PR2: GameplayInputSystem)
 *
 * SCOPE
 *   Snapshot of all gameplay input for a single frame.
 *   Produced by GameplayInputSystem::ConsumeFrameInput().
 *   Consumed by App::Tick() for movement, jumping, camera control.
 *
 * DESIGN NOTES
 *   - Edge flags (jumpPressed) are consumed once per frame
 *   - Hold flags (sprintDown) reflect current state
 *   - Deltas (mouseDX/DY) accumulate between frames, cleared on consume
 *   - Diagnostic flags indicate what was masked by ImGui
 *
 * PROOF POINTS
 *   [PROOF-JUMP-ONCE] — jumpPressed consumed on first fixed step only
 ******************************************************************************/

namespace Engine
{
    struct FrameInput
    {
        float dt = 0.0f;

        // Movement (masked if imguiKeyboard)
        float moveX = 0.0f;    // -1 to +1 (A/D)
        float moveZ = 0.0f;    // -1 to +1 (W/S)
        float yawAxis = 0.0f;  // [TP-LOOK-KEYS] Q/E keyboard yaw (-1 to +1)

        // Mouse look (masked if imguiMouse)
        float mouseDX = 0.0f;
        float mouseDY = 0.0f;

        // Actions
        bool jumpPressed = false;   // Edge, consumed once
        bool sprintDown = false;    // Held state

        // Diagnostics
        bool blockedByImGuiKeyboard = false;
        bool blockedByImGuiMouse = false;
    };
}
