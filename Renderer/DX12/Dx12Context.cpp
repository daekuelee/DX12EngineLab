#include "Dx12Context.h"
#include "Dx12Debug.h"
#include "ToggleSystem.h"
#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace Renderer
{
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

        // 8. Create CBV/SRV/UAV descriptor heap (shader-visible)
        {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = FrameCount; // One SRV slot per frame
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            ThrowIfFailed(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvSrvUavHeap)),
                "Failed to create CBV/SRV/UAV heap\n");

            m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // 9. Initialize frame context ring (per-frame allocators + fence + buffers + SRVs)
        if (!m_frameRing.Initialize(m_device.Get(), m_cbvSrvUavHeap.Get(), m_cbvSrvUavDescriptorSize))
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

        // 11. Initialize render scene (geometry)
        if (!m_scene.Initialize(m_device.Get(), m_commandQueue.Get()))
        {
            OutputDebugStringA("Failed to initialize render scene\n");
            return false;
        }

        // 12. Create command list (closed initially, will be reset per-frame)
        {
            // Get first frame's allocator to create the list
            FrameContext& firstFrame = m_frameRing.BeginFrame(0);

            ThrowIfFailed(m_device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                firstFrame.allocator.Get(),
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

        OutputDebugStringA("Dx12Context initialized successfully\n");
        return true;
    }

    void Dx12Context::Render()
    {
        if (!m_initialized)
            return;

        // 1. Begin frame - get fence-gated frame context using monotonic frameId
        uint32_t frameResourceIndex = static_cast<uint32_t>(m_frameId % FrameCount);
        FrameContext& frameCtx = m_frameRing.BeginFrame(m_frameId);

        // 2. Get backbuffer index (separate from frame resource index!)
        uint32_t backBufferIndex = GetBackBufferIndex();

        // 3. Update ViewProj constant buffer (simple orthographic for now)
        {
            float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
            float scale = 0.01f; // Scale down to fit 100x100 grid

            // Simple orthographic projection matrix (row-major)
            // Maps world coords to NDC: x/y scaled, z passed through
            float viewProj[16] = {
                scale / aspect, 0.0f,  0.0f, 0.0f,
                0.0f,           scale, 0.0f, 0.0f,
                0.0f,           0.0f,  1.0f, 0.0f,
                0.0f,           0.0f,  0.0f, 1.0f
            };

            memcpy(frameCtx.frameCBMapped, viewProj, sizeof(viewProj));
        }

        // 4. Update transforms (100x100 grid = 10k instances)
        {
            float* transforms = static_cast<float*>(frameCtx.transformsUploadMapped);
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

                    // Row-major 4x4 identity with translation in last row
                    transforms[idx * 16 + 0] = 1.0f;  transforms[idx * 16 + 1] = 0.0f;  transforms[idx * 16 + 2] = 0.0f;  transforms[idx * 16 + 3] = 0.0f;
                    transforms[idx * 16 + 4] = 0.0f;  transforms[idx * 16 + 5] = 1.0f;  transforms[idx * 16 + 6] = 0.0f;  transforms[idx * 16 + 7] = 0.0f;
                    transforms[idx * 16 + 8] = 0.0f;  transforms[idx * 16 + 9] = 0.0f;  transforms[idx * 16 + 10] = 1.0f; transforms[idx * 16 + 11] = 0.0f;
                    transforms[idx * 16 + 12] = tx;   transforms[idx * 16 + 13] = ty;   transforms[idx * 16 + 14] = tz;   transforms[idx * 16 + 15] = 1.0f;

                    ++idx;
                }
            }
        }

        // 5. Reset command list with this frame's allocator
        // Start CPU timing for command recording
        LARGE_INTEGER recordStart, recordEnd, perfFreq;
        QueryPerformanceFrequency(&perfFreq);
        QueryPerformanceCounter(&recordStart);

        ThrowIfFailed(m_commandList->Reset(frameCtx.allocator.Get(), m_shaderLibrary.GetPSO()),
            "Failed to reset command list\n");

        // 6. Barrier: transforms default buffer COPY_DEST (already in COPY_DEST on first frame)
        // Note: Buffer starts in COPY_DEST state, first barrier is SRV->COPY_DEST after first use
        // For simplicity, we track state with a static - proper tracking would use ResourceStateTracker
        static bool s_firstFrame = true;
        if (!s_firstFrame)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = frameCtx.transformsDefault.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            m_commandList->ResourceBarrier(1, &barrier);
        }

        // 7. Copy transforms from upload to default
        m_commandList->CopyResource(frameCtx.transformsDefault.Get(), frameCtx.transformsUpload.Get());

        // 8. Barrier: transforms default buffer -> SRV
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = frameCtx.transformsDefault.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            m_commandList->ResourceBarrier(1, &barrier);
        }

        s_firstFrame = false;

        // 9. Transition backbuffer: PRESENT -> RENDER_TARGET
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_backBuffers[backBufferIndex].Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            m_commandList->ResourceBarrier(1, &barrier);
        }

        // 10. Clear render target
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<SIZE_T>(backBufferIndex) * m_rtvDescriptorSize;
        {
            const float clearColor[] = { 0.05f, 0.07f, 0.10f, 1.0f };
            m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        }

        // 11. Set render state
        m_commandList->SetGraphicsRootSignature(m_shaderLibrary.GetRootSignature());
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 12. Set descriptor heaps
        ID3D12DescriptorHeap* heaps[] = { m_cbvSrvUavHeap.Get() };
        m_commandList->SetDescriptorHeaps(1, heaps);

        // 13. Bind root parameters
        // S7 Proof: break_RPIndexSwap - swap root param indices (will cause wrong bindings)
        uint32_t rpFrameCB = 0;
        uint32_t rpTransforms = 1;
        if (ToggleSystem::IsBreakRPIndexSwapEnabled())
        {
            rpFrameCB = 1;     // Wrong! Should be 0
            rpTransforms = 0;  // Wrong! Should be 1
            OutputDebugStringA("S7: break_RPIndexSwap ACTIVE - root params swapped!\n");
        }

        // S7 Proof: stomp_Lifetime - use wrong frame index for SRV (will cause stomp/flicker)
        uint32_t srvFrameIndex = frameResourceIndex;
        if (ToggleSystem::IsStompLifetimeEnabled())
        {
            srvFrameIndex = (frameResourceIndex + 1) % FrameCount; // Wrong frame!
            OutputDebugStringA("S7: stomp_Lifetime ACTIVE - using wrong frame SRV!\n");
        }

        m_commandList->SetGraphicsRootConstantBufferView(rpFrameCB, frameCtx.frameCBGpuVA); // b0: ViewProj
        m_commandList->SetGraphicsRootDescriptorTable(rpTransforms, m_frameRing.GetSrvGpuHandle(srvFrameIndex)); // t0: Transforms SRV

        // 14. Draw scene based on mode
        uint32_t drawCalls = 0;
        if (ToggleSystem::GetDrawMode() == DrawMode::Instanced)
        {
            m_scene.RecordDraw(m_commandList.Get(), InstanceCount);
            drawCalls = 1;
        }
        else
        {
            m_scene.RecordDrawNaive(m_commandList.Get(), InstanceCount);
            drawCalls = InstanceCount;
        }

        // 15. Transition backbuffer: RENDER_TARGET -> PRESENT
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_backBuffers[backBufferIndex].Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            m_commandList->ResourceBarrier(1, &barrier);
        }

        // 16. Close and execute command list
        ThrowIfFailed(m_commandList->Close(), "Failed to close command list\n");

        // End CPU timing for command recording
        QueryPerformanceCounter(&recordEnd);
        double cpuRecordMs = static_cast<double>(recordEnd.QuadPart - recordStart.QuadPart) * 1000.0 / static_cast<double>(perfFreq.QuadPart);

        // Log evidence: mode, draws, cpu_record_ms, frameId (S6 acceptance)
        {
            char evidenceBuf[256];
            sprintf_s(evidenceBuf, "mode=%s draws=%u cpu_record_ms=%.3f frameId=%u\n",
                ToggleSystem::GetDrawModeName(), drawCalls, cpuRecordMs, frameResourceIndex);
            OutputDebugStringA(evidenceBuf);
        }

        ID3D12CommandList* commandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, commandLists);

        // 17. End frame - signal fence
        m_frameRing.EndFrame(m_commandQueue.Get(), frameCtx);

        // 18. Present
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

        // 19. Increment monotonic frame counter
        m_frameId++;
    }

    void Dx12Context::Shutdown()
    {
        if (!m_initialized)
            return;

        // Wait for all GPU work to complete
        m_frameRing.WaitForAll();

        // Shutdown scene
        m_scene.Shutdown();

        // Shutdown shader library
        m_shaderLibrary.Shutdown();

        // Shutdown frame ring (releases fence, allocators)
        m_frameRing.Shutdown();

        // Release all other resources (ComPtr handles this automatically)
        m_commandList.Reset();

        for (UINT i = 0; i < FrameCount; ++i)
            m_backBuffers[i].Reset();

        m_rtvHeap.Reset();
        m_cbvSrvUavHeap.Reset();
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
