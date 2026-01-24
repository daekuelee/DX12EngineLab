#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace Renderer
{
    class RenderScene
    {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        RenderScene(const RenderScene&) = delete;
        RenderScene& operator=(const RenderScene&) = delete;

        // Initialize geometry (VB/IB in DEFAULT heap)
        bool Initialize(ID3D12Device* device, ID3D12CommandQueue* queue);

        // Shutdown and release resources
        void Shutdown();

        // Record draw commands - instanced mode (1 draw call)
        void RecordDraw(ID3D12GraphicsCommandList* cmdList, uint32_t instanceCount);

        // Record draw commands - naive mode (instanceCount draw calls, 1 instance each)
        void RecordDrawNaive(ID3D12GraphicsCommandList* cmdList, uint32_t instanceCount);

        // Accessors
        uint32_t GetIndexCount() const { return m_indexCount; }
        D3D12_VERTEX_BUFFER_VIEW GetVBView() const { return m_vbv; }
        D3D12_INDEX_BUFFER_VIEW GetIBView() const { return m_ibv; }

    private:
        bool CreateCubeGeometry(ID3D12Device* device, ID3D12CommandQueue* queue);
        bool UploadBuffer(ID3D12Device* device, ID3D12CommandQueue* queue,
                         ID3D12Resource* dstDefault, const void* srcData, uint64_t numBytes,
                         D3D12_RESOURCE_STATES afterState);

        // Cube geometry in DEFAULT heap
        Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vbv = {};
        D3D12_INDEX_BUFFER_VIEW m_ibv = {};
        uint32_t m_indexCount = 0;

        // Temporary resources for upload (fence for synchronization)
        Microsoft::WRL::ComPtr<ID3D12Fence> m_uploadFence;
        HANDLE m_uploadFenceEvent = nullptr;
        uint64_t m_uploadFenceValue = 0;
    };
}
