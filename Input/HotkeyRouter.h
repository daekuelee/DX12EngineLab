#pragma once
/******************************************************************************
 * FILE CONTRACT — HotkeyRouter.h (PR-A: Renamed from InputRouter)
 *
 * SCOPE
 *   Owns engine hotkey bindings (T/G/C/U/V/O/F1-F9).
 *   Does NOT handle mouse (GameplayInputSystem owns mouse).
 *   Does NOT handle gameplay keys (WASD/Space/Shift).
 *
 * CONSUMPTION RULES
 *   OnWin32Message() returns true if hotkey consumed.
 *   Caller: if true, return 0; else, pass to DefWindowProc.
 *   Handles: WM_KEYDOWN, WM_KEYUP, WM_KILLFOCUS
 *   Does NOT handle: WM_MOUSEMOVE (GameplayInputSystem)
 *
 * EDGE GATING
 *   Dual-layer: lParam bit30 + s_keyWasDown array.
 *   Blocked by ImGui WantsKeyboard().
 *
 * PROOF POINTS
 *   [PROOF-HOTKEY-EDGE] — T/F7 blocked on repeat, blocked when ImGui captures
 ******************************************************************************/

#include <Windows.h>

namespace Engine { class App; }

namespace HotkeyRouter
{
    void Initialize(Engine::App* app);
    bool OnWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void ResetKeyStates();
}
