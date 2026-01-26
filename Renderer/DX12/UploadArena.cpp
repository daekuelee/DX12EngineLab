#include "UploadArena.h"
#include "DiagnosticLogger.h"
#include <algorithm>

namespace Renderer
{
    void UploadArena::Begin(FrameLinearAllocator* allocator, bool diagEnabled)
    {
        m_allocator = allocator;
        m_diagEnabled = diagEnabled;

        // Reset per-frame metrics
        m_frameMetrics = UploadArenaMetrics{};

        // Capture capacity from allocator
        if (m_allocator)
        {
            m_frameMetrics.capacity = m_allocator->GetCapacity();
        }
    }

    Allocation UploadArena::Allocate(uint64_t size, uint64_t alignment, const char* tag)
    {
        if (!m_allocator)
        {
            return Allocation{}; // Return null allocation if no allocator set
        }

        // Delegate to underlying allocator first
        Allocation result = m_allocator->Allocate(size, alignment, tag);

        // Only update metrics on successful allocation
        if (result.cpuPtr != nullptr)
        {
            m_frameMetrics.allocCalls++;
            m_frameMetrics.allocBytes += size;

            // Update last allocation info using returned result.offset (not re-computed)
            m_frameMetrics.lastAllocTag = tag;
            m_frameMetrics.lastAllocSize = size;
            m_frameMetrics.lastAllocOffset = result.offset;

            // Update peak offset on every successful allocation
            m_frameMetrics.peakOffset = (std::max)(m_frameMetrics.peakOffset, m_allocator->GetOffset());

            // Optional throttled diagnostic logging when diag mode is enabled
            if (m_diagEnabled)
            {
                DiagnosticLogger::LogThrottled("UPLOAD_ARENA",
                    "UploadArena: alloc %s size=%llu align=%llu offset=%llu peak=%llu/%llu\n",
                    tag ? tag : "(null)",
                    static_cast<unsigned long long>(size),
                    static_cast<unsigned long long>(alignment),
                    static_cast<unsigned long long>(result.offset),
                    static_cast<unsigned long long>(m_frameMetrics.peakOffset),
                    static_cast<unsigned long long>(m_frameMetrics.capacity));
            }
        }

        return result;
    }

    void UploadArena::End()
    {
        // Snapshot metrics for HUD (stable read during next frame's render)
        m_lastSnapshot = m_frameMetrics;

        // Reset per-frame metrics for next frame
        m_frameMetrics = UploadArenaMetrics{};
    }
}
