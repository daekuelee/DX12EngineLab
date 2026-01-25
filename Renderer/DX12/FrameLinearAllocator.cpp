#include "FrameLinearAllocator.h"
#include <cstdio>

namespace Renderer
{
    bool FrameLinearAllocator::Initialize(ID3D12Device* device, uint64_t capacity)
    {
        if (!device || capacity == 0)
            return false;

        m_capacity = capacity;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = capacity;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_uploadBuffer));

        if (FAILED(hr))
            return false;

        // Persistently map the buffer
        D3D12_RANGE readRange = { 0, 0 };
        hr = m_uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cpuBasePtr));
        if (FAILED(hr))
            return false;

        m_gpuBaseVA = m_uploadBuffer->GetGPUVirtualAddress();
        m_offset = 0;

        return true;
    }

    void FrameLinearAllocator::Reset()
    {
        // Log offset before reset for diagnostics
        char buf[128];
        sprintf_s(buf, "FrameLinearAllocator::Reset offset=%llu\n", m_offset);
        OutputDebugStringA(buf);

        m_offset = 0;
    }

    Allocation FrameLinearAllocator::Allocate(uint64_t size, uint64_t alignment)
    {
        // Align offset
        uint64_t alignedOffset = (m_offset + alignment - 1) & ~(alignment - 1);

        // Check for overflow
        if (alignedOffset + size > m_capacity)
        {
            OutputDebugStringA("FrameLinearAllocator::Allocate - out of memory!\n");
            return {};
        }

        Allocation alloc;
        alloc.cpuPtr = m_cpuBasePtr + alignedOffset;
        alloc.gpuVA = m_gpuBaseVA + alignedOffset;
        alloc.offset = alignedOffset;

        m_offset = alignedOffset + size;

        return alloc;
    }

    void FrameLinearAllocator::Shutdown()
    {
        if (m_uploadBuffer && m_cpuBasePtr)
        {
            m_uploadBuffer->Unmap(0, nullptr);
            m_cpuBasePtr = nullptr;
        }

        m_uploadBuffer.Reset();
        m_gpuBaseVA = 0;
        m_offset = 0;
        m_capacity = 0;
    }
}
