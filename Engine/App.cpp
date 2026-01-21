#include "App.h"

namespace Engine
{
    bool App::Initialize(HWND hwnd)
    {
        if (m_initialized)
            return false;

        m_hwnd = hwnd;
        m_initialized = true;

        return true;
    }

    void App::Tick()
    {
        // Placeholder - will contain per-frame update and render logic
    }

    void App::Shutdown()
    {
        if (!m_initialized)
            return;

        m_hwnd = nullptr;
        m_initialized = false;
    }
}
