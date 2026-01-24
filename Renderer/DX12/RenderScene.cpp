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

        OutputDebugStringA("RenderScene: Cube geometry created successfully\n");
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
