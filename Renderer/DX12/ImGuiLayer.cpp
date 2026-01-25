#include "ImGuiLayer.h"
#include "ToggleSystem.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include <cstdio>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Renderer
{
    // FPS calculation state
    static float s_fps = 0.0f;
    static float s_frameTimeMs = 0.0f;
    static LARGE_INTEGER s_lastFpsTime = {};
    static LARGE_INTEGER s_fpsFrequency = {};
    static uint32_t s_frameCount = 0;
    static bool s_fpsTimerInitialized = false;

    bool ImGuiLayer::Initialize(HWND hwnd, ID3D12Device* device,
                                 uint32_t numFramesInFlight, DXGI_FORMAT rtvFormat)
    {
        if (m_initialized)
            return false;

        // Create dedicated shader-visible SRV heap for ImGui (exactly 1 descriptor for font texture)
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask = 0;

        HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap));
        if (FAILED(hr))
        {
            OutputDebugStringA("[ImGui] FAILED to create SRV heap\n");
            return false;
        }

        // Setup ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup platform/renderer backends
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX12_Init(device, static_cast<int>(numFramesInFlight),
                            rtvFormat,
                            m_srvHeap.Get(),
                            m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
                            m_srvHeap->GetGPUDescriptorHandleForHeapStart());

        // Setup style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.Alpha = 0.85f;  // Semi-transparent background

        // Initialize FPS timer
        QueryPerformanceFrequency(&s_fpsFrequency);
        QueryPerformanceCounter(&s_lastFpsTime);
        s_fpsTimerInitialized = true;
        s_frameCount = 0;

        m_initialized = true;

        char logBuf[128];
        sprintf_s(logBuf, "[ImGui] Init OK: heapDescriptors=1 frameCount=%u\n", numFramesInFlight);
        OutputDebugStringA(logBuf);

        return true;
    }

    void ImGuiLayer::Shutdown()
    {
        if (!m_initialized)
            return;

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        m_srvHeap.Reset();
        m_initialized = false;

        OutputDebugStringA("[ImGui] Shutdown complete\n");
    }

    void ImGuiLayer::BeginFrame()
    {
        if (!m_initialized)
            return;

        // Update FPS counter
        s_frameCount++;
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        float elapsed = static_cast<float>(currentTime.QuadPart - s_lastFpsTime.QuadPart) /
                        static_cast<float>(s_fpsFrequency.QuadPart);

        if (elapsed >= 0.5f)  // Update FPS every 0.5 seconds
        {
            s_fps = static_cast<float>(s_frameCount) / elapsed;
            s_frameTimeMs = elapsed * 1000.0f / static_cast<float>(s_frameCount);
            s_frameCount = 0;
            s_lastFpsTime = currentTime;
        }

        // Start new ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::RenderHUD()
    {
        if (!m_initialized)
            return;

        BuildHUDContent();
        ImGui::Render();
    }

    void ImGuiLayer::RecordCommands(ID3D12GraphicsCommandList* cmdList)
    {
        if (!m_initialized)
            return;

        // Bind ImGui's dedicated descriptor heap
        ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
        cmdList->SetDescriptorHeaps(1, heaps);

        // Render ImGui draw data
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
    }

    LRESULT ImGuiLayer::WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
    }

    bool ImGuiLayer::WantsKeyboard()
    {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureKeyboard;
    }

    bool ImGuiLayer::WantsMouse()
    {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse;
    }

    void ImGuiLayer::BuildHUDContent()
    {
        // Position at top-left with some padding
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.7f);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("HUD", nullptr, windowFlags))
        {
            // FPS display
            ImGui::Text("FPS: %.1f (%.3f ms)", s_fps, s_frameTimeMs);
            ImGui::Separator();

            // Current modes
            const char* drawModeName = ToggleSystem::GetDrawModeName();
            const char* colorModeName = ToggleSystem::GetColorModeName();
            bool gridEnabled = ToggleSystem::IsGridEnabled();

            ImGui::Text("Draw Mode: %s [T]", drawModeName);
            ImGui::Text("Color Mode: %s [C]", colorModeName);
            ImGui::Text("Grid: %s [G]", gridEnabled ? "ON" : "OFF");
            ImGui::Separator();

            // Collapsible controls section
            if (ImGui::CollapsingHeader("Controls"))
            {
                ImGui::BulletText("T: Toggle Draw Mode");
                ImGui::BulletText("C: Cycle Color Mode");
                ImGui::BulletText("G: Toggle Grid");
                ImGui::BulletText("F1/F2: Diagnostics");
                ImGui::BulletText("WASD/Arrows: Move");
                ImGui::BulletText("Space/Ctrl: Up/Down");
                ImGui::BulletText("Q/E: Rotate");
            }
        }
        ImGui::End();
    }
}
