#include "RenderScene.h"
#include "GeometryFactory.h"
#include <utility>

using Microsoft::WRL::ComPtr;

namespace Renderer
{
    bool RenderScene::Initialize(GeometryFactory* factory)
    {
        if (!factory)
            return false;

        if (!CreateCubeGeometry(factory))
        {
            OutputDebugStringA("RenderScene: Failed to create cube geometry\n");
            return false;
        }

        if (!CreateFloorGeometry(factory))
        {
            OutputDebugStringA("RenderScene: Failed to create floor geometry\n");
            return false;
        }

        if (!CreateMarkerGeometry(factory))
        {
            OutputDebugStringA("RenderScene: Failed to create marker geometry\n");
            return false;
        }

        OutputDebugStringA("RenderScene: Geometry created successfully\n");
        return true;
    }

    void RenderScene::Shutdown()
    {
        m_vertexBuffer.Reset();
        m_indexBuffer.Reset();
        m_vbv = {};
        m_ibv = {};
        m_indexCount = 0;

        m_floorVertexBuffer.Reset();
        m_floorIndexBuffer.Reset();
        m_floorVbv = {};
        m_floorIbv = {};
        m_floorIndexCount = 0;

        m_markerVertexBuffer.Reset();
        m_markerVbv = {};
        m_markerVertexCount = 0;
    }

    void RenderScene::RecordDraw(ID3D12GraphicsCommandList* cmdList, uint32_t instanceCount)
    {
        // Set geometry (D4: IA/RS/OM set in Tick, not RecordDraw - but VB/IB binding is data, not state)
        cmdList->IASetVertexBuffers(0, 1, &m_vbv);
        cmdList->IASetIndexBuffer(&m_ibv);

        // Draw (primitive topology set by caller per D4)
        cmdList->DrawIndexedInstanced(m_indexCount, instanceCount, 0, 0, 0);
    }

    void RenderScene::RecordDrawNaive(ID3D12GraphicsCommandList* cmdList, uint32_t instanceCount)
    {
        // Set geometry once
        cmdList->IASetVertexBuffers(0, 1, &m_vbv);
        cmdList->IASetIndexBuffer(&m_ibv);

        // Naive: 10k individual draw calls, each with InstanceOffset root constant
        // SV_InstanceID does NOT include StartInstanceLocation, so we pass offset via root constant
        for (uint32_t i = 0; i < instanceCount; ++i)
        {
            cmdList->SetGraphicsRoot32BitConstants(2, 1, &i, 0);  // RP_InstanceOffset
            cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
        }
    }

    void RenderScene::RecordDrawFloor(ID3D12GraphicsCommandList* cmdList)
    {
        cmdList->IASetVertexBuffers(0, 1, &m_floorVbv);
        cmdList->IASetIndexBuffer(&m_floorIbv);

        // Floor uses instance 0's transform (identity at origin)
        cmdList->DrawIndexedInstanced(m_floorIndexCount, 1, 0, 0, 0);
    }

    void RenderScene::RecordDrawMarkers(ID3D12GraphicsCommandList* cmdList)
    {
        cmdList->IASetVertexBuffers(0, 1, &m_markerVbv);
        // No index buffer - using DrawInstanced with vertex buffer only
        cmdList->DrawInstanced(m_markerVertexCount, 1, 0, 0);
    }

