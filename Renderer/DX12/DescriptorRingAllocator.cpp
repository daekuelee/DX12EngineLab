#include "DescriptorRingAllocator.h"
#include <cstdio>
#include <cassert>

namespace Renderer
{
    bool DescriptorRingAllocator::Initialize(ID3D12Device* device, uint32_t capacity, uint32_t reservedCount)
    {
        if (!device)
        {
            OutputDebugStringA("[DescRing] ERROR: null device\n");
            return false;
        }

        if (reservedCount >= capacity)
        {
            OutputDebugStringA("[DescRing] ERROR: reservedCount must be less than capacity\n");
            return false;
        }

        m_capacity = capacity;
        m_reservedCount = reservedCount;
        m_head = reservedCount;  // Dynamic allocations start after reserved
        m_tail = reservedCount;
        m_usedCount = 0;
        m_currentFrameStart = reservedCount;
        m_currentFrameCount = 0;
        m_frameRecordHead = 0;
        m_frameRecordTail = 0;
        m_frameRecordCount = 0;

        // Create shader-visible descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = capacity;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap));
        if (FAILED(hr))
        {
            char buf[128];
            sprintf_s(buf, "[DescRing] ERROR: CreateDescriptorHeap failed (0x%08X)\n", hr);
            OutputDebugStringA(buf);
            return false;
        }

        m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        char buf[256];
        sprintf_s(buf, "[DescRing] Init: reserved=%u capacity=%u head=%u tail=%u descSize=%u\n",
                  m_reservedCount, m_capacity, m_head, m_tail, m_descriptorSize);
        OutputDebugStringA(buf);

        return true;
    }

    void DescriptorRingAllocator::Shutdown()
    {
        LogStats();
        m_heap.Reset();
        m_descriptorSize = 0;
        m_capacity = 0;
        m_reservedCount = 0;
        m_head = 0;
        m_tail = 0;
        m_usedCount = 0;
        m_currentFrameStart = 0;
        m_currentFrameCount = 0;
        m_frameRecordHead = 0;
        m_frameRecordTail = 0;
        m_frameRecordCount = 0;
        OutputDebugStringA("[DescRing] Shutdown complete\n");
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorRingAllocator::GetReservedCpuHandle(uint32_t slot) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = {};

        if (slot >= m_reservedCount)
        {
            char buf[128];
            sprintf_s(buf, "[DescRing] ERROR: reserved slot %u out of range (max %u)\n",
                      slot, m_reservedCount);
            OutputDebugStringA(buf);
#if defined(_DEBUG)
            __debugbreak();
#endif
            return handle;
        }

        handle = m_heap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += SIZE_T(slot) * m_descriptorSize;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorRingAllocator::GetReservedGpuHandle(uint32_t slot) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = {};

        if (slot >= m_reservedCount)
        {
            char buf[128];
            sprintf_s(buf, "[DescRing] ERROR: reserved slot %u out of range (max %u)\n",
                      slot, m_reservedCount);
            OutputDebugStringA(buf);
#if defined(_DEBUG)
            __debugbreak();
#endif
            return handle;
        }

        handle = m_heap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += SIZE_T(slot) * m_descriptorSize;
        return handle;
    }

    void DescriptorRingAllocator::BeginFrame(uint64_t completedFenceValue)
    {
        // Retire completed frames
        while (m_frameRecordCount > 0)
        {
            FrameRecord& rec = m_frameRecords[m_frameRecordTail];
            if (rec.fenceValue > completedFenceValue)
                break;

            // === RETIREMENT CONTRACT: frame must start at current tail ===
            if (rec.startIndex != m_tail)
            {
                char buf[256];
                sprintf_s(buf, "[DescRing] RETIRE CONTRACT VIOLATION! rec.start=%u != tail=%u "
                          "fence=%llu count=%u head=%u\n",
                          rec.startIndex, m_tail, rec.fenceValue, rec.count, m_head);
                OutputDebugStringA(buf);
#if defined(_DEBUG)
                __debugbreak();
#endif
            }

            // Retire this frame's slots (advance tail, wrapping as needed)
            uint32_t toRetire = rec.count;
            while (toRetire > 0)
            {
                uint32_t slotsToEnd = m_capacity - m_tail;
                uint32_t retireNow = (toRetire < slotsToEnd) ? toRetire : slotsToEnd;
                m_tail += retireNow;
                if (m_tail >= m_capacity)
                    m_tail = m_reservedCount;
                toRetire -= retireNow;
            }

            m_usedCount -= rec.count;
            m_frameRecordTail = (m_frameRecordTail + 1) % MaxFrameRecords;
            m_frameRecordCount--;

            char buf[128];
            sprintf_s(buf, "[DescRing] Retired fence=%llu start=%u count=%u tail=%u used=%u\n",
                      rec.fenceValue, rec.startIndex, rec.count, m_tail, m_usedCount);
            OutputDebugStringA(buf);
        }

        // Reset frame accumulator - record current head as this frame's start
        m_currentFrameStart = m_head;
        m_currentFrameCount = 0;
    }

