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

        // Diagnostic logging trigger
        static bool ShouldLogDiagnostics() { return s_logDiagnostics; }
        static void RequestDiagnosticLog() { s_logDiagnostics = true; }
        static void ClearDiagnosticLog() { s_logDiagnostics = false; }

        // Grid (cube) visibility toggle for debugging floor isolation
        static bool IsGridEnabled() { return s_gridEnabled; }
        static void SetGridEnabled(bool enabled) { s_gridEnabled = enabled; }
        static void ToggleGrid() { s_gridEnabled = !s_gridEnabled; }

        // Marker visibility toggle (corner markers for debug)
        static bool IsMarkersEnabled() { return s_markersEnabled; }
        static void SetMarkersEnabled(bool enabled) { s_markersEnabled = enabled; }
        static void ToggleMarkers() { s_markersEnabled = !s_markersEnabled; }

    private:
        static inline DrawMode s_drawMode = DrawMode::Instanced;
        static inline bool s_gridEnabled = true;
        static inline bool s_markersEnabled = false;  // OFF by default

        // Diagnostic flags (for S7 proof toggles)
        static inline bool s_sentinelInstance0 = false;
        static inline bool s_stompLifetime = false;

        // Diagnostic logging flag
        static inline bool s_logDiagnostics = false;
    };
}
