#pragma once

#include <cstdint>

namespace Renderer
{
    enum class KccTraceStatus : uint8_t
    {
        Off = 0,
        Recording,
        Triggered,
        Frozen
    };

    enum class KccTraceCulprit : uint8_t
    {
        None = 0,
        Recover,
        StepUp,
        StepMove,
        StepDown,
        PostRecover,
        Unknown
    };

    struct KccTraceUiState
    {
        bool enabled = false;
        bool autoTrigger = true;
        float upwardPopEps = 0.02f;
    };

    struct KccTraceUiActions
    {
        bool freezeRequested = false;
        bool clearRequested = false;
        bool saveRequested = false;
    };

    struct KccTraceHudState
    {
        KccTraceStatus status = KccTraceStatus::Off;
        KccTraceCulprit lastCulprit = KccTraceCulprit::None;
        uint32_t storedTicks = 0;
        uint32_t triggerTick = 0;
        float yBegin = 0.0f;
        float yFinal = 0.0f;
        bool lastSaveOk = false;
        const char* lastSavePath = nullptr;
    };
}
