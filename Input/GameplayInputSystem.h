#pragma once
/******************************************************************************
 * FILE CONTRACT — GameplayInputSystem.h (PR2)
 *
 * SCOPE
 *   Owns all gameplay input state (keyboard + mouse).
 *   Does NOT own hotkey/toggle handling (that's HotkeyRouter).
 *
 * THREAD MODEL
 *   Called from UI thread only (WndProc context for events, Tick for snapshot).
 *
 * OBSERVATION RULES
 *   OnWin32Message() observes input; NEVER consumes messages.
 *   Raw state ALWAYS updates even during ImGui capture.
 *   This prevents stuck keys and mouse spikes.
 *
 * SNAPSHOT RULES
 *   ConsumeFrameInput() called exactly once per App::Tick.
 *   Edges (pressedThisFrame) cleared after snapshot.
 *   Mouse deltas cleared after snapshot.
 *   ImGui masking applied at snapshot time, not observation time.
 *
 * INVARIANTS
 *   - Mouse lastX/lastY always updated (prevents spike on capture release)
 *   - Key edges set only on WM_KEYDOWN with !wasDown (prevents repeat edge)
 *   - ConsumeFrameInput must be called once per frame (edges lost if skipped)
 *
 * PROOF POINTS
 *   [PROOF-STUCK-KEY], [PROOF-HOLD-KEY], [PROOF-MOUSE-SPIKE], [PROOF-JUMP-ONCE]
 ******************************************************************************/

#include <Windows.h>

namespace Engine { struct FrameInput; }

namespace GameplayInputSystem
{
    void Initialize();
    void OnWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    Engine::FrameInput ConsumeFrameInput(float dt, bool imguiKeyboard, bool imguiMouse);
    void ResetAllState();
}
