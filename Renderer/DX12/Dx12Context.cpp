#include "Dx12Context.h"
#include "Dx12Debug.h"
#include "ShaderLibrary.h"
#include "ToggleSystem.h"
#include "RenderContext.h"
#include "ClearPass.h"
#include "GeometryPass.h"
#include "ImGuiPass.h"
#include "BarrierScope.h"
#include "DiagnosticLogger.h"
#include "ResourceStateTracker.h"
#include "PassOrchestrator.h"
#include "CharacterPass.h"
#include "../../Engine/WorldState.h"  // Day3.12 Phase 4B+: Fixture transform overrides
#include <cstdio>
#include <DirectXMath.h>

using namespace DirectX;

using Microsoft::WRL::ComPtr;

// Handedness switch: set to 0 for LH if RH produces inverted/invisible scene
#define USE_RIGHT_HANDED 1

namespace Renderer
{
    //-------------------------------------------------------------------------
    // Camera Helpers
    //-------------------------------------------------------------------------

    void Dx12Context::UpdateCamera(float dt)
    {
        // Movement input
        float moveX = 0.0f, moveY = 0.0f, moveZ = 0.0f;
        if (GetAsyncKeyState('W') & 0x8000 || GetAsyncKeyState(VK_UP) & 0x8000)    moveZ += 1.0f;
        if (GetAsyncKeyState('S') & 0x8000 || GetAsyncKeyState(VK_DOWN) & 0x8000)  moveZ -= 1.0f;
        if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000)  moveX -= 1.0f;
        if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000) moveX += 1.0f;
        if (GetAsyncKeyState(VK_SPACE) & 0x8000)  moveY += 1.0f;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) moveY -= 1.0f;

        // Look input
        float yawDelta = 0.0f;
        if (GetAsyncKeyState('Q') & 0x8000) yawDelta -= 1.0f;
        if (GetAsyncKeyState('E') & 0x8000) yawDelta += 1.0f;

        // Apply yaw
        m_camera.yaw += yawDelta * m_camera.lookSpeed * dt;

        // Build movement vectors in camera space
        float cosY = cosf(m_camera.yaw);
        float sinY = sinf(m_camera.yaw);
        XMFLOAT3 forward = { sinY, 0, cosY };  // Horizontal forward
        XMFLOAT3 right = { cosY, 0, -sinY };   // Horizontal right

        float speed = m_camera.moveSpeed * dt;
        m_camera.position[0] += (forward.x * moveZ + right.x * moveX) * speed;
        m_camera.position[2] += (forward.z * moveZ + right.z * moveX) * speed;
        m_camera.position[1] += moveY * speed;
    }

    static XMMATRIX BuildFreeCameraViewProj(const Dx12Context::FreeCamera& cam, float aspect)
    {
        // Forward vector from yaw/pitch (RH: -Z is forward at yaw=0)
        float cosP = cosf(cam.pitch);
        XMFLOAT3 forward = {
            sinf(cam.yaw) * cosP,
            sinf(cam.pitch),
            cosf(cam.yaw) * cosP
        };

        XMFLOAT3 pos = { cam.position[0], cam.position[1], cam.position[2] };
        XMVECTOR eye = XMLoadFloat3(&pos);
        XMVECTOR fwd = XMLoadFloat3(&forward);
        XMVECTOR target = XMVectorAdd(eye, fwd);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);

#if USE_RIGHT_HANDED
        XMMATRIX view = XMMatrixLookAtRH(eye, target, up);
        XMMATRIX proj = XMMatrixPerspectiveFovRH(cam.fovY, aspect, cam.nearZ, cam.farZ);
#else
        XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(cam.fovY, aspect, cam.nearZ, cam.farZ);
#endif

        return XMMatrixMultiply(view, proj);
    }

    static void ThrowIfFailed(HRESULT hr, const char* msg = nullptr)
    {
        if (FAILED(hr))
        {
            if (msg)
                OutputDebugStringA(msg);
            __debugbreak();
        }
    }

    //-------------------------------------------------------------------------
    // Initialize() Helpers - Device and SwapChain
    //-------------------------------------------------------------------------

    void Dx12Context::InitDevice()
    {
        // Enable debug layer in debug builds
#if defined(_DEBUG)
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();
                OutputDebugStringA("DX12 Debug layer enabled\n");
            }
        }
#endif

        // Create DXGI factory
        UINT factoryFlags = 0;