    DescriptorAllocation DescriptorRingAllocator::Allocate(uint32_t count, const char* tag)
    {
        if (!m_heap || count == 0)
            return {};

        const uint32_t dynamicCapacity = m_capacity - m_reservedCount;

        // === SAFETY GUARD 1: Single allocation cannot exceed total dynamic capacity ===
        if (count > dynamicCapacity)
        {
            char buf[256];
            sprintf_s(buf, "[DescRing] OOM! count=%u exceeds dynamicCapacity=%u tag=%s\n",
                      count, dynamicCapacity, tag ? tag : "?");
            OutputDebugStringA(buf);
#if defined(_DEBUG)
            __debugbreak();
#endif
            return {};
        }

        // === SAFETY GUARD 2: Total used + request cannot exceed dynamic capacity ===
        if (m_usedCount + count > dynamicCapacity)
        {
            char buf[256];
            sprintf_s(buf, "[DescRing] OOM! used=%u + count=%u > dynamicCapacity=%u tag=%s\n",
                      m_usedCount, count, dynamicCapacity, tag ? tag : "?");
            OutputDebugStringA(buf);
#if defined(_DEBUG)
            __debugbreak();
#endif
            return {};
        }

        // Calculate contiguous free space based on head/tail positions
        uint32_t contiguousFree = 0;
        if (m_head >= m_tail)
        {
            // Normal case: free space is from head to capacity end
            contiguousFree = m_capacity - m_head;
        }
        else
        {
            // Wrapped case: free space is from head to tail
            contiguousFree = m_tail - m_head;
        }

        // Debug log state (throttled)
        static uint32_t s_allocCount = 0;
        ++s_allocCount;
        if (count > 1 || (s_allocCount % 60) == 1)
        {
            char stateBuf[256];
            sprintf_s(stateBuf, "[DescRing] Alloc: head=%u tail=%u used=%u contiguousFree=%u req=%u tag=%s\n",
                      m_head, m_tail, m_usedCount, contiguousFree, count, tag ? tag : "?");
            OutputDebugStringA(stateBuf);
        }

        // Try to allocate in current contiguous region
        if (count <= contiguousFree)
        {
            return AllocateContiguous(count, tag);
        }

        // Need to wrap - only valid if head >= tail (space exists at front)
        if (m_head < m_tail)
        {
            // head < tail means no wrap possible - OOM
            char buf[256];
            sprintf_s(buf, "[DescRing] OOM! head<tail, no wrap. tag=%s head=%u tail=%u req=%u\n",
                      tag ? tag : "?", m_head, m_tail, count);
            OutputDebugStringA(buf);
#if defined(_DEBUG)
            __debugbreak();
#endif
            return {};
        }

        // head >= tail: compute waste and check front space
        uint32_t wastedSlots = m_capacity - m_head;
        uint32_t freeAtFront = m_tail - m_reservedCount;

        if (count > freeAtFront)
        {
            char buf[256];
            sprintf_s(buf, "[DescRing] OOM after wrap! tag=%s wasted=%u freeAtFront=%u req=%u\n",
                      tag ? tag : "?", wastedSlots, freeAtFront, count);
            OutputDebugStringA(buf);
#if defined(_DEBUG)
            __debugbreak();
#endif
            return {};
        }

        // Waste end slots (count them for THIS frame)
        m_usedCount += wastedSlots;
        m_currentFrameCount += wastedSlots;

        char wrapBuf[128];
        sprintf_s(wrapBuf, "[DescRing] Wrap: wasting %u slots, head=%u->%u\n",
                  wastedSlots, m_head, m_reservedCount);
        OutputDebugStringA(wrapBuf);

        // Reset head to start of dynamic region
        m_head = m_reservedCount;

        return AllocateContiguous(count, tag);
    }

    // Private helper: allocate from current head (already validated)
    DescriptorAllocation DescriptorRingAllocator::AllocateContiguous(uint32_t count, const char* tag)
    {
        DescriptorAllocation alloc;
        alloc.heapIndex = m_head;
        alloc.count = count;
        alloc.cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
        alloc.cpuHandle.ptr += SIZE_T(m_head) * m_descriptorSize;
        alloc.gpuHandle = m_heap->GetGPUDescriptorHandleForHeapStart();
        alloc.gpuHandle.ptr += SIZE_T(m_head) * m_descriptorSize;
        alloc.valid = true;

        m_head += count;
        m_usedCount += count;
        m_currentFrameCount += count;

        return alloc;
    }

    void DescriptorRingAllocator::EndFrame(uint64_t signaledFenceValue)
    {
        if (m_currentFrameCount == 0)
            return;

        if (m_frameRecordCount >= MaxFrameRecords)
        {
            OutputDebugStringA("[DescRing] Frame record overflow!\n");
#if defined(_DEBUG)
            __debugbreak();
#endif
            return;
        }

        FrameRecord& rec = m_frameRecords[m_frameRecordHead];
        rec.fenceValue = signaledFenceValue;
        rec.startIndex = m_currentFrameStart;
        rec.count = m_currentFrameCount;
        m_frameRecordHead = (m_frameRecordHead + 1) % MaxFrameRecords;
        m_frameRecordCount++;

        char buf[128];
        sprintf_s(buf, "[DescRing] EndFrame: fence=%llu start=%u count=%u records=%u\n",
                  signaledFenceValue, m_currentFrameStart, m_currentFrameCount, m_frameRecordCount);
        OutputDebugStringA(buf);
    }

    void DescriptorRingAllocator::LogStats() const
    {
        char buf[256];
        sprintf_s(buf, "[DescRing] Stats: capacity=%u reserved=%u dynamic_used=%u head=%u tail=%u pending_frames=%u\n",
                  m_capacity, m_reservedCount, m_usedCount, m_head, m_tail, m_frameRecordCount);
        OutputDebugStringA(buf);
    }
}
