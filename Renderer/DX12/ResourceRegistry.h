#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>
#include <string>

namespace Renderer
{
    //-------------------------------------------------------------------------
    // ResourceType: Identifies the kind of resource for validation
    //-------------------------------------------------------------------------
    enum class ResourceType : uint8_t
    {
        None = 0,
        Buffer,
        Texture2D,
        RenderTarget,
        DepthStencil
    };

    //-------------------------------------------------------------------------
    // ResourceHandle: 64-bit handle with generation validation
    //
    // Layout: | 32-bit generation | 24-bit index | 8-bit type |
    //
    // Generation prevents use-after-free bugs. When a resource is destroyed
    // and the slot is reused, the generation increments. Old handles with
    // the same index will fail validation (generation mismatch).
    //-------------------------------------------------------------------------
    struct ResourceHandle
    {
        uint64_t value = 0;

        bool IsValid() const { return value != 0; }

        uint32_t GetGeneration() const
        {
            return static_cast<uint32_t>(value >> 32);
        }

        uint32_t GetIndex() const
        {
            return static_cast<uint32_t>((value >> 8) & 0xFFFFFF);
        }

        ResourceType GetType() const
        {
            return static_cast<ResourceType>(value & 0xFF);
        }

        static ResourceHandle Make(uint32_t gen, uint32_t idx, ResourceType type)
        {
            ResourceHandle h;
            h.value = (static_cast<uint64_t>(gen) << 32) |
                      (static_cast<uint64_t>(idx & 0xFFFFFF) << 8) |
                      static_cast<uint64_t>(static_cast<uint8_t>(type));
            return h;
        }

        bool operator==(ResourceHandle o) const { return value == o.value; }
        bool operator!=(ResourceHandle o) const { return value != o.value; }
    };

    //-------------------------------------------------------------------------
    // ResourceDesc: Description for creating a resource
    //-------------------------------------------------------------------------
    struct ResourceDesc
    {
        ResourceType type = ResourceType::Buffer;
        D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
        uint64_t width = 0;          // Buffer size or texture width
        uint32_t height = 1;         // Texture height (1 for buffers)
        uint32_t depthOrArraySize = 1;
        uint32_t mipLevels = 1;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_CLEAR_VALUE* clearValue = nullptr;  // For render targets/depth stencils
        const char* debugName = nullptr;

        // Helper: Create buffer desc
        static ResourceDesc Buffer(uint64_t size, D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT,
                                   D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON,
                                   const char* name = nullptr)
        {
            ResourceDesc d;
            d.type = ResourceType::Buffer;
            d.heapType = heap;
            d.width = size;
            d.initialState = state;
            d.debugName = name;
            return d;
        }

        // Helper: Create texture2D desc
        static ResourceDesc Texture2D(uint32_t w, uint32_t h, DXGI_FORMAT fmt,
                                      D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
                                      D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON,
                                      const char* name = nullptr)
        {
            ResourceDesc d;
            d.type = ResourceType::Texture2D;
            d.heapType = D3D12_HEAP_TYPE_DEFAULT;
            d.width = w;
            d.height = h;
            d.format = fmt;
            d.flags = flags;
            d.initialState = state;
            d.debugName = name;
            return d;
        }
    };

    //-------------------------------------------------------------------------
    // ResourceRegistry: Handle-based resource ownership with generation validation
    //
    // Usage:
    //   1. Initialize() with device and capacity
    //   2. Create() resources to get handles
    //   3. Get() to access raw resource pointers
    //   4. Destroy() when done (safe to call with invalid handles)
    //   5. Shutdown() to release all resources
    //
    // Migration strategy: This is infrastructure-only in Phase 2. Existing code
    // continues to work. Migration to handles is opt-in and incremental.
    //-------------------------------------------------------------------------
    class ResourceRegistry
    {
    public:
        ResourceRegistry() = default;
        ~ResourceRegistry() = default;

        // Non-copyable
        ResourceRegistry(const ResourceRegistry&) = delete;
        ResourceRegistry& operator=(const ResourceRegistry&) = delete;

        // Initialize registry with device and capacity
        bool Initialize(ID3D12Device* device, uint32_t capacity = 256);

        // Shutdown and release all resources
        void Shutdown();

        // Create resource, returns handle. Hard-fail in debug on capacity overflow.
        ResourceHandle Create(const ResourceDesc& desc);

        // Destroy by handle. Safe to call with invalid handle (no-op).
        void Destroy(ResourceHandle handle);

        // Get raw resource. Returns nullptr if handle invalid/stale.
        ID3D12Resource* Get(ResourceHandle handle) const;

        // State tracking (for barrier management)
        D3D12_RESOURCE_STATES GetState(ResourceHandle handle) const;
        void SetState(ResourceHandle handle, D3D12_RESOURCE_STATES state);

        // Validation
        bool IsValid(ResourceHandle handle) const;

        // Debug
        uint32_t GetActiveCount() const { return m_activeCount; }
        uint32_t GetCapacity() const { return static_cast<uint32_t>(m_entries.size()); }
        void LogStats() const;

    private:
        struct Entry
        {
            Microsoft::WRL::ComPtr<ID3D12Resource> resource;
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
            uint32_t generation = 0;
            bool inUse = false;
            std::string debugName;
        };

        ID3D12Device* m_device = nullptr;
        std::vector<Entry> m_entries;
        std::vector<uint32_t> m_freeList;
        uint32_t m_activeCount = 0;

        bool CreateResourceInternal(const ResourceDesc& desc, Entry& entry);
    };
}