#if defined(_DEBUG)
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
        ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory)),
            "Failed to create DXGI factory\n");

        // Find best adapter and create device
        {
            ComPtr<IDXGIAdapter1> bestAdapter;
            SIZE_T bestVram = 0;

            for (UINT i = 0; ; ++i)
            {
                ComPtr<IDXGIAdapter1> adapter;
                if (m_factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND)
                    break;

                DXGI_ADAPTER_DESC1 desc = {};
                adapter->GetDesc1(&desc);

                // Skip software adapters
                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;

                // Pick adapter with most VRAM
                if (desc.DedicatedVideoMemory > bestVram)
                {
                    bestVram = desc.DedicatedVideoMemory;
                    bestAdapter = adapter;
                }
            }

            m_adapter = bestAdapter;

            ThrowIfFailed(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)),
                "Failed to create D3D12 device\n");
        }

        // Create command queue
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

            ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)),
                "Failed to create command queue\n");
        }
    }

    void Dx12Context::InitSwapChain()
    {
        DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
        swapDesc.Width = m_width;
        swapDesc.Height = m_height;
        swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.BufferCount = FrameCount;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapDesc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swapChain1;
        ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            m_hwnd,
            &swapDesc,
            nullptr,
            nullptr,
            &swapChain1),
            "Failed to create swap chain\n");

        ThrowIfFailed(swapChain1.As(&m_swapChain),
            "Failed to get IDXGISwapChain3\n");

        // Store backbuffer format for ImGui initialization
        m_backBufferFormat = swapDesc.Format;
    }

    void Dx12Context::InitRenderTargets()
    {
        // Create RTV descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)),
            "Failed to create RTV heap\n");

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Create RTVs for back buffers
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < FrameCount; ++i)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])),
                "Failed to get swap chain buffer\n");

            m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtvHandle);
            rtvHandle.ptr += m_rtvDescriptorSize;

            // Register backbuffers with state tracker (initial state is PRESENT after swap chain creation)
            m_stateTracker.AssumeState(m_backBuffers[i].Get(), D3D12_RESOURCE_STATE_PRESENT);
        }
    }

    void Dx12Context::InitDepthBuffer()
    {
        // Create DSV descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)),
            "Failed to create DSV heap\n");

        // Create depth buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = m_width;
        depthDesc.Height = m_height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_depthBuffer)),
            "Failed to create depth buffer\n");

        // Create DSV
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;

        m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc,
            m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    //-------------------------------------------------------------------------
    // Initialize() Helpers - Subsystems
    //-------------------------------------------------------------------------

    bool Dx12Context::InitFrameResources()
    {
        // Initialize CBV/SRV/UAV descriptor ring allocator (shader-visible)
        // Capacity: 1024 total, 4 reserved:
        //   - Slots 0-2: per-frame transforms SRVs (FrameContextRing)
        //   - Slot 3: character transforms SRV (CharacterRenderer, persistent)
        if (!m_descRing.Initialize(m_device.Get(), 1024, FrameCount + 1))
        {
            OutputDebugStringA("Failed to initialize descriptor ring allocator\n");
            return false;
        }

        // Initialize resource registry (needed by frame ring for transforms)
        if (!m_resourceRegistry.Initialize(m_device.Get()))
        {
            OutputDebugStringA("Failed to initialize resource registry\n");
            return false;
        }

        // Initialize frame context ring (per-frame allocators + fence + buffers + SRVs)
        if (!m_frameRing.Initialize(m_device.Get(), &m_descRing, &m_resourceRegistry))
        {
            OutputDebugStringA("Failed to initialize frame context ring\n");
            return false;
        }

        // Register transforms buffers with state tracker (initial state is COPY_DEST)
        for (uint32_t i = 0; i < FrameCount; ++i)
        {
            FrameContext& ctx = m_frameRing.BeginFrame(i);
            ID3D12Resource* transformsResource = m_resourceRegistry.Get(ctx.transformsHandle);
            char debugName[32];
            sprintf_s(debugName, "Transforms[%u]", i);
            m_stateTracker.Register(transformsResource, D3D12_RESOURCE_STATE_COPY_DEST, debugName);
        }

        // Create command list (closed initially, will be reset per-frame)
        FrameContext& firstFrame = m_frameRing.BeginFrame(0);

        ThrowIfFailed(m_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            firstFrame.cmdAllocator.Get(),
            nullptr, // No initial PSO
            IID_PPV_ARGS(&m_commandList)),
            "Failed to create command list\n");

        // Close it - will be reset at start of each frame
        m_commandList->Close();

        return true;
    }

    bool Dx12Context::InitShaders()
    {
        // Initialize shader library (root sig + PSO)
        if (!m_shaderLibrary.Initialize(m_device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM))
        {
            OutputDebugStringA("Failed to initialize shader library\n");
            return false;
        }

        // Note: Resource registry is initialized in InitFrameResources()

        return true;
    }

    bool Dx12Context::InitScene()
    {
        // Initialize geometry factory first
        if (!m_geometryFactory.Initialize(m_device.Get(), m_commandQueue.Get()))
        {
            OutputDebugStringA("Failed to initialize geometry factory\n");
            return false;
        }

        // Initialize scene using factory
        if (!m_scene.Initialize(&m_geometryFactory))
        {
            OutputDebugStringA("Failed to initialize render scene\n");
            return false;
        }

        // Initialize character renderer (Day3)
        // Pass descRing for persistent SRV allocation (reserved slot 3)
        if (!m_characterRenderer.Initialize(m_device.Get(), &m_stateTracker, &m_descRing))
        {
            OutputDebugStringA("Failed to initialize character renderer\n");
            return false;
        }

        return true;
    }

    bool Dx12Context::InitImGui()
    {
        if (!m_imguiLayer.Initialize(m_hwnd, m_device.Get(), m_commandQueue.Get(),
                                      FrameCount, m_backBufferFormat))
        {
            OutputDebugStringA("[ImGui] FAILED to initialize\n");
            return false;
        }
        return true;
    }

    //-------------------------------------------------------------------------
    // Initialize
    //-------------------------------------------------------------------------

    bool Dx12Context::Initialize(HWND hwnd, Engine::WorldState* worldState)
    {
        if (m_initialized)
            return false;

        m_hwnd = hwnd;
        m_worldState = worldState;  // Day3.12 Phase 4B+: Store for fixture transform overrides

        // Get window dimensions
        RECT rect = {};
        GetClientRect(hwnd, &rect);
        m_width = static_cast<uint32_t>(rect.right - rect.left);
        m_height = static_cast<uint32_t>(rect.bottom - rect.top);

        if (m_width == 0 || m_height == 0)
        {
            m_width = 1280;
            m_height = 720;
        }

        // Initialize device and core objects
        InitDevice();
        InitSwapChain();
        InitRenderTargets();
        InitDepthBuffer();

        // Initialize subsystems
        if (!InitFrameResources()) return false;
        if (!InitShaders()) return false;
        if (!InitScene()) return false;
        if (!InitImGui()) return false;

        // Setup viewport and scissor
        m_viewport = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
        m_scissorRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };

        m_frameId = 0;
        m_initialized = true;

        // [DT-SSOT] Timer removed - FrameClock owns dt

        OutputDebugStringA("Dx12Context initialized successfully\n");
        return true;
    }

    // CBV requires 256-byte alignment
    static constexpr uint64_t CBV_ALIGNMENT = 256;
    static constexpr uint64_t CB_SIZE = (sizeof(float) * 16 + CBV_ALIGNMENT - 1) & ~(CBV_ALIGNMENT - 1);

    // Transforms: (10k + extras) float4x4 = (10k + 32) * 64 bytes
    static constexpr uint64_t TRANSFORMS_SIZE = (InstanceCount + MaxExtraInstances) * sizeof(float) * 16;

    //-------------------------------------------------------------------------
    // Phase Helpers
    //-------------------------------------------------------------------------

    void Dx12Context::SetFrameCamera(const DirectX::XMFLOAT4X4& viewProj)
    {
        m_injectedViewProj = viewProj;
        m_useInjectedCamera = true;
    }

    /******************************************************************************
     * [DT-SSOT] SetFrameDeltaTime — Receive dt from App
     *
     * CONTRACT:
     *   - Called once per frame by App::Tick() BEFORE Render()
     *   - Sets m_lastDeltaTime for GetDeltaTime() accessor
     *
     * [CAMERA-OWNER] Camera ownership:
     *   - ThirdPerson: Engine owns camera; uses injected viewProj; no UpdateCamera
     *   - Free: Renderer owns camera via UpdateCamera(dt) called HERE only
     ******************************************************************************/
    void Dx12Context::SetFrameDeltaTime(float dt)
    {
        m_lastDeltaTime = dt;

        // [CAMERA-OWNER] Free camera update ONLY in Free mode
        if (ToggleSystem::GetCameraMode() == CameraMode::Free)
        {
            UpdateCamera(dt);
        }
    }

    void Dx12Context::SetHUDSnapshot(const HUDSnapshot& snap)
    {
        m_imguiLayer.SetHUDSnapshot(snap);
    }

    void Dx12Context::SetPawnTransform(float posX, float posY, float posZ, float yaw)
    {
        m_characterRenderer.SetPawnTransform(posX, posY, posZ, yaw);
    }

    Allocation Dx12Context::UpdateFrameConstants(FrameContext& ctx)
    {
        // Allocate from per-frame upload arena (unified front-door)
        Allocation frameCBAlloc = m_uploadArena.Allocate(CB_SIZE, CBV_ALIGNMENT, "FrameCB");

        XMFLOAT4X4 vpMatrix;
        if (m_useInjectedCamera)
        {
            // Use injected viewProj from ThirdPerson camera
            vpMatrix = m_injectedViewProj;
        }
        else
        {
            // Use FreeCamera (fallback)
            float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
            XMMATRIX viewProj = BuildFreeCameraViewProj(m_camera, aspect);
            XMStoreFloat4x4(&vpMatrix, viewProj);
        }

        // DirectXMath uses row-major, HLSL row_major matches - no transpose needed
        memcpy(frameCBAlloc.cpuPtr, &vpMatrix, sizeof(vpMatrix));

        return frameCBAlloc;
    }

    Allocation Dx12Context::UpdateTransforms(FrameContext& ctx)
    {
        // MT1: Skip transform generation if grid disabled
        if (!ToggleSystem::IsGridEnabled())
        {
            m_generatedTransformCount = 0;
            return {};  // Empty allocation
        }

        // Allocate from per-frame upload arena (unified front-door)
        Allocation transformsAlloc = m_uploadArena.Allocate(TRANSFORMS_SIZE, 256, "Transforms");

        float* transforms = static_cast<float*>(transformsAlloc.cpuPtr);
        uint32_t idx = 0;
        for (uint32_t z = 0; z < 100; ++z)
        {
            for (uint32_t x = 0; x < 100; ++x)
            {
                // Identity matrix with translation
                float tx = static_cast<float>(x) * 2.0f - 99.0f; // Center grid
                float ty = 1.5f;  // Day3.12 Fix: Match collision AABB Y=[0,3] center
                float tz = static_cast<float>(z) * 2.0f - 99.0f;

                // S7 Proof: sentinel_Instance0 - move instance 0 to distinct position
                if (idx == 0 && ToggleSystem::IsSentinelInstance0Enabled())
                {
                    tx = 150.0f;  // Far right
                    ty = 50.0f;   // Raised up
                    tz = 150.0f;  // Far back
                }

                // Row-major 4x4 scale+translate matrix (scale creates gaps between cubes)
                // Day3.12 Fix: Match collision AABB Y=[0,3] half-height
                const float scaleXZ = 0.9f;   // Half-width (matches cubeHalfXZ)
                const float scaleY = 1.5f;    // Half-height = (cubeMaxY - cubeMinY) / 2
                transforms[idx * 16 + 0] = scaleXZ;  transforms[idx * 16 + 1] = 0.0f;  transforms[idx * 16 + 2] = 0.0f;  transforms[idx * 16 + 3] = 0.0f;
                transforms[idx * 16 + 4] = 0.0f;  transforms[idx * 16 + 5] = scaleY;  transforms[idx * 16 + 6] = 0.0f;  transforms[idx * 16 + 7] = 0.0f;
                transforms[idx * 16 + 8] = 0.0f;  transforms[idx * 16 + 9] = 0.0f;  transforms[idx * 16 + 10] = scaleXZ; transforms[idx * 16 + 11] = 0.0f;
                transforms[idx * 16 + 12] = tx;   transforms[idx * 16 + 13] = ty;   transforms[idx * 16 + 14] = tz;   transforms[idx * 16 + 15] = 1.0f;

                ++idx;
            }
        }

        // Day3.12 Phase 4B+: Override fixture grid transforms to match collision AABBs
        // Skip when StepUpGridTest is active (mutual exclusion with T1/T2/T3 fixtures)
        bool fixtureOverrideActive = m_worldState && m_worldState->GetConfig().enableStepUpTestFixtures
            && !m_worldState->GetConfig().enableStepUpGridTest;

        // MODE_SNAPSHOT: Log renderer branch once per second
        static int s_frameCount = 0;
        if (m_worldState && (++s_frameCount % 60 == 1)) {
            char snap[256];
            sprintf_s(snap, "[RENDER_SNAP] fixtureOverride=%d fixtures=%d gridTest=%d extras=%zu\n",
                fixtureOverrideActive ? 1 : 0,
                m_worldState->GetConfig().enableStepUpTestFixtures ? 1 : 0,
                m_worldState->GetConfig().enableStepUpGridTest ? 1 : 0,
                m_worldState->GetExtras().size());
            OutputDebugStringA(snap);
        }

        if (fixtureOverrideActive)
        {
            float hxz = 0.9f;  // cubeHalfXZ

            // Override cube to include both cube + step height
            auto overrideWithStep = [&](uint16_t gridIdx, float stepHeight) {
                int gx = gridIdx % 100;
                int gz = gridIdx / 100;
                float cx = static_cast<float>(gx) * 2.0f - 99.0f;
                float cz = static_cast<float>(gz) * 2.0f - 99.0f;

                // Total height = cube (3.0) + step
                float totalHeight = 3.0f + stepHeight;
                float cy = totalHeight * 0.5f;  // Center Y
                float sy = totalHeight * 0.5f;  // Scale Y

                float* m = transforms + gridIdx * 16;
                m[0] = hxz;  m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f;
                m[4] = 0.0f; m[5] = sy;   m[6] = 0.0f; m[7] = 0.0f;
                m[8] = 0.0f; m[9] = 0.0f; m[10] = hxz; m[11] = 0.0f;
                m[12] = cx;  m[13] = cy;  m[14] = cz;  m[15] = 1.0f;
            };

            overrideWithStep(m_worldState->GetFixtureT1Idx(), 0.3f);      // T1: step h=0.3
            overrideWithStep(m_worldState->GetFixtureT2Idx(), 0.6f);      // T2: step h=0.6
            overrideWithStep(m_worldState->GetFixtureT3StepIdx(), 0.5f);  // T3: step h=0.5
        }

        // Render extras in BOTH modes (fixture mode: ceiling, grid test mode: 26 stairs)
        const auto& extras = m_worldState ? m_worldState->GetExtras()
                                          : std::vector<Engine::ExtraCollider>{};
        for (size_t i = 0; i < extras.size() && i < MaxExtraInstances; ++i)
        {
            const Engine::AABB& aabb = extras[i].aabb;
            float cx = (aabb.minX + aabb.maxX) * 0.5f;
            float cy = (aabb.minY + aabb.maxY) * 0.5f;
            float cz = (aabb.minZ + aabb.maxZ) * 0.5f;
            float sx = (aabb.maxX - aabb.minX) * 0.5f;
            float sy = (aabb.maxY - aabb.minY) * 0.5f;
            float sz = (aabb.maxZ - aabb.minZ) * 0.5f;

            uint32_t extraIdx = InstanceCount + static_cast<uint32_t>(i);
            float* m = transforms + extraIdx * 16;
            m[0] = sx;   m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f;
            m[4] = 0.0f; m[5] = sy;   m[6] = 0.0f; m[7] = 0.0f;
            m[8] = 0.0f; m[9] = 0.0f; m[10] = sz;  m[11] = 0.0f;
            m[12] = cx;  m[13] = cy;  m[14] = cz;  m[15] = 1.0f;
        }

#if defined(_DEBUG)
        // Sentinel: Zero out unused extra slots to catch over-draw
        for (uint32_t i = static_cast<uint32_t>(extras.size()); i < MaxExtraInstances; ++i)
        {
            uint32_t extraIdx = InstanceCount + i;
            float* m = transforms + extraIdx * 16;
            memset(m, 0, sizeof(float) * 16);  // All zeros = degenerate (invisible)
        }
#endif

        // Always include extras in count (works for both fixture and grid test modes)
        m_generatedTransformCount = InstanceCount + static_cast<uint32_t>(extras.size());

        return transformsAlloc;
    }

    void Dx12Context::RecordBarriersAndCopy(FrameContext& ctx, const Allocation& transformsAlloc)
    {
        // Get transforms buffer from registry
        ID3D12Resource* transformsResource = m_resourceRegistry.Get(ctx.transformsHandle);

        // Transition transforms buffer to COPY_DEST via state tracker
        m_stateTracker.Transition(transformsResource, D3D12_RESOURCE_STATE_COPY_DEST);
        m_stateTracker.FlushBarriers(m_commandList.Get());

        // Copy transforms from upload allocator to default buffer using CopyBufferRegion
        m_commandList->CopyBufferRegion(
            transformsResource, 0,                                    // dest, destOffset
            ctx.uploadAllocator.GetBuffer(), transformsAlloc.offset,  // src, srcOffset
            TRANSFORMS_SIZE);                                         // numBytes

        // Transition transforms buffer to SRV state
        m_stateTracker.Transition(transformsResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_stateTracker.FlushBarriers(m_commandList.Get());
    }

    void Dx12Context::RecordPasses(FrameContext& ctx, const Allocation& frameCBAlloc, uint32_t srvFrameIndex)
    {
        uint32_t frameResourceIndex = static_cast<uint32_t>(m_frameId % FrameCount);
        uint32_t backBufferIndex = GetBackBufferIndex();

        // Build RTV handle for current backbuffer
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<SIZE_T>(backBufferIndex) * m_rtvDescriptorSize;
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

        // Build orchestrator inputs
        PassOrchestratorInputs inputs = {};
        inputs.cmd = m_commandList.Get();
        inputs.frame = &ctx;
        inputs.descRing = &m_descRing;
        inputs.shaders = &m_shaderLibrary;
        inputs.scene = &m_scene;
        inputs.imguiLayer = &m_imguiLayer;
        inputs.backBuffer = m_backBuffers[backBufferIndex].Get();
        inputs.rtvHandle = rtvHandle;
        inputs.dsvHandle = dsvHandle;
        inputs.viewport = m_viewport;
        inputs.scissor = m_scissorRect;
        inputs.frameCBAddress = frameCBAlloc.gpuVA;
        inputs.srvTableHandle = m_frameRing.GetSrvGpuHandle(srvFrameIndex);

        // Geometry pass inputs
        inputs.geoInputs.drawMode = ToggleSystem::GetDrawMode();
        inputs.geoInputs.colorMode = ToggleSystem::GetColorMode();
        inputs.geoInputs.gridEnabled = ToggleSystem::IsGridEnabled();
        inputs.geoInputs.markersEnabled = ToggleSystem::IsMarkersEnabled();
        // Day3.12 Phase 4B+ Fix: Use generated count with safety clamp
        uint32_t maxDrawCount = InstanceCount + MaxExtraInstances;
        uint32_t drawCount = m_generatedTransformCount;
        if (drawCount > maxDrawCount)
        {
            char buf[128];
            sprintf_s(buf, "[MT1] CLAMP: gen=%u > max=%u, clamping\n", drawCount, maxDrawCount);
            OutputDebugStringA(buf);
            drawCount = maxDrawCount;
        }
        inputs.geoInputs.instanceCount = drawCount;
        // MT1: Pass generated transform count and frame ID for validation
        inputs.geoInputs.generatedTransformCount = m_generatedTransformCount;
        inputs.geoInputs.frameId = m_frameId;
        // MT2: Debug single instance mode
        inputs.geoInputs.debugSingleInstance = ToggleSystem::IsDebugSingleInstanceEnabled();
        inputs.geoInputs.debugInstanceIndex = ToggleSystem::GetDebugInstanceIndex();
        // Task B: Opaque PSO toggle
        inputs.geoInputs.useOpaquePSO = ToggleSystem::IsOpaquePSOEnabled();

        // Check if we need to record character pass (ThirdPerson mode)
        bool recordCharacter = (ToggleSystem::GetCameraMode() == CameraMode::ThirdPerson);

        // Execute passes via orchestrator
        // If recording character, skip ImGui in orchestrator (we'll record it after character)
        PassEnableFlags flags;
        flags.imguiPass = !recordCharacter;  // Disable ImGui if we're recording character
        uint32_t drawCalls = PassOrchestrator::Execute(inputs, flags);

        // Record character pass (separate from grid, only in ThirdPerson mode)
        if (recordCharacter)
        {
            // After PassOrchestrator::Execute, backbuffer is in PRESENT state
            // Transition back to RENDER_TARGET for character + ImGui
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_backBuffers[backBufferIndex].Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            m_commandList->ResourceBarrier(1, &barrier);

            // Re-set render targets (they were cleared when orchestrator finished)
            m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
            m_commandList->RSSetViewports(1, &m_viewport);
            m_commandList->RSSetScissorRects(1, &m_scissorRect);

            // 1. Allocate and write character matrices via UploadArena
            Allocation charAlloc = m_uploadArena.Allocate(
                CharacterRenderer::TransformsSize, 256, "CharXforms");

            // 2. Build and write matrices
            m_characterRenderer.WriteMatrices(charAlloc.cpuPtr);

            // 3. Build copy info (decoupled from FrameContext)
            CharacterCopyInfo copyInfo;
            copyInfo.uploadSrc = ctx.uploadAllocator.GetBuffer();
            copyInfo.srcOffset = charAlloc.offset;

            // 4. Build pass inputs and record
            CharacterPassInputs charInputs;
            charInputs.renderer = &m_characterRenderer;
            charInputs.copyInfo = copyInfo;
            charInputs.descRing = &m_descRing;
            charInputs.stateTracker = &m_stateTracker;
            charInputs.scene = &m_scene;
            charInputs.shaders = &m_shaderLibrary;
            charInputs.frameCBAddress = frameCBAlloc.gpuVA;

            CharacterPass::Record(m_commandList.Get(), charInputs);
            drawCalls += 1;  // Character is 1 draw call (6 instances)

            // Record ImGui pass (we skipped it in orchestrator)
            m_imguiLayer.RecordCommands(m_commandList.Get());
            drawCalls += 1;

            // Transition backbuffer back to PRESENT
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            m_commandList->ResourceBarrier(1, &barrier);
        }

        // PROOF: Once-per-second log showing frame indices and SRV offset match
        if (DiagnosticLogger::ShouldLog("PROOF_BIND"))
        {
            D3D12_GPU_DESCRIPTOR_HANDLE heapGpuStart = m_descRing.GetHeap()->GetGPUDescriptorHandleForHeapStart();
            D3D12_GPU_DESCRIPTOR_HANDLE boundHandle = m_frameRing.GetSrvGpuHandle(srvFrameIndex);
            SIZE_T actualOffset = boundHandle.ptr - heapGpuStart.ptr;
            SIZE_T expectedOffset = static_cast<SIZE_T>(frameResourceIndex) * m_descRing.GetDescriptorSize();
            bool match = (actualOffset == expectedOffset) || ToggleSystem::IsStompLifetimeEnabled();

            DiagnosticLogger::Log(
                "PROOF: frameId=%llu resIdx=%u backBuf=%u srvIdx=%u actual=%llu exp=%llu %s mode=%s\n",
                m_frameId, frameResourceIndex, backBufferIndex, srvFrameIndex,
                static_cast<unsigned long long>(actualOffset),
                static_cast<unsigned long long>(expectedOffset),
                match ? "OK" : "MISMATCH",
                ToggleSystem::GetDrawModeName());
        }

        // PASS proof log (throttled 1/sec): shows PSO pointers, SRV index, grid state, mode
        DiagnosticLogger::LogThrottled("PASS",
            "PASS: floor_pso=%p cubes_pso=%p cubes_srvIdx=%u grid=%d mode=%s draws=%u\n",
            m_shaderLibrary.GetFloorPSO(), m_shaderLibrary.GetPSO(),
            srvFrameIndex, ToggleSystem::IsGridEnabled() ? 1 : 0,
            ToggleSystem::GetDrawModeName(), drawCalls);
    }

    void Dx12Context::ExecuteAndPresent(FrameContext& ctx)
    {
        // Close and execute command list
        ThrowIfFailed(m_commandList->Close(), "Failed to close command list\n");

        ID3D12CommandList* commandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, commandLists);

        // End frame - signal fence
        m_frameRing.EndFrame(m_commandQueue.Get(), ctx);

        // Record descriptor ring frame end (attach fence to this frame's allocations)
        m_descRing.EndFrame(m_frameRing.GetCurrentFenceValue());

        // Present
        HRESULT hr = m_swapChain->Present(1, 0); // VSync on
        if (FAILED(hr))
        {
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            {
                HRESULT reason = m_device->GetDeviceRemovedReason();
                char errBuf[256];
                sprintf_s(errBuf, "Device removed! Reason: 0x%08X\n", reason);
                OutputDebugStringA(errBuf);
            }
        }
    }

    //-------------------------------------------------------------------------
    // Main Render Loop
    //-------------------------------------------------------------------------

    void Dx12Context::Render()
    {
        if (!m_initialized)
            return;

        // [DT-SSOT] dt and camera handled via SetFrameDeltaTime() before Render()

        // Begin frame: fence wait + allocator reset
        uint32_t frameResourceIndex = static_cast<uint32_t>(m_frameId % FrameCount);
        FrameContext& frameCtx = m_frameRing.BeginFrame(m_frameId);

        // Begin upload arena with per-frame allocator
        bool diagEnabled = ToggleSystem::IsUploadDiagEnabled();
        m_uploadArena.Begin(&frameCtx.uploadAllocator, diagEnabled);

        // Retire completed descriptor ring frames
        uint64_t completedFence = m_frameRing.GetFence()->GetCompletedValue();
        m_descRing.BeginFrame(completedFence);

        // Phase 1: Upload (CPU writes to upload heap via linear allocator)
        Allocation frameCBAlloc = UpdateFrameConstants(frameCtx);
        Allocation transformsAlloc = UpdateTransforms(frameCtx);

        // Phase 2: Record (command list recording)
        // Start CPU timing for command recording
        LARGE_INTEGER recordStart, recordEnd, perfFreq;
        QueryPerformanceFrequency(&perfFreq);
        QueryPerformanceCounter(&recordStart);

        ThrowIfFailed(m_commandList->Reset(frameCtx.cmdAllocator.Get(), m_shaderLibrary.GetPSO()),
            "Failed to reset command list\n");

        // Start ImGui frame (NewFrame called here)
        m_imguiLayer.BeginFrame();

        // S7 Proof: stomp_Lifetime - use wrong frame index for SRV (will cause stomp/flicker)
        uint32_t srvFrameIndex = frameResourceIndex;
        if (ToggleSystem::IsStompLifetimeEnabled())
        {
            srvFrameIndex = (frameResourceIndex + 1) % FrameCount; // Wrong frame!
            DiagnosticLogger::LogThrottled("STOMP", "WARNING: stomp_Lifetime ACTIVE - press F2 to disable\n");
        }

        RecordBarriersAndCopy(frameCtx, transformsAlloc);

        // Build ImGui draw data (must be before RecordCommands)
        m_imguiLayer.RenderHUD();

        RecordPasses(frameCtx, frameCBAlloc, srvFrameIndex);

        // End CPU timing for command recording
        QueryPerformanceCounter(&recordEnd);
        double cpuRecordMs = static_cast<double>(recordEnd.QuadPart - recordStart.QuadPart) * 1000.0 / static_cast<double>(perfFreq.QuadPart);

        // Log evidence: mode, draws, cpu_record_ms, frameId (throttled to once per second)
        DiagnosticLogger::LogThrottled("EVIDENCE", "mode=%s draws=%u cpu_record_ms=%.3f frameId=%u\n",
            ToggleSystem::GetDrawModeName(),
            ToggleSystem::IsGridEnabled() ? (ToggleSystem::GetDrawMode() == DrawMode::Instanced ? 2 : InstanceCount + 1) : 1,
            cpuRecordMs, frameResourceIndex);

        // Diagnostic: log viewport/scissor/client mismatch
        {
            static uint64_t s_lastLogFrame = 0;
            bool shouldLog = (m_frameId == 0) || ToggleSystem::ShouldLogDiagnostics() || ((m_frameId - s_lastLogFrame) >= 60);
            if (shouldLog)
            {
                uint32_t drawCalls = ToggleSystem::IsGridEnabled() ?
                    (ToggleSystem::GetDrawMode() == DrawMode::Instanced ? 2 : InstanceCount + 1) : 1;
                if (ToggleSystem::IsMarkersEnabled()) drawCalls += 1;

                RECT clientRect;
                GetClientRect(m_hwnd, &clientRect);
                char diagBuf[512];
                sprintf_s(diagBuf,
                    "DIAG[%llu]: client=%dx%d viewport=(%.0f,%.0f,%.0f,%.0f) scissor=(%ld,%ld,%ld,%ld)=%ldx%ld mode=%s instances=%u draws=%u\n",
                    m_frameId,
                    clientRect.right - clientRect.left, clientRect.bottom - clientRect.top,
                    m_viewport.TopLeftX, m_viewport.TopLeftY, m_viewport.Width, m_viewport.Height,
                    m_scissorRect.left, m_scissorRect.top, m_scissorRect.right, m_scissorRect.bottom,
                    m_scissorRect.right - m_scissorRect.left, m_scissorRect.bottom - m_scissorRect.top,
                    ToggleSystem::GetDrawModeName(), InstanceCount, drawCalls);
                OutputDebugStringA(diagBuf);
                s_lastLogFrame = m_frameId;
                ToggleSystem::ClearDiagnosticLog();
            }
        }

        // End upload arena and pass metrics to HUD
        m_uploadArena.End();
        m_imguiLayer.SetUploadArenaMetrics(m_uploadArena.GetLastSnapshot());

        // Phase 3: Execute & Present
        ExecuteAndPresent(frameCtx);

        // Reset frame-scoped camera injection for next frame
        m_useInjectedCamera = false;

        // Increment monotonic frame counter
        ++m_frameId;
    }

    void Dx12Context::Shutdown()
    {
        if (!m_initialized)
            return;

        // Wait for all GPU work to complete
        m_frameRing.WaitForAll();

        // Shutdown scene
        m_scene.Shutdown();

        // Shutdown character renderer
        m_characterRenderer.Shutdown();

        // Shutdown geometry factory
        m_geometryFactory.Shutdown();

        // Shutdown ImGui layer
        m_imguiLayer.Shutdown();

        // Shutdown shader library
        m_shaderLibrary.Shutdown();

        // Shutdown resource registry
        m_resourceRegistry.Shutdown();

        // Shutdown frame ring (releases fence, allocators)
        m_frameRing.Shutdown();

        // Release all other resources (ComPtr handles this automatically)
        m_commandList.Reset();

        for (UINT i = 0; i < FrameCount; ++i)
            m_backBuffers[i].Reset();

        m_rtvHeap.Reset();
        m_dsvHeap.Reset();
        m_depthBuffer.Reset();
        m_descRing.Shutdown();
        m_swapChain.Reset();
        m_commandQueue.Reset();
        m_device.Reset();
        m_adapter.Reset();
        m_factory.Reset();

        m_hwnd = nullptr;
        m_initialized = false;

        OutputDebugStringA("Dx12Context shutdown complete\n");
    }
}
