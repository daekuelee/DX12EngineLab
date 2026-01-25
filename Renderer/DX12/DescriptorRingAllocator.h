#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace Renderer
{
    //-------------------------------------------------------------------------
    // DescriptorAllocation: Result of a ring allocation
    //-------------------------------------------------------------------------
    struct DescriptorAllocation
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
        uint32_t heapIndex = UINT32_MAX;
        uint32_t count = 0;
        bool valid = false;

        bool IsValid() const { return valid; }
    };

    //-------------------------------------------------------------------------
    // DescriptorRingAllocator: Fence-protected descriptor ring
    //
    // This is THE shader-visible CBV/SRV/UAV heap (DX12 allows only one bound
    // at a time). It provides:
    //   - Reserved slots at front for static/per-frame resources (transforms SRVs)
    //   - Dynamic ring allocation for transient descriptors
    //   - Fence-based retirement of completed frames
    //
    // Heap Layout:
    //   [Reserved: 0..reservedCount-1] [Dynamic Ring: reservedCount..capacity-1]
    //
    // Critical constraint: An allocation MUST NOT cross the heap end. If it would
    // wrap, the remaining slots are wasted until retired.
    //
    // Usage:
    //   1. Initialize() with device, capacity, and reserved count
    //   2. Use GetReservedCpuHandle/GetReservedGpuHandle for reserved slots
    //   3. Call BeginFrame() at frame start after fence wait
    //   4. Call Allocate() for dynamic descriptors during frame
    //   5. Call EndFrame() after queue signal
    //   6. Shutdown() to release heap
    //-------------------------------------------------------------------------
    class DescriptorRingAllocator
    {
    public:
        DescriptorRingAllocator() = default;
        ~DescriptorRingAllocator() = default;

        // Non-copyable
        DescriptorRingAllocator(const DescriptorRingAllocator&) = delete;
        DescriptorRingAllocator& operator=(const DescriptorRingAllocator&) = delete;

        // Initialize heap with reserved slots at front
        // reservedCount: slots 0..reservedCount-1 are static, not ring-managed
        bool Initialize(ID3D12Device* device, uint32_t capacity = 1024, uint32_t reservedCount = 3);

        // Shutdown and release heap
        void Shutdown();

        // === Reserved Slot Access ===
        // Get handle for reserved slot (0..reservedCount-1). Does NOT consume ring space.
        D3D12_CPU_DESCRIPTOR_HANDLE GetReservedCpuHandle(uint32_t slot) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetReservedGpuHandle(uint32_t slot) const;

        // === Frame Lifecycle ===

        // Call at frame start after fence wait. Retires completed frames.
        void BeginFrame(uint64_t completedFenceValue);

        // Allocate contiguous descriptors. Never wraps - wastes end-of-heap if needed.
        // Hard-fails if not enough space.
        DescriptorAllocation Allocate(uint32_t count, const char* tag = nullptr);

        // Call at frame end. Attaches fence to this frame's allocations.
        void EndFrame(uint64_t signaledFenceValue);

        // === Accessors ===
        ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
        uint32_t GetDescriptorSize() const { return m_descriptorSize; }
        uint32_t GetCapacity() const { return m_capacity; }
        uint32_t GetReservedCount() const { return m_reservedCount; }
        uint32_t GetDynamicUsed() const { return m_usedCount; }

        // Debug
        void LogStats() const;

    private:
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
        uint32_t m_descriptorSize = 0;
        uint32_t m_capacity = 0;        // Total heap size
        uint32_t m_reservedCount = 0;   // First N slots are reserved

        // Ring state (operates in range [m_reservedCount, m_capacity))
        uint32_t m_head = 0;            // Next alloc position (>= reservedCount)
        uint32_t m_tail = 0;            // Oldest pending (>= reservedCount)
        uint32_t m_usedCount = 0;       // Dynamic slots in use

        // Per-frame tracking
        struct FrameRecord
        {
            uint64_t fenceValue = 0;
            uint32_t startIndex = 0;    // First slot of this frame's allocations
            uint32_t count = 0;         // Total slots used (including wasted)
        };
        static constexpr uint32_t MaxFrameRecords = 8;
        FrameRecord m_frameRecords[MaxFrameRecords] = {};
        uint32_t m_frameRecordHead = 0;
        uint32_t m_frameRecordTail = 0;
        uint32_t m_frameRecordCount = 0;

        // Current frame accumulator
        uint32_t m_currentFrameStart = 0;  // Set in BeginFrame
        uint32_t m_currentFrameCount = 0;

        // Private helper: allocate from current head (already validated)
        DescriptorAllocation AllocateContiguous(uint32_t count, const char* tag);
    };
}
