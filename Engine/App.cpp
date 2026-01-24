#include "App.h"

namespace Engine
{
    bool App::Initialize(HWND hwnd)
    {
        if (m_initialized)
            return false;

        m_hwnd = hwnd;

        // Initialize DX12 renderer
        if (!m_renderer.Initialize(hwnd))
        {
            return false;
        }

        m_initialized = true;
        return true;
    }

    void App::Tick()
    {
        if (!m_initialized)
            return;

        // Render frame
        m_renderer.Render();
    }

    void App::Shutdown()
    {
        if (!m_initialized)
            return;

        // Shutdown renderer first
        m_renderer.Shutdown();

        m_hwnd = nullptr;
        m_initialized = false;
    }
}
