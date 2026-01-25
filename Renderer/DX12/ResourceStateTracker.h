#pragma once

#include <d3d12.h>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <Windows.h>

namespace Renderer
{
    // ResourceStateTracker: SOLE authority for resource state tracking
    // All barrier emission should go through this tracker
    //
    // Scope: Whole-resource state tracking only (no per-subresource tracking)
    // Note: UAVBarrier is NOT a state transition - it's an ordering constraint
    class ResourceStateTracker
    {
    public:
        ResourceStateTracker() = default;
        ~ResourceStateTracker() = default;

        // Non-copyable
        ResourceStateTracker(const ResourceStateTracker&) = delete;
        ResourceStateTracker& operator=(const ResourceStateTracker&) = delete;

        // Registration - for owned resources with known initial state
        void Register(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, const char* debugName = nullptr);

        // AssumeState - for external resources (swapchain backbuffers)
        // Does not create ownership, just tracks state
        void AssumeState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);

        // Unregister - remove resource from tracking
        void Unregister(ID3D12Resource* resource);

        // Transition - queue a state transition (batched)
        void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES targetState);

        // UAVBarrier - for compute synchronization
        void UAVBarrier(ID3D12Resource* resource);

        // FlushBarriers - emit all queued barriers
        void FlushBarriers(ID3D12GraphicsCommandList* cmdList);

        // Query current tracked state
        D3D12_RESOURCE_STATES GetState(ID3D12Resource* resource) const;

        // Check if resource is tracked
        bool IsTracked(ID3D12Resource* resource) const;

        // Debug: enable/disable transition logging
        void SetDiagnosticsEnabled(bool enabled) { m_diagnosticsEnabled = enabled; }

    private:
        struct TrackedResource
        {
            D3D12_RESOURCE_STATES currentState;
            const char* debugName;
        };

        std::unordered_map<ID3D12Resource*, TrackedResource> m_trackedResources;
        std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers;
        bool m_diagnosticsEnabled = false;
    };

} // namespace Renderer
