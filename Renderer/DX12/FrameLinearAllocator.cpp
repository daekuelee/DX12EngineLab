#include "FrameLinearAllocator.h"
#include <cstdio>
#include <cassert>

namespace Renderer
{
    // Helper: check if value is power-of-two (and non-zero)
    static inline bool IsPowerOfTwo(uint64_t x) { return x != 0 && (x & (x - 1)) == 0; }
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

    Allocation FrameLinearAllocator::Allocate(uint64_t size, uint64_t alignment, const char* tag)
    {
        // Alignment validation: must be non-zero and power-of-two
        assert(IsPowerOfTwo(alignment) && "Alignment must be non-zero and power-of-two");

        // Align offset
        uint64_t alignedOffset = (m_offset + alignment - 1) & ~(alignment - 1);

        // Check for overflow - HARD FAIL, do not silently continue
        if (alignedOffset + size > m_capacity)
        {
            char errBuf[256];
            sprintf_s(errBuf, "FrameLinearAllocator::Allocate OOM! tag=%s offset=%llu size=%llu cap=%llu\n",
                      tag ? tag : "?", alignedOffset, size, m_capacity);
            OutputDebugStringA(errBuf);

#if defined(_DEBUG)
            __debugbreak();  // Hard fail in Debug - catch this immediately
#endif
            // In Release: return invalid allocation that will cause obvious failure
            // (null cpuPtr will crash on write, which is better than silent corruption)
            return {};
        }

        Allocation alloc;
        alloc.cpuPtr = m_cpuBasePtr + alignedOffset;
        alloc.gpuVA = m_gpuBaseVA + alignedOffset;
        alloc.offset = alignedOffset;

        m_offset = alignedOffset + size;

        // Optional allocation logging for proof/debugging
        if (tag)
        {
            char logBuf[128];
            sprintf_s(logBuf, "ALLOC: %s offset=%llu size=%llu\n", tag, alignedOffset, size);
            OutputDebugStringA(logBuf);
        }

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
