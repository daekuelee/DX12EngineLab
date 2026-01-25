#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace Renderer
{
    // Result of buffer creation - contains resource and view
    struct VertexBufferResult
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_VERTEX_BUFFER_VIEW view = {};
    };

    struct IndexBufferResult
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_INDEX_BUFFER_VIEW view = {};
    };

    // Factory for creating geometry buffers with upload synchronization
    // Consolidates duplicated buffer creation logic from RenderScene
    class GeometryFactory
    {
    public:
        GeometryFactory() = default;
        ~GeometryFactory() = default;

        GeometryFactory(const GeometryFactory&) = delete;
        GeometryFactory& operator=(const GeometryFactory&) = delete;

        // Initialize factory with device and queue for uploads
        bool Initialize(ID3D12Device* device, ID3D12CommandQueue* queue);

        // Shutdown and release upload resources
        void Shutdown();

        // Create vertex buffer in DEFAULT heap with synchronized upload
        // Returns populated result with resource and view, or empty on failure
        VertexBufferResult CreateVertexBuffer(const void* data, uint64_t sizeBytes, uint32_t strideBytes);

        // Create index buffer in DEFAULT heap with synchronized upload
        // format: DXGI_FORMAT_R16_UINT or DXGI_FORMAT_R32_UINT
        IndexBufferResult CreateIndexBuffer(const void* data, uint64_t sizeBytes, DXGI_FORMAT format);

    private:
        // Upload data from CPU to DEFAULT heap buffer with fence wait
        bool UploadBuffer(ID3D12Resource* dstDefault, const void* srcData, uint64_t numBytes,
                         D3D12_RESOURCE_STATES afterState);

        ID3D12Device* m_device = nullptr;  // Non-owning
        ID3D12CommandQueue* m_queue = nullptr;  // Non-owning

        Microsoft::WRL::ComPtr<ID3D12Fence> m_uploadFence;
        HANDLE m_uploadFenceEvent = nullptr;
        uint64_t m_uploadFenceValue = 0;
    };
}
