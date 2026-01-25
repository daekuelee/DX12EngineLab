#pragma once

#include <Windows.h>
#include <cstdio>
#include <cstdarg>

namespace Renderer
{
    // DiagnosticLogger: Centralized throttled diagnostic logging
    // All logging functions are throttled to once per second by default
    class DiagnosticLogger
    {
    public:
        // Throttle interval in milliseconds (1 second default)
        static constexpr DWORD DefaultThrottleMs = 1000;

        // Check if logging should occur for this tag (returns true once per interval)
        // Uses a simple tag-based system with a fixed number of slots
        static bool ShouldLog(const char* tag, DWORD throttleMs = DefaultThrottleMs)
        {
            DWORD now = GetTickCount();
            DWORD* lastTime = GetThrottleSlot(tag);

            if (now - *lastTime > throttleMs)
            {
                *lastTime = now;
                return true;
            }
            return false;
        }

        // Log a message with throttling (printf-style)
        // tag: identifies this logging point for throttling
        // format: printf format string
        template<typename... Args>
        static void LogThrottled(const char* tag, const char* format, Args... args)
        {
            if (ShouldLog(tag))
            {
                char buf[512];
                sprintf_s(buf, format, args...);
                OutputDebugStringA(buf);
            }
        }

        // Log unconditionally (no throttling)
        template<typename... Args>
        static void Log(const char* format, Args... args)
        {
            char buf[512];
            sprintf_s(buf, format, args...);
            OutputDebugStringA(buf);
        }

    private:
        // Simple hash-based throttle slot lookup
        // Uses a fixed array to avoid dynamic allocation
        static constexpr size_t NumSlots = 16;

        static DWORD* GetThrottleSlot(const char* tag)
        {
            static DWORD slots[NumSlots] = {};

            // Simple hash of tag string
            size_t hash = 0;
            for (const char* p = tag; *p; ++p)
                hash = hash * 31 + *p;

            return &slots[hash % NumSlots];
        }
    };

} // namespace Renderer
