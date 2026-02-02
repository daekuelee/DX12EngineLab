/******************************************************************************
 * GameplayInputSystem.cpp — Centralized input state + FrameInput snapshots (PR-A)
 *
 * CONTRACT
 *   - OnWin32Message() observes input; NEVER consumes messages.
 *   - Raw state ALWAYS updates even during ImGui capture.
 *   - ConsumeFrameInput() produces snapshot with ImGui masking applied.
 *   - Held state (WASD/Shift) uses s_keys[].down (SSOT with edges).
 *   - Edges and deltas cleared after consumption.
 *
 * PROOF POINTS
 *   [PROOF-STUCK-KEY]   — Event-tracked key up/down + WM_KILLFOCUS safety reset prevents stuck movement
 *   [PROOF-MOUSE-SPIKE] — lastX/lastY always updated prevents spikes
 *   [PROOF-JUMP-ONCE]   — Edge consumed once in ConsumeFrameInput
 *   [PROOF-SSOT]        — Held + edge inputs derive from the same event-recorded key state (s_keys[])
 ******************************************************************************/

#include "GameplayInputSystem.h"
#include "../Engine/FrameInput.h"
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <cstdio>

namespace GameplayInputSystem
{
    //-------------------------------------------------------------------------
    // Key state tracking
    //-------------------------------------------------------------------------
    struct KeyState
    {
        bool down = false;
        bool pressedThisFrame = false;
        bool releasedThisFrame = false;
    };

    static KeyState s_keys[256] = {};

    //-------------------------------------------------------------------------
    // Mouse state tracking
    //-------------------------------------------------------------------------
    static int s_lastMouseX = 0;
    static int s_lastMouseY = 0;
    static bool s_mouseInitialized = false;
    static float s_pendingMouseDX = 0.0f;
    static float s_pendingMouseDY = 0.0f;

    //-------------------------------------------------------------------------
    // Public API
    //-------------------------------------------------------------------------
    void Initialize()
    {
        ResetAllState();
    }

    /******************************************************************************
     * FUNCTION CONTRACT — OnWin32Message
     *
     * PRECONDITIONS
     *   - Called from WndProc AFTER ImGui forwarding
     *   - Called BEFORE HotkeyRouter
     *
     * SIDE EFFECTS
     *   - WM_KEYDOWN: Sets key.down=true, key.pressedThisFrame=true (if !repeat)
     *   - WM_KEYUP: Sets key.down=false, key.releasedThisFrame=true
     *   - WM_MOUSEMOVE: Updates lastX/lastY (always), accumulates pendingDX/DY
     *   - WM_KILLFOCUS: Calls ResetAllState()
     *
     * RETURNS
     *   - void (NEVER consumes messages)
     *
     * INVARIANTS
     *   - lastX/lastY ALWAYS updated on WM_MOUSEMOVE (prevents spike)
     *   - pressedThisFrame only set when !wasDown && !key.down (edge detection)
     ******************************************************************************/
    void OnWin32Message(HWND, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_KEYDOWN:
        {
            UINT vk = static_cast<UINT>(wParam);
            if (vk >= 256) break;

            // Edge detection: bit30 of lParam indicates previous key state (1 = was down)
            const bool wasDown = ((lParam & 0x40000000) != 0) || s_keys[vk].down;

            if (!wasDown)
            {
                s_keys[vk].pressedThisFrame = true;
#if defined(_DEBUG)
                if (vk == VK_SPACE)
                {
                    OutputDebugStringA("[GameplayInputSystem] WM_KEYDOWN VK_SPACE pressed=1 (edge)\n");
                }
#endif
            }
            s_keys[vk].down = true;
            break;
        }

        case WM_KEYUP:
        {
            UINT vk = static_cast<UINT>(wParam);
            if (vk >= 256) break;

            s_keys[vk].down = false;
            s_keys[vk].releasedThisFrame = true;
            break;
        }

        case WM_MOUSEMOVE:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            if (!s_mouseInitialized)
            {
                // First mouse event - just record position, no delta
                s_lastMouseX = xPos;
                s_lastMouseY = yPos;
                s_mouseInitialized = true;
                break;
            }

            // Accumulate delta for this frame
            // INVARIANT: lastX/lastY ALWAYS updated (prevents spike on capture release)
            s_pendingMouseDX += static_cast<float>(xPos - s_lastMouseX);
            s_pendingMouseDY += static_cast<float>(yPos - s_lastMouseY);
            s_lastMouseX = xPos;
            s_lastMouseY = yPos;
            break;
        }

