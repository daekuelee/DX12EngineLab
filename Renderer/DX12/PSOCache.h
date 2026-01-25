#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <unordered_map>

namespace Renderer
{
    //-------------------------------------------------------------------------
    // PSOKey: Complete field coverage for PSO hash/compare
    //
    // All fields that affect PSO compilation are included. Zero false cache hits.
    // Root signature stored as pointer - caller must ensure it outlives cached PSOs.
    //-------------------------------------------------------------------------
    struct PSOKey
    {
        // === Shaders (bytecode identity via hash) ===
        uint64_t vsHash = 0;
        uint64_t psHash = 0;
        uint64_t gsHash = 0;  // 0 if null
        uint64_t hsHash = 0;  // 0 if null
        uint64_t dsHash = 0;  // 0 if null

        // === Root Signature ===
        // Stored as pointer - caller contract: root sig must outlive cached PSOs
        ID3D12RootSignature* rootSignature = nullptr;

        // === Input Layout ===
        // Hash of serialized InputElementDescs
        uint64_t inputLayoutHash = 0;

        // === Rasterizer State (all fields) ===
        D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID;
        D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;
        BOOL frontCounterClockwise = FALSE;
        INT depthBias = 0;
        FLOAT depthBiasClamp = 0.0f;
        FLOAT slopeScaledDepthBias = 0.0f;
        BOOL depthClipEnable = TRUE;
        BOOL multisampleEnable = FALSE;
        BOOL antialiasedLineEnable = FALSE;
        UINT forcedSampleCount = 0;
        D3D12_CONSERVATIVE_RASTERIZATION_MODE conservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // === Depth Stencil State (all fields) ===
        BOOL depthEnable = TRUE;
        D3D12_DEPTH_WRITE_MASK depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS;
        BOOL stencilEnable = FALSE;
        UINT8 stencilReadMask = 0xFF;
        UINT8 stencilWriteMask = 0xFF;
        D3D12_DEPTH_STENCILOP_DESC frontFace = {};
        D3D12_DEPTH_STENCILOP_DESC backFace = {};

        // === Blend State ===
        BOOL alphaToCoverageEnable = FALSE;
        BOOL independentBlendEnable = FALSE;
        D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlend[8] = {};

        // === Output Merger ===
        UINT numRenderTargets = 1;
        DXGI_FORMAT rtvFormats[8] = {};
        DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;

        // === Sample Desc ===
        UINT sampleCount = 1;
        UINT sampleQuality = 0;
        UINT sampleMask = UINT_MAX;

        // === Misc ===
        D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ibStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        // Comparison
        bool operator==(const PSOKey& other) const;
        bool operator!=(const PSOKey& other) const { return !(*this == other); }

        // Hash
        size_t Hash() const;
    };

    // Hasher for use with std::unordered_map
    struct PSOKeyHasher
    {
        size_t operator()(const PSOKey& key) const { return key.Hash(); }
    };

    //-------------------------------------------------------------------------
    // PSOCache: Hash-based lazy PSO creation with caching
    //
    // Usage:
    //   1. Initialize() with device
    //   2. PreWarm() known PSOs at startup (optional but recommended)
    //   3. GetOrCreate() during rendering - returns cached or creates new
    //   4. LogStats() to verify hit rate
    //   5. Shutdown() to release all PSOs
    //-------------------------------------------------------------------------
    class PSOCache
    {
    public:
        PSOCache() = default;
        ~PSOCache() = default;

        // Non-copyable
        PSOCache(const PSOCache&) = delete;
        PSOCache& operator=(const PSOCache&) = delete;

        // Initialize cache with device reference
        bool Initialize(ID3D12Device* device, uint32_t maxEntries = 128);

        // Shutdown and release all cached PSOs
        void Shutdown();

        // Get or create PSO. Tag is for debug logging only.
        // Returns nullptr on failure (hard-fail in debug).
        ID3D12PipelineState* GetOrCreate(
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
            const char* tag = nullptr);

        // Pre-warm cache at startup for known PSOs
        // Returns true if PSO created successfully
        bool PreWarm(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const char* tag);

        // Print cache stats to debug output
        void LogStats() const;

        // Accessors
        uint64_t GetHitCount() const { return m_hits; }
        uint64_t GetMissCount() const { return m_misses; }
        uint32_t GetEntryCount() const { return static_cast<uint32_t>(m_cache.size()); }

        // Build PSOKey from descriptor (exposed for testing/debugging)
        static PSOKey BuildKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

        // Hash helper for bytecode (FNV-1a 64-bit)
        static uint64_t HashBytecode(const void* data, size_t size);

        // Hash helper for input layout
        static uint64_t HashInputLayout(const D3D12_INPUT_ELEMENT_DESC* elements, uint32_t count);

    private:
        ID3D12Device* m_device = nullptr;
        std::unordered_map<PSOKey, Microsoft::WRL::ComPtr<ID3D12PipelineState>, PSOKeyHasher> m_cache;
        uint64_t m_hits = 0;
        uint64_t m_misses = 0;
        uint32_t m_maxEntries = 128;
    };
}
