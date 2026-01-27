#include "ImGuiLayer.h"
#include "ToggleSystem.h"
#include "Dx12Context.h"  // For HUDSnapshot definition

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

    bool ImGuiLayer::Initialize(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue,
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

        char heapLogBuf[256];
        sprintf_s(heapLogBuf, "[ImGui] Heap created: type=CBV_SRV_UAV flags=SHADER_VISIBLE numDesc=1\n");
        OutputDebugStringA(heapLogBuf);

        // Setup ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup platform/renderer backends
        ImGui_ImplWin32_Init(hwnd);

        // Guard: commandQueue must not be null
        if (!commandQueue)
        {
            OutputDebugStringA("[ImGui] FAIL: commandQueue=null\n");
#if defined(_DEBUG)
            __debugbreak();
#endif
            return false;
        }

        // Use InitInfo struct (required for v1.91+)
        ImGui_ImplDX12_InitInfo init_info = {};
        init_info.Device = device;
        init_info.CommandQueue = commandQueue;  // FIX: was missing
        init_info.NumFramesInFlight = static_cast<int>(numFramesInFlight);
        init_info.RTVFormat = rtvFormat;
        init_info.SrvDescriptorHeap = m_srvHeap.Get();
        // Use legacy single-descriptor API (we have exactly 1 descriptor in our heap)
        init_info.LegacySingleSrvCpuDescriptor = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        init_info.LegacySingleSrvGpuDescriptor = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        ImGui_ImplDX12_Init(&init_info);

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
        sprintf_s(logBuf, "[ImGui] Init OK: heapDescriptors=1 frameCount=%u cmdQueue=%p\n",
                  numFramesInFlight, commandQueue);
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

    void ImGuiLayer::SetUploadArenaMetrics(const UploadArenaMetrics& metrics)
    {
        m_uploadMetrics = metrics;  // Struct copy, no heap alloc
        m_hasUploadMetrics = true;
    }

    void ImGuiLayer::SetHUDSnapshot(const HUDSnapshot& snap)
    {
        m_worldState.mapName = snap.mapName;
        m_worldState.posX = snap.posX;
        m_worldState.posY = snap.posY;
        m_worldState.posZ = snap.posZ;
        m_worldState.velX = snap.velX;
        m_worldState.velY = snap.velY;
        m_worldState.velZ = snap.velZ;
        m_worldState.speed = snap.speed;
        m_worldState.onGround = snap.onGround;
        m_worldState.sprintAlpha = snap.sprintAlpha;
        m_worldState.yawDeg = snap.yawDeg;
        m_worldState.pitchDeg = snap.pitchDeg;
        m_worldState.fovDeg = snap.fovDeg;
        m_worldState.jumpQueued = snap.jumpQueued;
        // Part 1: Respawn tracking
        m_worldState.respawnCount = snap.respawnCount;
        m_worldState.lastRespawnReason = snap.lastRespawnReason;
        // Part 2: Collision stats
        m_worldState.candidatesChecked = snap.candidatesChecked;
        m_worldState.penetrationsResolved = snap.penetrationsResolved;
        m_worldState.lastHitCubeId = snap.lastHitCubeId;
        m_worldState.lastAxisResolved = snap.lastAxisResolved;
        // Day3.4: Iteration diagnostics
        m_worldState.iterationsUsed = snap.iterationsUsed;
        m_worldState.contacts = snap.contacts;
        m_worldState.maxPenetrationAbs = snap.maxPenetrationAbs;
        m_worldState.hitMaxIter = snap.hitMaxIter;
        // Day3.5: Support diagnostics
        m_worldState.supportSource = snap.supportSource;
        m_worldState.supportY = snap.supportY;
        m_worldState.supportCubeId = snap.supportCubeId;
        m_worldState.snappedThisTick = snap.snappedThisTick;
        m_worldState.supportGap = snap.supportGap;
        // Floor diagnostics
        m_worldState.inFloorBounds = snap.inFloorBounds;
        m_worldState.didFloorClamp = snap.didFloorClamp;
        m_worldState.floorMinX = snap.floorMinX;
        m_worldState.floorMaxX = snap.floorMaxX;
        m_worldState.floorMinZ = snap.floorMinZ;
        m_worldState.floorMaxZ = snap.floorMaxZ;
        m_worldState.floorY = snap.floorY;
        // Day3.7: Camera basis proof
        m_worldState.camFwdX = snap.camFwdX;
        m_worldState.camFwdZ = snap.camFwdZ;
        m_worldState.camRightX = snap.camRightX;
        m_worldState.camRightZ = snap.camRightZ;
        m_worldState.camDot = snap.camDot;
        // Day3.7: Collision extent proof
        m_worldState.pawnExtentX = snap.pawnExtentX;
        m_worldState.pawnExtentZ = snap.pawnExtentZ;
        m_hasWorldState = true;
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
            ImGui::Text("CamMode: %s [V]", ToggleSystem::GetCameraModeName());

            // World State section (Day3) - only show in ThirdPerson mode
            if (ToggleSystem::GetCameraMode() == CameraMode::ThirdPerson && m_hasWorldState)
            {
                ImGui::Separator();
                ImGui::Text("-- World State --");
                if (m_worldState.mapName)
                    ImGui::Text("Map: %s", m_worldState.mapName);
                ImGui::Text("Pos: %.1f, %.1f, %.1f", m_worldState.posX, m_worldState.posY, m_worldState.posZ);
                ImGui::Text("Speed: %.1f", m_worldState.speed);
                ImGui::Text("OnGround: %s", m_worldState.onGround ? "YES" : "NO");
                ImGui::Text("Sprint: %.0f%%", m_worldState.sprintAlpha * 100.0f);
                ImGui::Text("Yaw: %.1f deg", m_worldState.yawDeg);
                ImGui::Text("Pitch: %.1f deg", m_worldState.pitchDeg);
                ImGui::Text("FOV: %.1f deg", m_worldState.fovDeg);
                if (m_worldState.jumpQueued)
                    ImGui::TextColored(ImVec4(0,1,0,1), "JUMP!");

                // Part 1: Respawn tracking
                if (m_worldState.respawnCount > 0)
                {
                    ImGui::Separator();
                    ImGui::Text("-- Respawn --");
                    ImGui::Text("Count: %u", m_worldState.respawnCount);
                    if (m_worldState.lastRespawnReason)
                        ImGui::Text("Reason: %s", m_worldState.lastRespawnReason);
                }

                // Part 2: Collision stats
                ImGui::Separator();
                ImGui::Text("-- Collision --");
                ImGui::Text("Candidates: %u  Contacts(sum): %u", m_worldState.candidatesChecked, m_worldState.contacts);
                ImGui::Text("Penetrations: %u", m_worldState.penetrationsResolved);
                if (m_worldState.lastHitCubeId >= 0)
                {
                    const char* axisName = (m_worldState.lastAxisResolved == 0) ? "X" :
                                           (m_worldState.lastAxisResolved == 1) ? "Y" : "Z";
                    ImGui::Text("LastHit: cube=%d axis=%s", m_worldState.lastHitCubeId, axisName);
                }

                // Day3.4: Iteration solver diagnostics
                ImGui::Separator();
                ImGui::Text("-- Solver --");
                if (m_worldState.hitMaxIter)
                    ImGui::TextColored(ImVec4(1,0.3f,0,1), "Solver: HIT_MAX_ITER (%u/8)", m_worldState.iterationsUsed);
                else
                    ImGui::Text("SolverIter: %u/8", m_worldState.iterationsUsed);
                if (m_worldState.maxPenetrationAbs > 0.001f)
                    ImGui::TextColored(ImVec4(1,0.5f,0,1), "MaxPenAbs: %.4f", m_worldState.maxPenetrationAbs);

                // Day3.5: Support diagnostics
                ImGui::Separator();
                ImGui::Text("-- Support --");
                const char* supportName = (m_worldState.supportSource == 0) ? "FLOOR" :
                                          (m_worldState.supportSource == 1) ? "CUBE" : "NONE";
                if (m_worldState.supportSource == 1)
                    ImGui::Text("Support: %s(%d) Y=%.3f", supportName, m_worldState.supportCubeId, m_worldState.supportY);
                else
                    ImGui::Text("Support: %s Y=%.3f", supportName, m_worldState.supportY);
                ImGui::Text("onGround: %s", m_worldState.onGround ? "YES" : "NO");
                ImGui::Text("Snapped: %s  Gap: %.4f", m_worldState.snappedThisTick ? "YES" : "NO", m_worldState.supportGap);
                ImGui::Text("contacts: %u", m_worldState.contacts);

                // Floor bounds reference
                ImGui::Separator();
                ImGui::Text("-- Floor Bounds --");
                ImGui::Text("posX: %.2f  posZ: %.2f", m_worldState.posX, m_worldState.posZ);
                ImGui::Text("posY (feet): %.3f", m_worldState.posY);
                ImGui::Text("velY: %.2f", m_worldState.velY);
                ImGui::Text("inBounds: %s", m_worldState.inFloorBounds ? "YES" : "NO");
                ImGui::Text("Bounds: X[%.0f,%.0f] Z[%.0f,%.0f]",
                    m_worldState.floorMinX, m_worldState.floorMaxX,
                    m_worldState.floorMinZ, m_worldState.floorMaxZ);

                // Day3.7: Camera basis proof (Bug A)
                ImGui::Separator();
                ImGui::Text("-- Camera Basis --");
                ImGui::Text("Fwd: (%.2f, %.2f)", m_worldState.camFwdX, m_worldState.camFwdZ);
                ImGui::Text("Right: (%.2f, %.2f)", m_worldState.camRightX, m_worldState.camRightZ);
                ImGui::Text("Dot: %.4f", m_worldState.camDot);

                // Day3.7: Collision extent proof (Bug C)
                ImGui::Separator();
                ImGui::Text("-- Collision Extent --");
                ImGui::Text("Extent: X=%.2f Z=%.2f", m_worldState.pawnExtentX, m_worldState.pawnExtentZ);

                ImGui::Separator();
                ImGui::Text("-- Render Passes --");
                // Character pass is always active in ThirdPerson mode
                bool gridActive = ToggleSystem::IsGridEnabled();
                bool charActive = (ToggleSystem::GetCameraMode() == CameraMode::ThirdPerson);
                ImGui::Text("Passes: Grid=%s Char=%s",
                    gridActive ? "ON" : "OFF",
                    charActive ? "ON" : "OFF");
                if (charActive)
                    ImGui::Text("Character Parts: 6");
            }

            // Upload diagnostics section (Day2) - only show when diag mode enabled AND metrics valid
            if (ToggleSystem::IsUploadDiagEnabled() && m_hasUploadMetrics)
            {
                ImGui::Separator();
                ImGui::Text("-- Upload Arena --");
                ImGui::Text("Alloc Calls: %u", m_uploadMetrics.allocCalls);
                ImGui::Text("Alloc Bytes: %llu KB", m_uploadMetrics.allocBytes / 1024);
                ImGui::Text("Peak Offset: %llu / %llu KB (%.1f%%)",
                    m_uploadMetrics.peakOffset / 1024,
                    m_uploadMetrics.capacity / 1024,
                    m_uploadMetrics.capacity > 0
                        ? (100.0f * m_uploadMetrics.peakOffset / m_uploadMetrics.capacity)
                        : 0.0f);

                // Warn if >80% capacity
                if (m_uploadMetrics.capacity > 0 &&
                    m_uploadMetrics.peakOffset > m_uploadMetrics.capacity * 8 / 10)
                {
                    ImGui::TextColored(ImVec4(1,1,0,1), "Warning: >80%% capacity");
                }

                // Last allocation detail
                if (m_uploadMetrics.lastAllocTag)
                {
                    ImGui::Text("Last: %s (%llu B @ %llu)",
                        m_uploadMetrics.lastAllocTag,
                        m_uploadMetrics.lastAllocSize,
                        m_uploadMetrics.lastAllocOffset);
                }
            }

            ImGui::Separator();

            // Collapsible controls section
            if (ImGui::CollapsingHeader("Controls"))
            {
                ImGui::BulletText("V: Toggle Camera Mode");
                ImGui::BulletText("T: Toggle Draw Mode");
                ImGui::BulletText("C: Cycle Color Mode");
                ImGui::BulletText("G: Toggle Grid");
                ImGui::BulletText("U: Upload Diagnostics");
                if (ToggleSystem::GetCameraMode() == CameraMode::ThirdPerson)
                {
                    ImGui::BulletText("WASD: Move (cam-relative)");
                    ImGui::BulletText("Mouse: Look around");
                    ImGui::BulletText("Q/E: Yaw, R/F: Pitch");
                    ImGui::BulletText("Shift: Sprint");
                    ImGui::BulletText("Space: Jump");
                }
                else
                {
                    ImGui::BulletText("WASD/Arrows: Move");
                    ImGui::BulletText("Space/Ctrl: Up/Down");
                    ImGui::BulletText("Q/E: Rotate");
                }
            }
        }
        ImGui::End();
    }
}
