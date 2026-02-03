/******************************************************************************
 * FrameClock.cpp — dt SSOT implementation
 *
 * Platform types (LARGE_INTEGER, QPC) isolated here; header is clean.
 ******************************************************************************/

#include "FrameClock.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#ifdef _DEBUG
#include <cassert>
#endif

namespace Engine
{
    void FrameClock::Init()
    {
        LARGE_INTEGER freq, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&now);
        m_frequency = static_cast<uint64_t>(freq.QuadPart);
        m_lastTime = static_cast<uint64_t>(now.QuadPart);
        m_deltaSeconds = 0.0f;
        m_initialized = true;
        m_hasUpdatedOnce = false;
    }

    void FrameClock::Update()
    {
        if (!m_initialized || m_frequency == 0)
        {
            m_deltaSeconds = 0.0f;
            return;
        }

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        uint64_t currentTime = static_cast<uint64_t>(now.QuadPart);

        float dt = static_cast<float>(currentTime - m_lastTime)
                 / static_cast<float>(m_frequency);

        // Clamp to prevent spiral of death after breakpoint/pause
        if (dt > kMaxFrameDtSeconds)
            dt = kMaxFrameDtSeconds;

        m_lastTime = currentTime;
        m_deltaSeconds = dt;
        m_hasUpdatedOnce = true;
    }

    float FrameClock::GetDeltaSeconds() const
    {
#ifdef _DEBUG
        // [PROOF-DT-ORDER] Fail-fast: GetDeltaSeconds called before first Update
        assert(m_hasUpdatedOnce && "FrameClock::GetDeltaSeconds called before Update()");
#endif
        return m_deltaSeconds;
    }
}
