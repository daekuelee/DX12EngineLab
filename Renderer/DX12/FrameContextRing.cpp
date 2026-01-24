#include "FrameContextRing.h"
#include <cstdio>

namespace Renderer
{
    // CBV requires 256-byte alignment
    static constexpr uint64_t CBV_ALIGNMENT = 256;
    static constexpr uint64_t CB_SIZE = (sizeof(float) * 16 + CBV_ALIGNMENT - 1) & ~(CBV_ALIGNMENT - 1);

    // Transforms: 10k float4x4 = 10k * 64 bytes
    static constexpr uint64_t TRANSFORMS_SIZE = InstanceCount * sizeof(float) * 16;

    bool FrameContextRing::Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, uint32_t srvHeapIncrementSize)
    {
        if (!device || !srvHeap)
            return false;

        m_device = device;
        m_srvHeap = srvHeap;
        m_srvIncrementSize = srvHeapIncrementSize;

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
            IID_PPV_ARGS(&ctx.allocator));
        if (FAILED(hr))
            return false;

        D3D12_HEAP_PROPERTIES uploadHeapProps = {};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_HEAP_PROPERTIES defaultHeapProps = {};
        defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        // Frame CB (upload heap)
        {
            bufferDesc.Width = CB_SIZE;
            hr = device->CreateCommittedResource(
                &uploadHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&ctx.frameCB));
            if (FAILED(hr))
                return false;

            D3D12_RANGE readRange = { 0, 0 };
            hr = ctx.frameCB->Map(0, &readRange, &ctx.frameCBMapped);
            if (FAILED(hr))
                return false;

            ctx.frameCBGpuVA = ctx.frameCB->GetGPUVirtualAddress();
        }

        // Transforms upload buffer
        {
            bufferDesc.Width = TRANSFORMS_SIZE;
            hr = device->CreateCommittedResource(
                &uploadHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&ctx.transformsUpload));
            if (FAILED(hr))
                return false;

            D3D12_RANGE readRange = { 0, 0 };
            hr = ctx.transformsUpload->Map(0, &readRange, &ctx.transformsUploadMapped);
            if (FAILED(hr))
                return false;
        }

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
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = InstanceCount;
        srvDesc.Buffer.StructureByteStride = sizeof(float) * 16; // float4x4 = 64 bytes
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        // Get CPU handle at this frame's SRV slot
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        cpuHandle.ptr += static_cast<SIZE_T>(ctx.srvSlot) * m_srvIncrementSize;

        device->CreateShaderResourceView(ctx.transformsDefault.Get(), &srvDesc, cpuHandle);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE FrameContextRing::GetSrvGpuHandle(uint32_t frameIndex) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += static_cast<SIZE_T>(m_frames[frameIndex].srvSlot) * m_srvIncrementSize;
        return gpuHandle;
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

            if (ctx.frameCB && ctx.frameCBMapped)
                ctx.frameCB->Unmap(0, nullptr);
            if (ctx.transformsUpload && ctx.transformsUploadMapped)
                ctx.transformsUpload->Unmap(0, nullptr);

            ctx.frameCB.Reset();
            ctx.transformsUpload.Reset();
            ctx.transformsDefault.Reset();
            ctx.allocator.Reset();
            ctx.fenceValue = 0;
            ctx.frameCBMapped = nullptr;
            ctx.transformsUploadMapped = nullptr;
        }

        m_fence.Reset();
        m_device = nullptr;
        m_srvHeap = nullptr;
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

        // Reset allocator now that GPU is done with it
        ctx.allocator->Reset();

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
