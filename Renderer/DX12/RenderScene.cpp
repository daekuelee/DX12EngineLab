#include "RenderScene.h"

using Microsoft::WRL::ComPtr;

namespace Renderer
{
    bool RenderScene::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue)
    {
        if (!device || !queue)
            return false;

        // Create fence for upload synchronization
        HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_uploadFence));
        if (FAILED(hr))
            return false;

        m_uploadFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!m_uploadFenceEvent)
            return false;

        m_uploadFenceValue = 0;

        if (!CreateCubeGeometry(device, queue))
        {
            OutputDebugStringA("RenderScene: Failed to create cube geometry\n");
            return false;
        }

        if (!CreateFloorGeometry(device, queue))
        {
            OutputDebugStringA("RenderScene: Failed to create floor geometry\n");
            return false;
        }

        if (!CreateMarkerGeometry(device, queue))
        {
            OutputDebugStringA("RenderScene: Failed to create marker geometry\n");
            return false;
        }

        OutputDebugStringA("RenderScene: Geometry created successfully\n");
        return true;
    }

    void RenderScene::Shutdown()
    {
        if (m_uploadFenceEvent)
        {
            CloseHandle(m_uploadFenceEvent);
            m_uploadFenceEvent = nullptr;
        }

        m_uploadFence.Reset();
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

        // Naive: 10k individual draw calls, each with StartInstanceLocation to read correct transform
        for (uint32_t i = 0; i < instanceCount; ++i)
        {
            cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, i);
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

    bool RenderScene::CreateCubeGeometry(ID3D12Device* device, ID3D12CommandQueue* queue)
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
            // -Z face (front)
            0, 1, 2, 0, 2, 3,
            // +Z face (back)
            4, 6, 5, 4, 7, 6,
            // -X face (left)
            4, 5, 1, 4, 1, 0,
            // +X face (right)
            3, 2, 6, 3, 6, 7,
            // +Y face (top)
            1, 5, 6, 1, 6, 2,
            // -Y face (bottom)
            4, 0, 3, 4, 3, 7
        };

        m_indexCount = _countof(indices);

        const uint64_t vbBytes = sizeof(vertices);
        const uint64_t ibBytes = sizeof(indices);

        // Create vertex buffer in DEFAULT heap
        {
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = vbBytes;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_vertexBuffer));

            if (FAILED(hr))
                return false;

            if (!UploadBuffer(device, queue, m_vertexBuffer.Get(), vertices, vbBytes,
                             D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
                return false;

            m_vbv.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
            m_vbv.SizeInBytes = static_cast<UINT>(vbBytes);
            m_vbv.StrideInBytes = sizeof(Vertex);
        }

        // Create index buffer in DEFAULT heap
        {
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = ibBytes;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_indexBuffer));

            if (FAILED(hr))
                return false;

            if (!UploadBuffer(device, queue, m_indexBuffer.Get(), indices, ibBytes,
                             D3D12_RESOURCE_STATE_INDEX_BUFFER))
                return false;

            m_ibv.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
            m_ibv.SizeInBytes = static_cast<UINT>(ibBytes);
            m_ibv.Format = DXGI_FORMAT_R16_UINT;
        }

        return true;
    }

    bool RenderScene::CreateFloorGeometry(ID3D12Device* device, ID3D12CommandQueue* queue)
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

        const uint64_t vbBytes = sizeof(vertices);
        const uint64_t ibBytes = sizeof(indices);

        // Create floor vertex buffer in DEFAULT heap
        {
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = vbBytes;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_floorVertexBuffer));

            if (FAILED(hr))
                return false;

            if (!UploadBuffer(device, queue, m_floorVertexBuffer.Get(), vertices, vbBytes,
                             D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
                return false;

            m_floorVbv.BufferLocation = m_floorVertexBuffer->GetGPUVirtualAddress();
            m_floorVbv.SizeInBytes = static_cast<UINT>(vbBytes);
            m_floorVbv.StrideInBytes = sizeof(Vertex);
        }

        // Create floor index buffer in DEFAULT heap
        {
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = ibBytes;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_floorIndexBuffer));

            if (FAILED(hr))
                return false;

            if (!UploadBuffer(device, queue, m_floorIndexBuffer.Get(), indices, ibBytes,
                             D3D12_RESOURCE_STATE_INDEX_BUFFER))
                return false;

            m_floorIbv.BufferLocation = m_floorIndexBuffer->GetGPUVirtualAddress();
            m_floorIbv.SizeInBytes = static_cast<UINT>(ibBytes);
            m_floorIbv.Format = DXGI_FORMAT_R16_UINT;
        }

        return true;
    }

    bool RenderScene::CreateMarkerGeometry(ID3D12Device* device, ID3D12CommandQueue* queue)
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
        const uint64_t vbBytes = sizeof(vertices);

        // Create marker vertex buffer in DEFAULT heap
        {
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = vbBytes;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_markerVertexBuffer));

            if (FAILED(hr))
                return false;

            if (!UploadBuffer(device, queue, m_markerVertexBuffer.Get(), vertices, vbBytes,
                             D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
                return false;

            m_markerVbv.BufferLocation = m_markerVertexBuffer->GetGPUVirtualAddress();
            m_markerVbv.SizeInBytes = static_cast<UINT>(vbBytes);
            m_markerVbv.StrideInBytes = sizeof(Vertex);
        }

        return true;
    }

    bool RenderScene::UploadBuffer(ID3D12Device* device, ID3D12CommandQueue* queue,
                                   ID3D12Resource* dstDefault, const void* srcData, uint64_t numBytes,
                                   D3D12_RESOURCE_STATES afterState)
    {
        // Create upload buffer
        ComPtr<ID3D12Resource> uploadBuffer;
        {
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = numBytes;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&uploadBuffer));

            if (FAILED(hr))
                return false;
        }

        // Map and copy data
        {
            void* mapped = nullptr;
            D3D12_RANGE readRange = { 0, 0 };
            HRESULT hr = uploadBuffer->Map(0, &readRange, &mapped);
            if (FAILED(hr))
                return false;

            memcpy(mapped, srcData, static_cast<size_t>(numBytes));
            uploadBuffer->Unmap(0, nullptr);
        }

        // Create command allocator and list for upload
        ComPtr<ID3D12CommandAllocator> cmdAlloc;
        ComPtr<ID3D12GraphicsCommandList> cmdList;

        HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
        if (FAILED(hr))
            return false;

        hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList));
        if (FAILED(hr))
            return false;

        // Record copy command
        cmdList->CopyBufferRegion(dstDefault, 0, uploadBuffer.Get(), 0, numBytes);

        // Transition to final state
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = dstDefault;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = afterState;
        cmdList->ResourceBarrier(1, &barrier);

        hr = cmdList->Close();
        if (FAILED(hr))
            return false;

        // Execute and wait
        ID3D12CommandList* cmdLists[] = { cmdList.Get() };
        queue->ExecuteCommandLists(1, cmdLists);

        m_uploadFenceValue++;
        hr = queue->Signal(m_uploadFence.Get(), m_uploadFenceValue);
        if (FAILED(hr))
            return false;

        if (m_uploadFence->GetCompletedValue() < m_uploadFenceValue)
        {
            m_uploadFence->SetEventOnCompletion(m_uploadFenceValue, m_uploadFenceEvent);
            WaitForSingleObject(m_uploadFenceEvent, INFINITE);
        }

        return true;
    }
}
