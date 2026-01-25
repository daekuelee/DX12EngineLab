#include "ResourceRegistry.h"
#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace Renderer
{
    bool ResourceRegistry::Initialize(ID3D12Device* device, uint32_t capacity)
    {
        if (!device)
        {
            OutputDebugStringA("[ResourceRegistry] ERROR: null device\n");
            return false;
        }

        m_device = device;
        m_entries.resize(capacity);
        m_freeList.clear();
        m_activeCount = 0;

        // Initialize free list with all slots (in reverse order for LIFO)
        m_freeList.reserve(capacity);
        for (uint32_t i = capacity; i > 0; --i)
        {
            m_freeList.push_back(i - 1);
        }

        char buf[128];
        sprintf_s(buf, "[ResourceRegistry] Initialized with capacity=%u\n", capacity);
        OutputDebugStringA(buf);

        return true;
    }

    void ResourceRegistry::Shutdown()
    {
        LogStats();

        // Release all resources
        for (Entry& entry : m_entries)
        {
            if (entry.inUse && entry.resource)
            {
                entry.resource.Reset();
            }
            entry.inUse = false;
            entry.debugName.clear();
        }

        m_freeList.clear();
        m_entries.clear();
        m_activeCount = 0;
        m_device = nullptr;

        OutputDebugStringA("[ResourceRegistry] Shutdown complete\n");
    }

    bool ResourceRegistry::CreateResourceInternal(const ResourceDesc& desc, Entry& entry)
    {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = desc.heapType;

        D3D12_RESOURCE_DESC resourceDesc = {};

        if (desc.type == ResourceType::Buffer)
        {
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Width = desc.width;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Flags = desc.flags;
        }
        else
        {
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.Width = desc.width;
            resourceDesc.Height = desc.height;
            resourceDesc.DepthOrArraySize = static_cast<UINT16>(desc.depthOrArraySize);
            resourceDesc.MipLevels = static_cast<UINT16>(desc.mipLevels);
            resourceDesc.Format = desc.format;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resourceDesc.Flags = desc.flags;
        }

        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            desc.initialState,
            desc.clearValue,
            IID_PPV_ARGS(&entry.resource));

        if (FAILED(hr))
        {
            char buf[128];
            sprintf_s(buf, "[ResourceRegistry] ERROR: CreateCommittedResource failed (0x%08X) name=%s\n",
                      hr, desc.debugName ? desc.debugName : "?");
            OutputDebugStringA(buf);
            return false;
        }

        entry.state = desc.initialState;
        entry.inUse = true;
        entry.debugName = desc.debugName ? desc.debugName : "";

        // Set debug name on resource
        if (desc.debugName && entry.resource)
        {
            wchar_t wname[256];
            size_t converted = 0;
            mbstowcs_s(&converted, wname, desc.debugName, 255);
            entry.resource->SetName(wname);
        }

        return true;
    }

    ResourceHandle ResourceRegistry::Create(const ResourceDesc& desc)
    {
        if (!m_device)
        {
            OutputDebugStringA("[ResourceRegistry] ERROR: not initialized\n");
            return {};
        }

        // Check for capacity
        if (m_freeList.empty())
        {
            char buf[128];
            sprintf_s(buf, "[ResourceRegistry] ERROR: capacity reached (%u entries)\n",
                      static_cast<uint32_t>(m_entries.size()));
            OutputDebugStringA(buf);
#if defined(_DEBUG)
            __debugbreak();
#endif
            return {};
        }

        // Pop from free list
        uint32_t index = m_freeList.back();
        m_freeList.pop_back();

        Entry& entry = m_entries[index];

        // Increment generation when reusing a slot
        entry.generation++;

        // Create the resource
        if (!CreateResourceInternal(desc, entry))
        {
            // Put back on free list
            m_freeList.push_back(index);
            return {};
        }

        ++m_activeCount;

        // Build handle
        ResourceHandle handle = ResourceHandle::Make(entry.generation, index, desc.type);

        char buf[256];
        sprintf_s(buf, "[ResourceRegistry] Created: idx=%u gen=%u type=%d name=\"%s\"\n",
                  index, entry.generation, static_cast<int>(desc.type),
                  desc.debugName ? desc.debugName : "");
        OutputDebugStringA(buf);

        return handle;
    }

    void ResourceRegistry::Destroy(ResourceHandle handle)
    {
        if (!handle.IsValid())
            return;

        if (!IsValid(handle))
        {
            // Handle is stale or invalid - no-op
            return;
        }

        uint32_t index = handle.GetIndex();
        Entry& entry = m_entries[index];

        char buf[256];
        sprintf_s(buf, "[ResourceRegistry] Destroyed: idx=%u gen=%u name=\"%s\"\n",
                  index, entry.generation, entry.debugName.c_str());
        OutputDebugStringA(buf);

        // Release resource
        entry.resource.Reset();
        entry.inUse = false;
        entry.debugName.clear();
        entry.state = D3D12_RESOURCE_STATE_COMMON;

        // Add to free list (generation NOT incremented until reuse)
        m_freeList.push_back(index);

        --m_activeCount;
    }

    ID3D12Resource* ResourceRegistry::Get(ResourceHandle handle) const
    {
        if (!IsValid(handle))
            return nullptr;

        return m_entries[handle.GetIndex()].resource.Get();
    }

    D3D12_RESOURCE_STATES ResourceRegistry::GetState(ResourceHandle handle) const
    {
        if (!IsValid(handle))
            return D3D12_RESOURCE_STATE_COMMON;

        return m_entries[handle.GetIndex()].state;
    }

    void ResourceRegistry::SetState(ResourceHandle handle, D3D12_RESOURCE_STATES state)
    {
        if (!IsValid(handle))
            return;

        m_entries[handle.GetIndex()].state = state;
    }

    bool ResourceRegistry::IsValid(ResourceHandle handle) const
    {
        if (!handle.IsValid())
            return false;

        uint32_t index = handle.GetIndex();
        if (index >= m_entries.size())
            return false;

        const Entry& entry = m_entries[index];
        return entry.inUse && (entry.generation == handle.GetGeneration());
    }

    void ResourceRegistry::LogStats() const
    {
        char buf[256];
        sprintf_s(buf, "[ResourceRegistry] Stats: %u active, %u free, %u capacity\n",
                  m_activeCount,
                  static_cast<uint32_t>(m_freeList.size()),
                  static_cast<uint32_t>(m_entries.size()));
        OutputDebugStringA(buf);
    }
}
