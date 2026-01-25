#include "GeometryFactory.h"

using Microsoft::WRL::ComPtr;

namespace Renderer
{
    bool GeometryFactory::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue)
    {
        if (!device || !queue)
            return false;

        m_device = device;
        m_queue = queue;

        HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_uploadFence));
        if (FAILED(hr))
            return false;

        m_uploadFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!m_uploadFenceEvent)
            return false;

        m_uploadFenceValue = 0;
        return true;
    }

    void GeometryFactory::Shutdown()
    {
        if (m_uploadFenceEvent)
        {
            CloseHandle(m_uploadFenceEvent);
            m_uploadFenceEvent = nullptr;
        }

        m_uploadFence.Reset();
        m_device = nullptr;
        m_queue = nullptr;
    }

    VertexBufferResult GeometryFactory::CreateVertexBuffer(const void* data, uint64_t sizeBytes, uint32_t strideBytes)
    {
        VertexBufferResult result;

        if (!m_device || !data || sizeBytes == 0)
            return result;

        // Create buffer in DEFAULT heap
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&result.resource));

        if (FAILED(hr))
            return VertexBufferResult{};

        if (!UploadBuffer(result.resource.Get(), data, sizeBytes,
                         D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
        {
            return VertexBufferResult{};
        }

        result.view.BufferLocation = result.resource->GetGPUVirtualAddress();
        result.view.SizeInBytes = static_cast<UINT>(sizeBytes);
        result.view.StrideInBytes = strideBytes;

        return result;
    }

    IndexBufferResult GeometryFactory::CreateIndexBuffer(const void* data, uint64_t sizeBytes, DXGI_FORMAT format)
    {
        IndexBufferResult result;

        if (!m_device || !data || sizeBytes == 0)
            return result;

        // Create buffer in DEFAULT heap
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&result.resource));

        if (FAILED(hr))
            return IndexBufferResult{};

        if (!UploadBuffer(result.resource.Get(), data, sizeBytes,
                         D3D12_RESOURCE_STATE_INDEX_BUFFER))
        {
            return IndexBufferResult{};
        }

        result.view.BufferLocation = result.resource->GetGPUVirtualAddress();
        result.view.SizeInBytes = static_cast<UINT>(sizeBytes);
        result.view.Format = format;

        return result;
    }

    bool GeometryFactory::UploadBuffer(ID3D12Resource* dstDefault, const void* srcData, uint64_t numBytes,
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

            HRESULT hr = m_device->CreateCommittedResource(
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

        HRESULT hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
        if (FAILED(hr))
            return false;

        hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList));
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
        m_queue->ExecuteCommandLists(1, cmdLists);

        m_uploadFenceValue++;
        hr = m_queue->Signal(m_uploadFence.Get(), m_uploadFenceValue);
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
