#pragma once

#include <d3d12.h>

namespace Renderer
{
    // RAII helper for symmetric resource state transitions
    // Ensures cleanup barrier is emitted even on early return/exception
    class BarrierScope
    {
    public:
        // Begin transition from initialState to targetState
        BarrierScope(ID3D12GraphicsCommandList* cmd,
                     ID3D12Resource* resource,
                     D3D12_RESOURCE_STATES initialState,
                     D3D12_RESOURCE_STATES targetState)
            : m_cmd(cmd)
            , m_resource(resource)
            , m_initialState(initialState)
            , m_targetState(targetState)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = m_initialState;
            barrier.Transition.StateAfter = m_targetState;
            m_cmd->ResourceBarrier(1, &barrier);
        }

        // Non-copyable, non-movable
        BarrierScope(const BarrierScope&) = delete;
        BarrierScope& operator=(const BarrierScope&) = delete;
        BarrierScope(BarrierScope&&) = delete;
        BarrierScope& operator=(BarrierScope&&) = delete;

        // Restore to initial state
        ~BarrierScope()
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = m_targetState;
            barrier.Transition.StateAfter = m_initialState;
            m_cmd->ResourceBarrier(1, &barrier);
        }

    private:
        ID3D12GraphicsCommandList* m_cmd;
        ID3D12Resource* m_resource;
        D3D12_RESOURCE_STATES m_initialState;
        D3D12_RESOURCE_STATES m_targetState;
    };

    // Convenience wrapper for backbuffer PRESENT <-> RENDER_TARGET transitions
    class BackbufferScope : public BarrierScope
    {
    public:
        BackbufferScope(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backbuffer)
            : BarrierScope(cmd, backbuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
        }
    };

} // namespace Renderer
