#pragma once

#include <Windows.h>

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
        bool m_initialized = false;
    };
}
