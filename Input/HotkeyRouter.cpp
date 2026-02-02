/******************************************************************************
 * HotkeyRouter.cpp — Table-driven engine hotkey routing (PR-A)
 *
 * CONTRACT
 *   - OnWin32Message() returns true if message consumed by engine.
 *   - Edge gating: toggles fire once per physical press (lParam bit30 + s_keyWasDown).
 *   - ImGui capture: if WantsKeyboard() true, hotkey is blocked.
 *   - WM_KILLFOCUS: resets all key states (prevents stuck keys after Alt-Tab).
 *   - Does NOT handle WM_MOUSEMOVE (GameplayInputSystem owns mouse).
 *
 * PROOF POINTS
 *   [PROOF-HOTKEY-EDGE] — T/F7 blocked on repeat, blocked when ImGui captures
 ******************************************************************************/

#include "HotkeyRouter.h"
#include "../Engine/App.h"
#include "../Renderer/DX12/ToggleSystem.h"
#include "../Renderer/DX12/ImGuiLayer.h"
#include <cstdio>

namespace HotkeyRouter
{
    //-------------------------------------------------------------------------
    // Forward declarations for handler functions
    //-------------------------------------------------------------------------
    static void HandleToggleDrawMode();
    static void HandleToggleGrid();
    static void HandleCycleColorMode();
    static void HandleToggleUploadDiag();
    static void HandleToggleCameraMode();
    static void HandleToggleOpaquePSO();
    static void HandleSentinelInstance0();
    static void HandleStompLifetime();
    static void HandleToggleControllerMode();
    static void HandleToggleStepUpGridTest();
    static void HandleToggleHudVerbose();
    static void HandleToggleDebugSingleInstance();

    //-------------------------------------------------------------------------
    // Binding table: vk + handler + name
    //-------------------------------------------------------------------------
    struct Binding
    {
        UINT vk;
        void (*handler)();
        const char* name;
    };

    static constexpr Binding s_bindings[] =
    {
        { 'C',    HandleCycleColorMode,             "CycleColorMode"   },
        { 'G',    HandleToggleGrid,                 "ToggleGrid"       },
        { 'O',    HandleToggleOpaquePSO,            "ToggleOpaquePSO"  },
        { 'T',    HandleToggleDrawMode,             "ToggleDrawMode"   },
        { 'U',    HandleToggleUploadDiag,           "ToggleUploadDiag" },
        { 'V',    HandleToggleCameraMode,           "ToggleCameraMode" },
        { VK_F1,  HandleSentinelInstance0,          "SentinelInst0"    },
        { VK_F2,  HandleStompLifetime,              "StompLifetime"    },
        { VK_F6,  HandleToggleControllerMode,       "ControllerMode"   },
        { VK_F7,  HandleToggleStepUpGridTest,       "StepUpGridTest"   },
        { VK_F8,  HandleToggleHudVerbose,           "HudVerbose"       },
        { VK_F9,  HandleToggleDebugSingleInstance,  "DebugSingleInst"  },
    };
    static constexpr size_t kBindingCount = sizeof(s_bindings) / sizeof(s_bindings[0]);

    //-------------------------------------------------------------------------
    // Module state
    //-------------------------------------------------------------------------
    static Engine::App* s_app = nullptr;
    static bool s_keyWasDown[256] = {};

    //-------------------------------------------------------------------------
    // Internal helpers
    //-------------------------------------------------------------------------
    static const Binding* FindBinding(UINT vk)
    {
        for (size_t i = 0; i < kBindingCount; ++i)
        {
            if (s_bindings[i].vk == vk)
                return &s_bindings[i];
        }
        return nullptr;
    }

    static bool OnKeyDown(UINT vk, LPARAM lParam);
    static void OnKeyUp(UINT vk);

    //-------------------------------------------------------------------------
    // Public API
    //-------------------------------------------------------------------------
    void Initialize(Engine::App* app)
    {
        s_app = app;
        ResetKeyStates();
    }

