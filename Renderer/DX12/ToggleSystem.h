#pragma once

#include <cstdint>

namespace Renderer
{
    // Draw modes
    enum class DrawMode : uint32_t
    {
        Instanced,  // 1 draw call with 10k instances
        Naive       // 10k draw calls with 1 instance each
    };

    // Global toggle system for runtime mode switching and diagnostics
    class ToggleSystem
    {
    public:
        // Current draw mode
        static DrawMode GetDrawMode() { return s_drawMode; }
        static void SetDrawMode(DrawMode mode) { s_drawMode = mode; }
        static void ToggleDrawMode()
        {
            s_drawMode = (s_drawMode == DrawMode::Instanced) ? DrawMode::Naive : DrawMode::Instanced;
        }

        // Get mode name for logging
        static const char* GetDrawModeName()
        {
            return (s_drawMode == DrawMode::Instanced) ? "instanced" : "naive";
        }

        // Diagnostic toggles (S7)
        static bool IsSentinelInstance0Enabled() { return s_sentinelInstance0; }
        static void SetSentinelInstance0(bool enabled) { s_sentinelInstance0 = enabled; }

        static bool IsStompLifetimeEnabled() { return s_stompLifetime; }
        static void SetStompLifetime(bool enabled) { s_stompLifetime = enabled; }

        static bool IsBreakRPIndexSwapEnabled() { return s_breakRPIndexSwap; }
        static void SetBreakRPIndexSwap(bool enabled) { s_breakRPIndexSwap = enabled; }

    private:
        static inline DrawMode s_drawMode = DrawMode::Instanced;

        // Diagnostic flags (for S7 proof toggles)
        static inline bool s_sentinelInstance0 = false;
        static inline bool s_stompLifetime = false;
        static inline bool s_breakRPIndexSwap = false;
    };
}
