#include "FrameContextRing.h"
#include "RenderConfig.h"

namespace Renderer
{
    // CBV requires 256-byte alignment
    static constexpr uint64_t CBV_ALIGNMENT = 256;
    static constexpr uint64_t CB_SIZE = (sizeof(float) * 16 + CBV_ALIGNMENT - 1) & ~(CBV_ALIGNMENT - 1);

    // Transforms: (10k + extras) float4x4 = (10k + 32) * 64 bytes
    static constexpr uint64_t TRANSFORMS_SIZE = (InstanceCount + MaxExtraInstances) * sizeof(float) * 16;

    bool FrameContextRing::Initialize(ID3D12Device* device, DescriptorRingAllocator* descRing, ResourceRegistry* registry)
    {
        if (!device || !descRing || !registry)
            return false;

        m_device = device;
        m_descRing = descRing;
        m_registry = registry;

        // Create fence
        HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        if (FAILED(hr))
            return false;

        m_fenceCounter = 0;

        // Create fence event
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!m_fenceEvent)
            return false;

        // Create per-frame resources
        for (uint32_t i = 0; i < FrameCount; ++i)
        {
            if (!CreatePerFrameBuffers(device, i))
                return false;

            // Assign per-frame SRV slot (slot = frameIndex)
            m_frames[i].srvSlot = i;

            // Create SRV for this frame's transforms buffer
            CreateSRV(device, i);
        }

        return true;
    }

    bool FrameContextRing::CreatePerFrameBuffers(ID3D12Device* device, uint32_t frameIndex)
    {
        FrameContext& ctx = m_frames[frameIndex];

        // Command allocator
        HRESULT hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&ctx.cmdAllocator));
        if (FAILED(hr))
            return false;

        // Per-frame linear allocator for upload heap (1MB capacity)
        // CB_SIZE (256) + TRANSFORMS_SIZE (640KB) = ~640KB, 1MB gives headroom
        static constexpr uint64_t ALLOCATOR_CAPACITY = 1 * 1024 * 1024; // 1MB
        if (!ctx.uploadAllocator.Initialize(device, ALLOCATOR_CAPACITY))
            return false;

        // Transforms default buffer via ResourceRegistry (starts in COPY_DEST)
        char debugName[32];
        sprintf_s(debugName, "TransformsDefault[%u]", frameIndex);
        ResourceDesc transformsDesc = ResourceDesc::Buffer(
            TRANSFORMS_SIZE,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_COPY_DEST,
            debugName);
        ctx.transformsHandle = m_registry->Create(transformsDesc);
        if (!ctx.transformsHandle.IsValid())
            return false;

        return true;
    }

    void FrameContextRing::CreateSRV(ID3D12Device* device, uint32_t frameIndex)
    {
        FrameContext& ctx = m_frames[frameIndex];

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;

#if MICROTEST_MODE
        // Raw buffer SRV for ByteAddressBuffer (diagnostic mode)
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.Buffer.NumElements = InstanceCount * 16;  // 16 floats per matrix
        srvDesc.Buffer.StructureByteStride = 0;           // Must be 0 for raw
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
#else
        // StructuredBuffer SRV for production (float4x4 per element)
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.NumElements = InstanceCount + MaxExtraInstances;  // Day3.12 Phase 4B+: Include extras
        srvDesc.Buffer.StructureByteStride = sizeof(float) * 16;  // 64 bytes per matrix
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
#endif

        // Get CPU handle at this frame's reserved SRV slot
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_descRing->GetReservedCpuHandle(ctx.srvSlot);

        // Get raw resource from registry
        ID3D12Resource* transformsResource = m_registry->Get(ctx.transformsHandle);
        device->CreateShaderResourceView(transformsResource, &srvDesc, cpuHandle);

    }

    D3D12_GPU_DESCRIPTOR_HANDLE FrameContextRing::GetSrvGpuHandle(uint32_t frameIndex) const
    {
        return m_descRing->GetReservedGpuHandle(m_frames[frameIndex].srvSlot);
    }

    void FrameContextRing::Shutdown()
    {
        WaitForAll();

        if (m_fenceEvent)
        {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }

        for (uint32_t i = 0; i < FrameCount; ++i)
        {
            FrameContext& ctx = m_frames[i];

            ctx.uploadAllocator.Shutdown();
            // Destroy transforms via registry
            if (m_registry && ctx.transformsHandle.IsValid())
            {
                m_registry->Destroy(ctx.transformsHandle);
                ctx.transformsHandle = ResourceHandle{};
            }
            ctx.cmdAllocator.Reset();
            ctx.fenceValue = 0;
        }

        m_fence.Reset();
        m_device = nullptr;
        m_descRing = nullptr;
        m_registry = nullptr;
    }

    FrameContext& FrameContextRing::BeginFrame(uint64_t frameId)
    {
        // Select frame context using monotonic frameId, NOT backbuffer index
        uint32_t frameIndex = static_cast<uint32_t>(frameId % FrameCount);
        FrameContext& ctx = m_frames[frameIndex];

        // Wait for this context to be available (fence-gated reuse)
        if (ctx.fenceValue != 0)
        {
            WaitForFence(ctx.fenceValue);
        }

        // Reset command allocator now that GPU is done with it
        ctx.cmdAllocator->Reset();

        // Reset linear allocator for upload heap (logs offset for diagnostics)
        ctx.uploadAllocator.Reset();

        return ctx;
    }

    void FrameContextRing::EndFrame(ID3D12CommandQueue* queue, FrameContext& ctx)
    {
        // Increment fence counter and signal
        m_fenceCounter++;
        HRESULT hr = queue->Signal(m_fence.Get(), m_fenceCounter);
        if (FAILED(hr))
        {
            OutputDebugStringA("FrameContextRing::EndFrame - Signal failed\n");
        }

        // Record fence value for this context
        ctx.fenceValue = m_fenceCounter;
    }

    void FrameContextRing::WaitForFence(uint64_t value)
    {
        if (m_fence->GetCompletedValue() >= value)
            return;

        m_fence->SetEventOnCompletion(value, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    void FrameContextRing::WaitForAll()
    {
        if (m_fenceCounter > 0)
        {
            WaitForFence(m_fenceCounter);
        }
    }
}
