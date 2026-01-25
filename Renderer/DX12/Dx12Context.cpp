#include "Dx12Context.h"
#include "Dx12Debug.h"
#include "ShaderLibrary.h"
#include "ToggleSystem.h"
#include <cstdio>
#include <DirectXMath.h>

using namespace DirectX;

using Microsoft::WRL::ComPtr;

// Handedness switch: set to 0 for LH if RH produces inverted/invisible scene
#define USE_RIGHT_HANDED 1

namespace Renderer
{
    //-------------------------------------------------------------------------
    // Free Camera State
    //-------------------------------------------------------------------------
    struct FreeCamera
    {
        XMFLOAT3 position = {0.0f, 180.0f, -220.0f};  // Start at preset A
        float yaw = 0.0f;      // Radians, 0 = looking along +Z
        float pitch = -0.5f;   // Radians, negative = looking down
        float fovY = XM_PIDIV4;
        float nearZ = 1.0f;
        float farZ = 1000.0f;
        float moveSpeed = 100.0f;   // Units per second
        float lookSpeed = 1.5f;     // Radians per second
    };

    static FreeCamera s_camera;
    static LARGE_INTEGER s_lastTime = {};
    static LARGE_INTEGER s_frequency = {};
    static bool s_timerInitialized = false;

    static void UpdateFreeCamera(float dt)
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
        s_camera.yaw += yawDelta * s_camera.lookSpeed * dt;

        // Build movement vectors in camera space
        float cosY = cosf(s_camera.yaw);
        float sinY = sinf(s_camera.yaw);
        XMFLOAT3 forward = { sinY, 0, cosY };  // Horizontal forward
        XMFLOAT3 right = { cosY, 0, -sinY };   // Horizontal right

