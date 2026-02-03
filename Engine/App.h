#pragma once

#include <Windows.h>
#include "../Renderer/DX12/Dx12Context.h"
#include "WorldState.h"
#include "FrameClock.h"

namespace Engine
{
    class App
    {
    public:
        App() = default;
        ~App() = default;

        App(const App&) = delete;
        App& operator=(const App&) = delete;

        bool Initialize(HWND hwnd);
        void Tick();
        void Shutdown();

        // Mouse input handler (called from WndProc)
        void OnMouseMove(int x, int y);

        // Day3.11: Controller mode toggle (forwarded from WndProc)
        void ToggleControllerMode();

        // Day3.12+: Step-up grid test toggle (forwarded from WndProc)
        void ToggleStepUpGridTest();

    private:
        HWND m_hwnd = nullptr;
        Renderer::Dx12Context m_renderer;
        bool m_initialized = false;

        // Day3: World state and fixed-step simulation
        WorldState m_worldState;
        float m_accumulator = 0.0f;
        bool m_prevJump = false;  // Previous frame's space key state for edge detection

        // Mouse look state
        int m_lastMouseX = 0;
        int m_lastMouseY = 0;
        bool m_mouseInitialized = false;
        float m_pendingMouseDeltaX = 0.0f;
        float m_pendingMouseDeltaY = 0.0f;

        // [DT-SSOT] Frame clock owns delta-time measurement
        FrameClock m_frameClock;

        static constexpr float FIXED_DT = 1.0f / 60.0f;  // 60 Hz fixed step
    };
}
