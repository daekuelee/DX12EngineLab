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
            // Part 1: Respawn tracking
            uint32_t respawnCount = 0;
            const char* lastRespawnReason = nullptr;
            // Part 2: Collision stats
            uint32_t candidatesChecked = 0;
            uint32_t penetrationsResolved = 0;
            int32_t lastHitCubeId = -1;
            uint8_t lastAxisResolved = 1;  // 0=X, 1=Y, 2=Z
            // Day3.4: Collision iteration diagnostics
            uint8_t iterationsUsed = 0;
            uint32_t contacts = 0;
            float maxPenetrationAbs = 0.0f;
            bool hitMaxIter = false;
            // Day3.5: Support diagnostics
            uint8_t supportSource = 2;  // 0=FLOOR, 1=CUBE, 2=NONE
            float supportY = -1000.0f;
            int32_t supportCubeId = -1;
            bool snappedThisTick = false;
            float supportGap = 0.0f;
            // Floor diagnostics (Day3 debug)
            bool inFloorBounds = false;
            bool didFloorClamp = false;
            float floorMinX = 0, floorMaxX = 0;
            float floorMinZ = 0, floorMaxZ = 0;
            float floorY = 0;
            // Day3.7: Camera basis proof (Bug A)
            float camFwdX = 0, camFwdZ = 0;
            float camRightX = 0, camRightZ = 0;
            float camDot = 0;
            // Day3.7: Collision extent proof (Bug C)
            float pawnExtentX = 0, pawnExtentZ = 0;
            // Day3.8: MTV debug fields
            float mtvPenX = 0, mtvPenZ = 0;
            uint8_t mtvAxis = 0;
            float mtvMagnitude = 0;
            float mtvCenterDiffX = 0, mtvCenterDiffZ = 0;
            // Day3.9: Regression debug
            bool xzStillOverlapping = false;
            bool yStepUpSkipped = false;
            float yDeltaApplied = 0;
            // Day3.11: Controller mode (0=AABB, 1=Capsule)
            uint8_t controllerMode = 0;
            // Day3.11: Capsule geometry
            float capsuleRadius = 0.0f;
            float capsuleHalfHeight = 0.0f;
            float capsuleP0y = 0.0f;
            float capsuleP1y = 0.0f;
            // Day3.11 Phase 2: Capsule depenetration diagnostics
            bool depenApplied = false;
            float depenTotalMag = 0.0f;
            bool depenClampTriggered = false;
            float depenMaxSingleMag = 0.0f;
            uint32_t depenOverlapCount = 0;
            uint32_t depenIterations = 0;
        } m_worldState;
        bool m_hasWorldState = false;
    };
}