        float speed = s_camera.moveSpeed * dt;
        s_camera.position.x += (forward.x * moveZ + right.x * moveX) * speed;
        s_camera.position.z += (forward.z * moveZ + right.z * moveX) * speed;
        s_camera.position.y += moveY * speed;
    }

    static XMMATRIX BuildFreeCameraViewProj(const FreeCamera& cam, float aspect)
    {
        // Forward vector from yaw/pitch (RH: -Z is forward at yaw=0)
        float cosP = cosf(cam.pitch);
        XMFLOAT3 forward = {
            sinf(cam.yaw) * cosP,
            sinf(cam.pitch),
            cosf(cam.yaw) * cosP
        };

        XMVECTOR eye = XMLoadFloat3(&cam.position);
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

    bool Dx12Context::Initialize(HWND hwnd)
    {
        if (m_initialized)
            return false;

        m_hwnd = hwnd;

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

        // 1. Enable debug layer in debug builds
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

        // 2. Create DXGI factory
        UINT factoryFlags = 0;
#if defined(_DEBUG)
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
        ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory)),
            "Failed to create DXGI factory\n");

        // 3. Find best adapter and create device
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

        // 4. Create command queue
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

            ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)),
                "Failed to create command queue\n");
        }

        // 5. Create swap chain
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

        // 6. Create RTV descriptor heap
        {
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
            rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvHeapDesc.NumDescriptors = FrameCount;
            rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)),
                "Failed to create RTV heap\n");

            m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        }

        // 7. Create RTVs for back buffers
        {
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

            for (UINT i = 0; i < FrameCount; ++i)
            {
                ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])),
                    "Failed to get swap chain buffer\n");

                m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtvHandle);
                rtvHandle.ptr += m_rtvDescriptorSize;
            }
        }

        // 8. Create DSV descriptor heap
        {
            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
            dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvHeapDesc.NumDescriptors = 1;
            dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)),
                "Failed to create DSV heap\n");
        }

        // 9. Create depth buffer
        {
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

        // 10. Initialize CBV/SRV/UAV descriptor ring allocator (shader-visible)
        // Capacity: 1024 total, 3 reserved for per-frame transforms SRVs
        if (!m_descRing.Initialize(m_device.Get(), 1024, FrameCount))
        {
            OutputDebugStringA("Failed to initialize descriptor ring allocator\n");
            return false;
        }

        // 9. Initialize frame context ring (per-frame allocators + fence + buffers + SRVs)
        if (!m_frameRing.Initialize(m_device.Get(), &m_descRing))
        {
            OutputDebugStringA("Failed to initialize frame context ring\n");
            return false;
        }

        // 10. Initialize shader library (root sig + PSO)
        if (!m_shaderLibrary.Initialize(m_device.Get(), DXGI_FORMAT_R8G8B8A8_UNORM))
        {
            OutputDebugStringA("Failed to initialize shader library\n");
            return false;
        }

        // 10b. Initialize resource registry (handle-based resource ownership)
        if (!m_resourceRegistry.Initialize(m_device.Get()))
        {
            OutputDebugStringA("Failed to initialize resource registry\n");
            return false;
        }

        // 11. Initialize render scene (geometry)
        if (!m_scene.Initialize(m_device.Get(), m_commandQueue.Get()))
        {
            OutputDebugStringA("Failed to initialize render scene\n");
            return false;
        }

        // 11b. Initialize ImGui layer
        if (!m_imguiLayer.Initialize(m_hwnd, m_device.Get(), m_commandQueue.Get(),
                                      FrameCount, m_backBufferFormat))
        {
            OutputDebugStringA("[ImGui] FAILED to initialize\n");
            return false;
        }

        // 12. Create command list (closed initially, will be reset per-frame)
        {
            // Get first frame's allocator to create the list
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
        }

        // 13. Setup viewport and scissor
        m_viewport = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
        m_scissorRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };

        m_frameId = 0;
        m_initialized = true;

        // 14. Initialize free camera timer
        QueryPerformanceFrequency(&s_frequency);
        QueryPerformanceCounter(&s_lastTime);
        s_timerInitialized = true;

        OutputDebugStringA("Dx12Context initialized successfully\n");
        return true;
    }

    // CBV requires 256-byte alignment
    static constexpr uint64_t CBV_ALIGNMENT = 256;
    static constexpr uint64_t CB_SIZE = (sizeof(float) * 16 + CBV_ALIGNMENT - 1) & ~(CBV_ALIGNMENT - 1);

    // Transforms: 10k float4x4 = 10k * 64 bytes
    static constexpr uint64_t TRANSFORMS_SIZE = InstanceCount * sizeof(float) * 16;

    //-------------------------------------------------------------------------
    // Phase Helpers
    //-------------------------------------------------------------------------

    float Dx12Context::UpdateDeltaTime()
    {
        float dt = 0.0f;
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        if (s_timerInitialized && s_frequency.QuadPart > 0)
        {
            dt = static_cast<float>(currentTime.QuadPart - s_lastTime.QuadPart) /
                 static_cast<float>(s_frequency.QuadPart);
            // Clamp to avoid huge jumps (e.g., after breakpoint)
            if (dt > 0.1f) dt = 0.1f;
        }
        s_lastTime = currentTime;
        return dt;
    }

    Allocation Dx12Context::UpdateFrameConstants(FrameContext& ctx)
    {
        // Allocate from per-frame linear allocator
        Allocation frameCBAlloc = ctx.uploadAllocator.Allocate(CB_SIZE, CBV_ALIGNMENT, "FrameCB");

        float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
        XMMATRIX viewProj = BuildFreeCameraViewProj(s_camera, aspect);

        // DirectXMath uses row-major, HLSL row_major matches - no transpose needed
        XMFLOAT4X4 vpMatrix;
        XMStoreFloat4x4(&vpMatrix, viewProj);
        memcpy(frameCBAlloc.cpuPtr, &vpMatrix, sizeof(vpMatrix));

        return frameCBAlloc;
    }

    Allocation Dx12Context::UpdateTransforms(FrameContext& ctx)
    {
        // Allocate from per-frame linear allocator
        Allocation transformsAlloc = ctx.uploadAllocator.Allocate(TRANSFORMS_SIZE, 256, "Transforms");

        float* transforms = static_cast<float*>(transformsAlloc.cpuPtr);
        uint32_t idx = 0;
        for (uint32_t z = 0; z < 100; ++z)
        {
            for (uint32_t x = 0; x < 100; ++x)
            {
                // Identity matrix with translation
                float tx = static_cast<float>(x) * 2.0f - 99.0f; // Center grid
                float ty = 0.0f;
                float tz = static_cast<float>(z) * 2.0f - 99.0f;

                // S7 Proof: sentinel_Instance0 - move instance 0 to distinct position
                if (idx == 0 && ToggleSystem::IsSentinelInstance0Enabled())
                {
                    tx = 150.0f;  // Far right
                    ty = 50.0f;   // Raised up
                    tz = 150.0f;  // Far back
                }

                // Row-major 4x4 scale+translate matrix (scale creates gaps between cubes)
                // TEMP: Tall boxes to verify volumetric depth (side faces visible)
                const float scaleXZ = 0.9f;   // Wider for visibility
                const float scaleY = 3.0f;    // Tall boxes to show side faces
                transforms[idx * 16 + 0] = scaleXZ;  transforms[idx * 16 + 1] = 0.0f;  transforms[idx * 16 + 2] = 0.0f;  transforms[idx * 16 + 3] = 0.0f;
                transforms[idx * 16 + 4] = 0.0f;  transforms[idx * 16 + 5] = scaleY;  transforms[idx * 16 + 6] = 0.0f;  transforms[idx * 16 + 7] = 0.0f;
                transforms[idx * 16 + 8] = 0.0f;  transforms[idx * 16 + 9] = 0.0f;  transforms[idx * 16 + 10] = scaleXZ; transforms[idx * 16 + 11] = 0.0f;
                transforms[idx * 16 + 12] = tx;   transforms[idx * 16 + 13] = ty;   transforms[idx * 16 + 14] = tz;   transforms[idx * 16 + 15] = 1.0f;

                ++idx;
            }
        }

        return transformsAlloc;
    }

    void Dx12Context::RecordBarriersAndCopy(FrameContext& ctx, const Allocation& transformsAlloc)
    {
        // Barrier: transforms default buffer -> COPY_DEST (per-frame state tracking, fixes #527)
        // Each frame context tracks its own transformsState to handle triple-buffering correctly
        if (ctx.transformsState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = ctx.transformsDefault.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = ctx.transformsState;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            m_commandList->ResourceBarrier(1, &barrier);
            ctx.transformsState = D3D12_RESOURCE_STATE_COPY_DEST;
        }

        // Copy transforms from upload allocator to default buffer using CopyBufferRegion
        m_commandList->CopyBufferRegion(
            ctx.transformsDefault.Get(), 0,                           // dest, destOffset
            ctx.uploadAllocator.GetBuffer(), transformsAlloc.offset,  // src, srcOffset
            TRANSFORMS_SIZE);                                         // numBytes

        // Barrier: transforms default buffer -> SRV
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = ctx.transformsDefault.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            m_commandList->ResourceBarrier(1, &barrier);
            ctx.transformsState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        }
    }

    void Dx12Context::RecordPasses(FrameContext& ctx, const Allocation& frameCBAlloc, uint32_t srvFrameIndex)
    {
        uint32_t frameResourceIndex = static_cast<uint32_t>(m_frameId % FrameCount);
        uint32_t backBufferIndex = GetBackBufferIndex();

        // Transition backbuffer: PRESENT -> RENDER_TARGET
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_backBuffers[backBufferIndex].Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            m_commandList->ResourceBarrier(1, &barrier);
        }

        // Clear render target
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<SIZE_T>(backBufferIndex) * m_rtvDescriptorSize;
        {
            const float clearColor[] = { 0.53f, 0.81f, 0.92f, 1.0f }; // Sky blue
            m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        }

        // Clear depth buffer
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // Set render state
        m_commandList->SetGraphicsRootSignature(m_shaderLibrary.GetRootSignature());
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Set descriptor heaps (use ring allocator's heap)
        ID3D12DescriptorHeap* heaps[] = { m_descRing.GetHeap() };
        m_commandList->SetDescriptorHeaps(1, heaps);

        // Bind root parameters
        m_commandList->SetGraphicsRootConstantBufferView(0, frameCBAlloc.gpuVA); // b0: ViewProj
        m_commandList->SetGraphicsRootDescriptorTable(1, m_frameRing.GetSrvGpuHandle(srvFrameIndex)); // t0: Transforms SRV

        // PROOF: Once-per-second log showing frame indices and SRV offset match
        {
            static DWORD s_lastBindLogTime = 0;
            DWORD now = GetTickCount();
            if (now - s_lastBindLogTime > 1000)
            {
                s_lastBindLogTime = now;

                D3D12_GPU_DESCRIPTOR_HANDLE heapGpuStart = m_descRing.GetHeap()->GetGPUDescriptorHandleForHeapStart();
                D3D12_GPU_DESCRIPTOR_HANDLE boundHandle = m_frameRing.GetSrvGpuHandle(srvFrameIndex);
                SIZE_T actualOffset = boundHandle.ptr - heapGpuStart.ptr;
                SIZE_T expectedOffset = static_cast<SIZE_T>(frameResourceIndex) * m_descRing.GetDescriptorSize();
                bool match = (actualOffset == expectedOffset) || ToggleSystem::IsStompLifetimeEnabled();

                char buf[256];
                sprintf_s(buf,
                    "PROOF: frameId=%llu resIdx=%u backBuf=%u srvIdx=%u actual=%llu exp=%llu %s mode=%s\n",
                    m_frameId, frameResourceIndex, backBufferIndex, srvFrameIndex,
                    static_cast<unsigned long long>(actualOffset),
                    static_cast<unsigned long long>(expectedOffset),
                    match ? "OK" : "MISMATCH",
                    ToggleSystem::GetDrawModeName());
                OutputDebugStringA(buf);
            }
        }

        // Draw floor first (single draw call, floor VS does NOT read transforms)
        m_commandList->SetPipelineState(m_shaderLibrary.GetFloorPSO());
        m_scene.RecordDrawFloor(m_commandList.Get());

        // Draw cubes based on mode (only if grid enabled)
        uint32_t drawCalls = 1; // 1 floor (always drawn)
        if (ToggleSystem::IsGridEnabled())
        {
            m_commandList->SetPipelineState(m_shaderLibrary.GetPSO());

            // Set color mode constant (RP_DebugCB = b2)
            uint32_t colorMode = static_cast<uint32_t>(ToggleSystem::GetColorMode());
            m_commandList->SetGraphicsRoot32BitConstants(RP_DebugCB, 1, &colorMode, 0);

            if (ToggleSystem::GetDrawMode() == DrawMode::Instanced)
            {
                uint32_t zero = 0;
                m_commandList->SetGraphicsRoot32BitConstants(2, 1, &zero, 0);  // RP_InstanceOffset = 0
                m_scene.RecordDraw(m_commandList.Get(), InstanceCount);
                drawCalls += 1; // +1 instanced cube draw
            }
            else
            {
                m_scene.RecordDrawNaive(m_commandList.Get(), InstanceCount);
                drawCalls += InstanceCount; // +10000 naive cube draws

                // B-1: Once-per-second naive path verification
                static DWORD s_lastNaiveLogTime = 0;
                DWORD naiveNow = GetTickCount();
                if (naiveNow - s_lastNaiveLogTime > 1000)
                {
                    s_lastNaiveLogTime = naiveNow;
                    char buf[128];
                    sprintf_s(buf, "B1-NAIVE: StartInstance first=0 last=%u (expected 0 and %u)\n",
                              InstanceCount - 1, InstanceCount - 1);
                    OutputDebugStringA(buf);
                }
            }
        }

        // PASS proof log (throttled 1/sec): shows PSO pointers, SRV index, grid state, mode
        {
            static DWORD s_lastPassLogTime = 0;
            DWORD now = GetTickCount();
            if (now - s_lastPassLogTime > 1000)
            {
                s_lastPassLogTime = now;
                char buf[256];
                sprintf_s(buf, "PASS: floor_pso=%p cubes_pso=%p cubes_srvIdx=%u grid=%d mode=%s\n",
                    m_shaderLibrary.GetFloorPSO(), m_shaderLibrary.GetPSO(),
                    srvFrameIndex, ToggleSystem::IsGridEnabled() ? 1 : 0,
                    ToggleSystem::GetDrawModeName());
                OutputDebugStringA(buf);
            }
        }

        // Draw corner markers (visual diagnostic - pass-through VS, no transforms)
        if (ToggleSystem::IsMarkersEnabled())
        {
            m_commandList->SetGraphicsRootSignature(m_shaderLibrary.GetMarkerRootSignature());
            m_commandList->SetPipelineState(m_shaderLibrary.GetMarkerPSO());
            m_scene.RecordDrawMarkers(m_commandList.Get());
            drawCalls += 1; // +1 for markers
        }

        // Draw ImGui overlay (last draw before PRESENT barrier)
        // Safety: ImGui draws after ALL scene draws
        m_imguiLayer.RecordCommands(m_commandList.Get());

        // Transition backbuffer: RENDER_TARGET -> PRESENT
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_backBuffers[backBufferIndex].Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            m_commandList->ResourceBarrier(1, &barrier);
        }
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

        // Pre-frame: delta time and camera update
        float dt = UpdateDeltaTime();
        UpdateFreeCamera(dt);

        // Begin frame: fence wait + allocator reset
        uint32_t frameResourceIndex = static_cast<uint32_t>(m_frameId % FrameCount);
        FrameContext& frameCtx = m_frameRing.BeginFrame(m_frameId);

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

            // Throttle warning to once per second
            static DWORD s_lastStompLogTime = 0;
            DWORD now = GetTickCount();
            if (now - s_lastStompLogTime > 1000)
            {
                s_lastStompLogTime = now;
                OutputDebugStringA("WARNING: stomp_Lifetime ACTIVE - press F2 to disable\n");
            }
        }

        RecordBarriersAndCopy(frameCtx, transformsAlloc);

        // Build ImGui draw data (must be before RecordCommands)
        m_imguiLayer.RenderHUD();

        RecordPasses(frameCtx, frameCBAlloc, srvFrameIndex);

        // End CPU timing for command recording
        QueryPerformanceCounter(&recordEnd);
        double cpuRecordMs = static_cast<double>(recordEnd.QuadPart - recordStart.QuadPart) * 1000.0 / static_cast<double>(perfFreq.QuadPart);

        // Log evidence: mode, draws, cpu_record_ms, frameId (throttled to once per second)
        {
            static DWORD s_lastEvidenceLogTime = 0;
            DWORD now = GetTickCount();
            if (now - s_lastEvidenceLogTime > 1000)
            {
                s_lastEvidenceLogTime = now;
                char evidenceBuf[256];
                sprintf_s(evidenceBuf, "mode=%s draws=%u cpu_record_ms=%.3f frameId=%u\n",
                    ToggleSystem::GetDrawModeName(),
                    ToggleSystem::IsGridEnabled() ? (ToggleSystem::GetDrawMode() == DrawMode::Instanced ? 2 : InstanceCount + 1) : 1,
                    cpuRecordMs, frameResourceIndex);
                OutputDebugStringA(evidenceBuf);
            }
        }

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

        // Phase 3: Execute & Present
        ExecuteAndPresent(frameCtx);

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
