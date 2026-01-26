#pragma once

#include <Windows.h>
#include "../Renderer/DX12/Dx12Context.h"
#include "WorldState.h"

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

    private:
        HWND m_hwnd = nullptr;
        Renderer::Dx12Context m_renderer;
        bool m_initialized = false;

        // Day3: World state and fixed-step simulation
        WorldState m_worldState;
        float m_accumulator = 0.0f;
        bool m_prevJump = false;  // Previous frame's space key state for edge detection

        static constexpr float FIXED_DT = 1.0f / 60.0f;  // 60 Hz fixed step
    };
}
