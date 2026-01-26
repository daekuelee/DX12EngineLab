#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>
#include "UploadArena.h"

namespace Renderer
{
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

    private:
        void BuildHUDContent();

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
        bool m_initialized = false;

        // Upload arena metrics (Day2)
        UploadArenaMetrics m_uploadMetrics;
        bool m_hasUploadMetrics = false;
    };
}
