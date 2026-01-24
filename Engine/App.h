#pragma once

#include <Windows.h>
#include "../Renderer/DX12/Dx12Context.h"

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
    };
}
