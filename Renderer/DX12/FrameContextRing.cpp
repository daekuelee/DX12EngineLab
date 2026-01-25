#include "FrameContextRing.h"
#include "RenderConfig.h"
#include <cstdio>

namespace Renderer
{
    // CBV requires 256-byte alignment
    static constexpr uint64_t CBV_ALIGNMENT = 256;
    static constexpr uint64_t CB_SIZE = (sizeof(float) * 16 + CBV_ALIGNMENT - 1) & ~(CBV_ALIGNMENT - 1);

    // Transforms: 10k float4x4 = 10k * 64 bytes
    static constexpr uint64_t TRANSFORMS_SIZE = InstanceCount * sizeof(float) * 16;

    bool FrameContextRing::Initialize(ID3D12Device* device, DescriptorRingAllocator* descRing)
    {
        if (!device || !descRing)
            return false;

        m_device = device;
        m_descRing = descRing;

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

        // B-1: One-time SRV heap diagnostic log
        OutputDebugStringA("=== SRV HEAP INIT (DescRing) ===\n");
        {
            char buf[256];
            sprintf_s(buf, "descRing capacity=%u reserved=%u descSize=%u\n",
                      m_descRing->GetCapacity(), m_descRing->GetReservedCount(),
                      m_descRing->GetDescriptorSize());
            OutputDebugStringA(buf);

            D3D12_GPU_DESCRIPTOR_HANDLE heapGpuStart = m_descRing->GetHeap()->GetGPUDescriptorHandleForHeapStart();
            sprintf_s(buf, "heapGpuStart=0x%llX\n", heapGpuStart.ptr);
            OutputDebugStringA(buf);

            for (uint32_t i = 0; i < FrameCount; ++i)
            {
                D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_descRing->GetReservedCpuHandle(m_frames[i].srvSlot);
                D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_descRing->GetReservedGpuHandle(m_frames[i].srvSlot);
                sprintf_s(buf, "frame[%u] srvSlot=%u CPU=0x%llX GPU=0x%llX\n",
                          i, m_frames[i].srvSlot, cpu.ptr, gpu.ptr);
                OutputDebugStringA(buf);
            }
        }
        OutputDebugStringA("================================\n");

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

        D3D12_HEAP_PROPERTIES defaultHeapProps = {};
        defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        // Transforms default buffer (starts in COPY_DEST for first frame)
        {
            bufferDesc.Width = TRANSFORMS_SIZE;
            hr = device->CreateCommittedResource(
                &defaultHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_COPY_DEST, // D3: Start in COPY_DEST, skip first barrier
                nullptr,
                IID_PPV_ARGS(&ctx.transformsDefault));
            if (FAILED(hr))
                return false;
        }

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
        srvDesc.Buffer.NumElements = InstanceCount;
        srvDesc.Buffer.StructureByteStride = sizeof(float) * 16;  // 64 bytes per matrix
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
#endif

        // Get CPU handle at this frame's reserved SRV slot
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_descRing->GetReservedCpuHandle(ctx.srvSlot);

        device->CreateShaderResourceView(ctx.transformsDefault.Get(), &srvDesc, cpuHandle);
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
            ctx.transformsDefault.Reset();
            ctx.cmdAllocator.Reset();
            ctx.fenceValue = 0;
        }

        m_fence.Reset();
        m_device = nullptr;
        m_descRing = nullptr;
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
