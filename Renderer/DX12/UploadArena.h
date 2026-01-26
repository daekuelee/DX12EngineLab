#pragma once

#include "FrameLinearAllocator.h"
#include <cstdint>

namespace Renderer
{
    struct UploadArenaMetrics
    {
        uint32_t allocCalls = 0;      // Allocation calls this frame
        uint64_t allocBytes = 0;      // Bytes allocated this frame
        uint64_t peakOffset = 0;      // High-water offset this frame
        uint64_t capacity = 0;        // Allocator capacity

        // Last allocation info (for detailed diag display)
        const char* lastAllocTag = nullptr;
        uint64_t lastAllocSize = 0;
        uint64_t lastAllocOffset = 0;
    };

    class UploadArena
    {
    public:
        // Begin frame - set active allocator, enable diag logging
        void Begin(FrameLinearAllocator* allocator, bool diagEnabled);

        // Main allocation entry point (passthrough + metrics)
        Allocation Allocate(uint64_t size, uint64_t alignment = 256, const char* tag = nullptr);

        // End frame - snapshot metrics for HUD, reset per-frame counters
        void End();

        // Accessors for HUD (reads last-frame snapshot, stable during render)
        const UploadArenaMetrics& GetLastSnapshot() const { return m_lastSnapshot; }

        // Truthful map calls: always 1 (persistent map)
        static constexpr uint32_t GetMapCalls() { return 1; }

    private:
        FrameLinearAllocator* m_allocator = nullptr;
        bool m_diagEnabled = false;

        // Per-frame metrics (mutated during frame)
        UploadArenaMetrics m_frameMetrics;

        // Last-frame snapshot (stable for HUD reads)
        UploadArenaMetrics m_lastSnapshot;
    };
}