    bool OnWin32Message(HWND, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_KEYDOWN:
            return OnKeyDown(static_cast<UINT>(wParam), lParam);

        case WM_KEYUP:
            OnKeyUp(static_cast<UINT>(wParam));
            return false;  // Don't claim exclusively

        case WM_KILLFOCUS:
#if defined(_DEBUG)
            OutputDebugStringA("[HotkeyRouter] WM_KILLFOCUS -> ResetKeyStates\n");
#endif
            ResetKeyStates();
            return false;

        default:
            return false;
        }
    }

    void ResetKeyStates()
    {
        for (int i = 0; i < 256; ++i)
            s_keyWasDown[i] = false;
    }

    //-------------------------------------------------------------------------
    // Key handling
    //-------------------------------------------------------------------------
    static bool OnKeyDown(UINT vk, LPARAM lParam)
    {
        // Find binding (return false if not registered -> falls through to DefWindowProc)
        const Binding* b = FindBinding(vk);
        if (!b)
            return false;

        const bool captured = Renderer::ImGuiLayer::WantsKeyboard();
        // Edge detection: bit30 of lParam indicates previous key state (1 = was down)
        // Double-check with our s_keyWasDown array as a safety net
        const bool isRepeat = ((lParam & 0x40000000) != 0) || ((vk < 256) && s_keyWasDown[vk]);

#if defined(_DEBUG)
        // Verbose logging for T and F7 keys (proof points)
        if (vk == 'T' || vk == VK_F7)
        {
            char buf[128];
            if (vk >= ' ' && vk <= 'Z')
            {
                sprintf_s(buf, "[HotkeyRouter] %c (%s) isRepeat=%d captured=%d -> %s\n",
                          static_cast<char>(vk), b->name,
                          isRepeat ? 1 : 0, captured ? 1 : 0,
                          (captured || isRepeat) ? "BLOCKED" : "FIRE");
            }
            else
            {
                sprintf_s(buf, "[HotkeyRouter] F%d (%s) isRepeat=%d captured=%d -> %s\n",
                          vk - VK_F1 + 1, b->name,
                          isRepeat ? 1 : 0, captured ? 1 : 0,
                          (captured || isRepeat) ? "BLOCKED" : "FIRE");
            }
            OutputDebugStringA(buf);
        }
#endif

        if (captured)
            return true;  // Blocked by ImGui, but consumed

        if (isRepeat)
            return true;  // Blocked by edge gate, but consumed

        // Mark key as down before invoking handler
        if (vk < 256)
            s_keyWasDown[vk] = true;

        b->handler();
        return true;
    }

    static void OnKeyUp(UINT vk)
    {
        if (vk < 256)
            s_keyWasDown[vk] = false;
    }

    //-------------------------------------------------------------------------
    // Handler implementations
    //-------------------------------------------------------------------------
    static void HandleToggleDrawMode()
    {
        Renderer::ToggleSystem::ToggleDrawMode();
        Renderer::ToggleSystem::RequestDiagnosticLog();
        OutputDebugStringA(Renderer::ToggleSystem::GetDrawMode() == Renderer::DrawMode::Naive
            ? "Naive\n" : "Instanced\n");
    }

    static void HandleToggleGrid()
    {
        Renderer::ToggleSystem::ToggleGrid();
        OutputDebugStringA(Renderer::ToggleSystem::IsGridEnabled() ? "Grid: ON\n" : "Grid: OFF\n");
    }

    static void HandleCycleColorMode()
    {
        Renderer::ToggleSystem::CycleColorMode();
        char buf[64];
        sprintf_s(buf, "ColorMode = %s\n", Renderer::ToggleSystem::GetColorModeName());
        OutputDebugStringA(buf);
    }

    static void HandleToggleUploadDiag()
    {
        Renderer::ToggleSystem::ToggleUploadDiag();
        OutputDebugStringA(Renderer::ToggleSystem::IsUploadDiagEnabled()
            ? "UploadDiag: ON\n" : "UploadDiag: OFF\n");
    }

    static void HandleToggleCameraMode()
    {
        Renderer::ToggleSystem::ToggleCameraMode();
        char buf[64];
        sprintf_s(buf, "CameraMode: %s\n", Renderer::ToggleSystem::GetCameraModeName());
        OutputDebugStringA(buf);
    }

    static void HandleToggleOpaquePSO()
    {
        Renderer::ToggleSystem::ToggleOpaquePSO();
        OutputDebugStringA(Renderer::ToggleSystem::IsOpaquePSOEnabled()
            ? "OpaquePSO: ON\n" : "OpaquePSO: OFF\n");
    }

    static void HandleSentinelInstance0()
    {
        bool current = Renderer::ToggleSystem::IsSentinelInstance0Enabled();
        Renderer::ToggleSystem::SetSentinelInstance0(!current);
        OutputDebugStringA(current ? "sentinel_Instance0: OFF\n" : "sentinel_Instance0: ON\n");
    }

    static void HandleStompLifetime()
    {
        bool current = Renderer::ToggleSystem::IsStompLifetimeEnabled();
        Renderer::ToggleSystem::SetStompLifetime(!current);
        OutputDebugStringA(current ? "stomp_Lifetime: OFF\n" : "stomp_Lifetime: ON\n");
    }

    static void HandleToggleControllerMode()
    {
        if (s_app)
            s_app->ToggleControllerMode();
    }

    static void HandleToggleStepUpGridTest()
    {
        if (s_app)
            s_app->ToggleStepUpGridTest();
    }

    static void HandleToggleHudVerbose()
    {
        Renderer::ToggleSystem::ToggleHudVerbose();
        OutputDebugStringA(Renderer::ToggleSystem::IsHudVerboseEnabled()
            ? "[HUD] Verbose: ON\n" : "[HUD] Verbose: OFF\n");
    }

    static void HandleToggleDebugSingleInstance()
    {
        Renderer::ToggleSystem::ToggleDebugSingleInstance();
        char buf[64];
        sprintf_s(buf, "DebugSingleInstance: %s (idx=%u)\n",
            Renderer::ToggleSystem::IsDebugSingleInstanceEnabled() ? "ON" : "OFF",
            Renderer::ToggleSystem::GetDebugInstanceIndex());
        OutputDebugStringA(buf);
    }

}  // namespace HotkeyRouter
