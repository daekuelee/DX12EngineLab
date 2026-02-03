#pragma once
/******************************************************************************
 * [DT-SSOT] DELTA TIME SINGLE SOURCE OF TRUTH
 *
 * OWNER: Engine::FrameClock
 * INVARIANT: Update() called ONCE per frame at App::Tick() start
 * MEASUREMENT: frame-start to frame-start (now - lastTime)
 * CLAMP: dt capped at kMaxFrameDtSeconds (breakpoint/pause safety)
 * ACCESS: GetDeltaSeconds() is const, safe to call anywhere after Update()
 *
 * GUARANTEES:
 *   - All consumers use same dt value per frame
 *   - No one-frame lag between simulation and render
 *   - Renderer never measures dt (pure consumer)
 ******************************************************************************/

#include <cstdint>

namespace Engine
{
    class FrameClock
    {
    public:
        // Maximum dt to prevent spiral of death after breakpoint/pause
        static constexpr float kMaxFrameDtSeconds = 0.1f;

        // Initialize the high-precision timer (call once at startup)
        void Init();

        // Measure delta time from last Update() call (call once per frame)
        void Update();

        // Get last measured delta time in seconds (const, call after Update)
        float GetDeltaSeconds() const;

    private:
        uint64_t m_frequency = 0;
        uint64_t m_lastTime = 0;
        float m_deltaSeconds = 0.0f;
        bool m_initialized = false;
        bool m_hasUpdatedOnce = false;  // Debug guard for GetDeltaSeconds before Update
    };
}
