#pragma once

#include <Windows.h>
#include "../Renderer/DX12/Dx12Context.h"
#include "WorldState.h"
#include "FrameClock.h"

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

        // Day3.12+: Step-up grid test toggle (forwarded from WndProc)
        void ToggleStepUpGridTest();

    private:
        HWND m_hwnd = nullptr;
        Renderer::Dx12Context m_renderer;
        bool m_initialized = false;

        // Day3: World state and fixed-step simulation
        WorldState m_worldState;
        float m_accumulator = 0.0f;

        // [DT-SSOT] Frame clock owns delta-time measurement
        FrameClock m_frameClock;

        static constexpr float FIXED_DT = 1.0f / 60.0f;  // 60 Hz fixed step

        //---------------------------------------------------------------------
        // Camera/Presentation Helpers (PR2.3)
        //
        // CONTRACT: Behavior-preserving extraction from Tick().
        // INVARIANT: Call order must be preserved:
        //   1. ApplyPresentationPreviewIfNeeded (sets offset before camera rig)
        //   2. UpdateRenderCamera (computes camera rig state)
        //   3. SubmitRenderCamera (reads computed camera state)
        //---------------------------------------------------------------------
        void ApplyPresentationPreviewIfNeeded(uint32_t stepCount, bool imguiBlocksGameplay);
        void UpdateRenderCamera(float frameDt);
        void SubmitRenderCamera();
    };
}