    bool RenderScene::CreateCubeGeometry(GeometryFactory* factory)
    {
        // Simple cube: 8 vertices, 36 indices (12 triangles)
        struct Vertex { float x, y, z; };

        const Vertex vertices[] = {
            // Front face
            {-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1}, { 1, -1, -1},
            // Back face
            {-1, -1,  1}, {-1,  1,  1}, { 1,  1,  1}, { 1, -1,  1}
        };

        const uint16_t indices[] = {
            // -Z face (front) - CW viewed from -Z
            0, 2, 1,  0, 3, 2,
            // +Z face (back) - CW viewed from +Z
            4, 6, 5,  4, 7, 6,
            // -X face (left) - CW viewed from -X
            0, 1, 5,  0, 5, 4,
            // +X face (right) - CW viewed from +X
            3, 6, 2,  3, 7, 6,
            // +Y face (top) - CW viewed from +Y
            1, 2, 6,  1, 6, 5,
            // -Y face (bottom) - CW viewed from -Y
            0, 4, 7,  0, 7, 3
        };

        m_indexCount = _countof(indices);

        // Create vertex buffer via factory
        auto vbResult = factory->CreateVertexBuffer(vertices, sizeof(vertices), sizeof(Vertex));
        if (!vbResult.resource)
            return false;

        m_vertexBuffer = std::move(vbResult.resource);
        m_vbv = vbResult.view;

        // Create index buffer via factory
        auto ibResult = factory->CreateIndexBuffer(indices, sizeof(indices), DXGI_FORMAT_R16_UINT);
        if (!ibResult.resource)
            return false;

        m_indexBuffer = std::move(ibResult.resource);
        m_ibv = ibResult.view;

        return true;
    }

    bool RenderScene::CreateFloorGeometry(GeometryFactory* factory)
    {
        // Large floor quad at y=-0.01 (slightly below cubes at y=0)
        struct Vertex { float x, y, z; };

        const Vertex vertices[] = {
            {-200.0f, -0.01f, -200.0f},
            {-200.0f, -0.01f,  200.0f},
            { 200.0f, -0.01f,  200.0f},
            { 200.0f, -0.01f, -200.0f}
        };

        const uint16_t indices[] = {
            0, 1, 2,
            0, 2, 3
        };

        m_floorIndexCount = _countof(indices);

        // Create floor vertex buffer via factory
        auto vbResult = factory->CreateVertexBuffer(vertices, sizeof(vertices), sizeof(Vertex));
        if (!vbResult.resource)
            return false;

        m_floorVertexBuffer = std::move(vbResult.resource);
        m_floorVbv = vbResult.view;

        // Create floor index buffer via factory
        auto ibResult = factory->CreateIndexBuffer(indices, sizeof(indices), DXGI_FORMAT_R16_UINT);
        if (!ibResult.resource)
            return false;

        m_floorIndexBuffer = std::move(ibResult.resource);
        m_floorIbv = ibResult.view;

        return true;
    }

    bool RenderScene::CreateMarkerGeometry(GeometryFactory* factory)
    {
        // 4 small triangles at NDC corners to visually prove drawable region
        // Each triangle is 3 vertices, total 12 vertices
        // Vertices are already in clip space (NDC), will use pass-through VS
        struct Vertex { float x, y, z; };

        const float s = 0.08f; // Size in NDC (8% of screen)
        const Vertex vertices[] = {
            // Bottom-left (NDC -1,-1) - appears at bottom-left of screen
            {-1.0f,       -1.0f,       0.5f},
            {-1.0f + s,   -1.0f,       0.5f},
            {-1.0f,       -1.0f + s,   0.5f},

            // Bottom-right (NDC 1,-1) - appears at bottom-right of screen
            { 1.0f - s,   -1.0f,       0.5f},
            { 1.0f,       -1.0f,       0.5f},
            { 1.0f,       -1.0f + s,   0.5f},

            // Top-left (NDC -1,1) - appears at top-left of screen
            {-1.0f,        1.0f - s,   0.5f},
            {-1.0f + s,    1.0f,       0.5f},
            {-1.0f,        1.0f,       0.5f},

            // Top-right (NDC 1,1) - appears at top-right of screen
            { 1.0f - s,    1.0f,       0.5f},
            { 1.0f,        1.0f,       0.5f},
            { 1.0f,        1.0f - s,   0.5f},
        };

        m_markerVertexCount = _countof(vertices);

        // Create marker vertex buffer via factory
        auto vbResult = factory->CreateVertexBuffer(vertices, sizeof(vertices), sizeof(Vertex));
        if (!vbResult.resource)
            return false;

        m_markerVertexBuffer = std::move(vbResult.resource);
        m_markerVbv = vbResult.view;

        return true;
    }
}
