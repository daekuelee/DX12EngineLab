#include "ResourceStateTracker.h"
#include <cstdio>

namespace Renderer
{
    void ResourceStateTracker::Register(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, const char* debugName)
    {
        if (!resource)
            return;

        TrackedResource tracked;
        tracked.currentState = initialState;
        tracked.debugName = debugName;
        m_trackedResources[resource] = tracked;

        if (m_diagnosticsEnabled && debugName)
        {
            char buf[256];
            sprintf_s(buf, "StateTracker: Register %s (0x%p) state=0x%X\n",
                debugName, resource, initialState);
            OutputDebugStringA(buf);
        }
    }

    void ResourceStateTracker::AssumeState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state)
    {
        if (!resource)
            return;

        auto it = m_trackedResources.find(resource);
        if (it != m_trackedResources.end())
        {
            it->second.currentState = state;
        }
        else
        {
            TrackedResource tracked;
            tracked.currentState = state;
            tracked.debugName = nullptr;
            m_trackedResources[resource] = tracked;
        }
    }

    void ResourceStateTracker::Unregister(ID3D12Resource* resource)
    {
        if (!resource)
            return;

        auto it = m_trackedResources.find(resource);
        if (it != m_trackedResources.end())
        {
            if (m_diagnosticsEnabled && it->second.debugName)
            {
                char buf[256];
                sprintf_s(buf, "StateTracker: Unregister %s (0x%p)\n",
                    it->second.debugName, resource);
                OutputDebugStringA(buf);
            }
            m_trackedResources.erase(it);
        }
    }

    void ResourceStateTracker::Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES targetState)
    {
        if (!resource)
            return;

        auto it = m_trackedResources.find(resource);
        if (it == m_trackedResources.end())
        {
            // Resource not tracked - hard fail in debug
#if defined(_DEBUG)
            OutputDebugStringA("StateTracker ERROR: Transition called on untracked resource!\n");
            __debugbreak();
#endif
            return;
        }

        D3D12_RESOURCE_STATES currentState = it->second.currentState;

        // Skip redundant transitions
        if (currentState == targetState)
            return;

        // Queue barrier
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = currentState;
        barrier.Transition.StateAfter = targetState;
        m_pendingBarriers.push_back(barrier);

        // Update tracked state
        it->second.currentState = targetState;

        if (m_diagnosticsEnabled)
        {
            const char* name = it->second.debugName ? it->second.debugName : "unnamed";
            char buf[256];
            sprintf_s(buf, "StateTracker: Transition %s 0x%X -> 0x%X\n",
                name, currentState, targetState);
            OutputDebugStringA(buf);
        }
    }

    void ResourceStateTracker::UAVBarrier(ID3D12Resource* resource)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = resource;
        m_pendingBarriers.push_back(barrier);
    }

    void ResourceStateTracker::FlushBarriers(ID3D12GraphicsCommandList* cmdList)
    {
        if (m_pendingBarriers.empty())
            return;

        cmdList->ResourceBarrier(
            static_cast<UINT>(m_pendingBarriers.size()),
            m_pendingBarriers.data());

        m_pendingBarriers.clear();
    }

    D3D12_RESOURCE_STATES ResourceStateTracker::GetState(ID3D12Resource* resource) const
    {
        auto it = m_trackedResources.find(resource);
        if (it != m_trackedResources.end())
            return it->second.currentState;

        // Not tracked - return COMMON as default
        return D3D12_RESOURCE_STATE_COMMON;
    }

    bool ResourceStateTracker::IsTracked(ID3D12Resource* resource) const
    {
        return m_trackedResources.find(resource) != m_trackedResources.end();
    }

} // namespace Renderer