        case WM_KILLFOCUS:
#if defined(_DEBUG)
            OutputDebugStringA("[GameplayInputSystem] WM_KILLFOCUS -> ResetAllState\n");
#endif
            ResetAllState();
            break;

        default:
            break;
        }
    }

    /******************************************************************************
     * FUNCTION CONTRACT — ConsumeFrameInput
     *
     * PRECONDITIONS
     *   - Called exactly once per App::Tick
     *   - dt > 0.0f
     *
     * SIDE EFFECTS
     *   - Clears all key.pressedThisFrame and key.releasedThisFrame flags
     *   - Clears pendingMouseDX/DY to zero
     *
     * RETURNS
     *   - FrameInput snapshot with:
     *     - blocksGameplay = imguiKeyboard || imguiMouse
     *     - moveX/Z, sprintDown, jumpPressed: from s_keys[] if !blocksGameplay, else zero
     *     - mouseDX/DY: from pendingDX/DY if !imguiMouse, else zero
     *     - blockedByImGui* flags for HUD diagnostics
     *
     * CONSUME-ONCE RULE
     *   - jumpPressed edge is consumed and cleared in this call
     *   - Subsequent calls in same frame would return jumpPressed=false
     ******************************************************************************/
    Engine::FrameInput ConsumeFrameInput(float dt, bool imguiKeyboard, bool imguiMouse)
    {
        Engine::FrameInput frame{};
        frame.dt = dt;
        frame.blockedByImGuiKeyboard = imguiKeyboard;
        frame.blockedByImGuiMouse = imguiMouse;

        // Block gameplay if ImGui captures keyboard OR mouse
        const bool blocksGameplay = imguiKeyboard || imguiMouse;

        if (!blocksGameplay)
        {
            // Movement axes (WASD)
            if (s_keys['W'].down) frame.moveZ += 1.0f;
            if (s_keys['S'].down) frame.moveZ -= 1.0f;
            if (s_keys['A'].down) frame.moveX -= 1.0f;
            if (s_keys['D'].down) frame.moveX += 1.0f;

            // Sprint (Shift)
            frame.sprintDown = s_keys[VK_SHIFT].down;

            // Jump: edge-triggered from event tracking
            // [PROOF-JUMP-ONCE] — Edge consumed here, cleared below
            frame.jumpPressed = s_keys[VK_SPACE].pressedThisFrame;

#if defined(_DEBUG)
            if (frame.jumpPressed)
            {
                OutputDebugStringA("[GameplayInputSystem] ConsumeFrameInput: jumpPressed=1 blocksGameplay=0 -> FIRE\n");
            }
#endif
        }
        // No else block needed - fields are already zero-initialized

        // Mouse: use accumulated deltas
        if (!imguiMouse)
        {
            frame.mouseDX = s_pendingMouseDX;
            frame.mouseDY = s_pendingMouseDY;
        }
        // else: mouse deltas zeroed (default initialized)

        // Clear edges and deltas for next frame
        for (int i = 0; i < 256; ++i)
        {
            s_keys[i].pressedThisFrame = false;
            s_keys[i].releasedThisFrame = false;
        }
        s_pendingMouseDX = 0.0f;
        s_pendingMouseDY = 0.0f;

#if defined(_DEBUG)
        if (frame.jumpPressed)
        {
            OutputDebugStringA("[GameplayInputSystem] ConsumeFrameInput: jumpPressed=0 (cleared)\n");
        }
#endif

        return frame;
    }

    void ResetAllState()
    {
        for (int i = 0; i < 256; ++i)
        {
            s_keys[i] = {};
        }
        s_pendingMouseDX = 0.0f;
        s_pendingMouseDY = 0.0f;
        // Note: s_mouseInitialized and s_lastMouseX/Y are NOT reset
        // This prevents spike on next mouse move after focus return
    }

}  // namespace GameplayInputSystem
