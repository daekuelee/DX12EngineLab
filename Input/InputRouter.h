#pragma once
/******************************************************************************
 * InputRouter.h — Table-driven engine hotkey routing (PR1)
 *
 * CONTRACT
 *   OnWin32Message() returns true if consumed by engine routing.
 *   Caller: if true, return 0; else, return DefWindowProc().
 *   Handles: WM_KEYDOWN, WM_KEYUP, WM_MOUSEMOVE, WM_KILLFOCUS
 *
 * SCOPE (PR1)
 *   Hotkey edge gating + mouse forward only.
 *   WASD movement remains polled in App::Tick.
 ******************************************************************************/

#include <Windows.h>

namespace Engine { class App; }

namespace InputRouter
{
    void Initialize(Engine::App* app);
    bool OnWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void ResetKeyStates();
}
