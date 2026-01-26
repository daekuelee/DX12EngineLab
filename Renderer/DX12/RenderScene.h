#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace Renderer
{
    class GeometryFactory;

    class RenderScene
    {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        RenderScene(const RenderScene&) = delete;
        RenderScene& operator=(const RenderScene&) = delete;

        // Initialize geometry (VB/IB in DEFAULT heap via factory)
        bool Initialize(GeometryFactory* factory);

        // Shutdown and release resources
        void Shutdown();

        // Record draw commands - instanced mode (1 draw call)
        void RecordDraw(ID3D12GraphicsCommandList* cmdList, uint32_t instanceCount);

        // Record draw commands - naive mode (instanceCount draw calls, 1 instance each)
        void RecordDrawNaive(ID3D12GraphicsCommandList* cmdList, uint32_t instanceCount);

        // Record draw commands for floor (single draw call)
        void RecordDrawFloor(ID3D12GraphicsCommandList* cmdList);

        // Record draw commands for corner markers (visual diagnostic)
        void RecordDrawMarkers(ID3D12GraphicsCommandList* cmdList);

        // Accessors
        uint32_t GetIndexCount() const { return m_indexCount; }
        D3D12_VERTEX_BUFFER_VIEW GetVBView() const { return m_vbv; }
        D3D12_INDEX_BUFFER_VIEW GetIBView() const { return m_ibv; }

        // Cube geometry accessors (return by reference for direct use with IASet*)
        const D3D12_VERTEX_BUFFER_VIEW& GetCubeVBV() const { return m_vbv; }
        const D3D12_INDEX_BUFFER_VIEW& GetCubeIBV() const { return m_ibv; }
        uint32_t GetCubeIndexCount() const { return m_indexCount; }

    private:
        bool CreateCubeGeometry(GeometryFactory* factory);
        bool CreateFloorGeometry(GeometryFactory* factory);
        bool CreateMarkerGeometry(GeometryFactory* factory);

        // Cube geometry in DEFAULT heap
        Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vbv = {};
        D3D12_INDEX_BUFFER_VIEW m_ibv = {};
        uint32_t m_indexCount = 0;

        // Floor geometry in DEFAULT heap
        Microsoft::WRL::ComPtr<ID3D12Resource> m_floorVertexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_floorIndexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_floorVbv = {};
        D3D12_INDEX_BUFFER_VIEW m_floorIbv = {};
        uint32_t m_floorIndexCount = 0;

        // Marker geometry in DEFAULT heap (corner triangles for visual diagnostic)
        Microsoft::WRL::ComPtr<ID3D12Resource> m_markerVertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_markerVbv = {};
        uint32_t m_markerVertexCount = 0;
    };
}
