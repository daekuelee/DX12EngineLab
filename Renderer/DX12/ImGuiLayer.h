#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>
#include "UploadArena.h"

namespace Renderer
{
    // Forward declaration
    struct HUDSnapshot;
    class ImGuiLayer
    {
    public:
        bool Initialize(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                        uint32_t numFramesInFlight, DXGI_FORMAT rtvFormat);
        void Shutdown();
        void BeginFrame();
        void RenderHUD();  // Build UI widgets + ImGui::Render()
        void RecordCommands(ID3D12GraphicsCommandList* cmdList);

        // Forward to ImGui backend (always call, ignore return value)
        static LRESULT WndProcHandler(HWND hwnd, UINT msg,
                                      WPARAM wParam, LPARAM lParam);

        // Input capture query (check AFTER ImGui::NewFrame has run)
        static bool WantsKeyboard();
        static bool WantsMouse();

        // Upload arena metrics for HUD display (Day2)
        void SetUploadArenaMetrics(const UploadArenaMetrics& metrics);

        // World state snapshot for HUD display (Day3)
        void SetHUDSnapshot(const HUDSnapshot& snap);

    private:
        void BuildHUDContent();

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
        bool m_initialized = false;

        // Upload arena metrics (Day2)
        UploadArenaMetrics m_uploadMetrics;
        bool m_hasUploadMetrics = false;

        // World state snapshot (Day3) - stored as individual fields to avoid header dependency
        struct WorldStateFields {
            const char* mapName = nullptr;
            float posX = 0, posY = 0, posZ = 0;
            float velX = 0, velY = 0, velZ = 0;
            float speed = 0;
            bool onGround = true;
            float sprintAlpha = 0;
            float yawDeg = 0, pitchDeg = 0;
            float fovDeg = 45;
            bool jumpQueued = false;
            // Character pass proof fields
            uint32_t characterPartCount = 0;
            bool gridPassActive = true;
            bool characterPassActive = false;
        } m_worldState;
        bool m_hasWorldState = false;
    };
}
