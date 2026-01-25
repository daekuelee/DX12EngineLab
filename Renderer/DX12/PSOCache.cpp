#include "PSOCache.h"
#include <cstdio>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace Renderer
{
    //-------------------------------------------------------------------------
    // FNV-1a 64-bit hash constants
    //-------------------------------------------------------------------------
    static constexpr uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
    static constexpr uint64_t FNV_PRIME = 0x100000001b3ULL;

    //-------------------------------------------------------------------------
    // PSOKey Implementation
    //-------------------------------------------------------------------------

    bool PSOKey::operator==(const PSOKey& other) const
    {
        // Compare all fields in order of definition

        // Shader hashes
        if (vsHash != other.vsHash) return false;
        if (psHash != other.psHash) return false;
        if (gsHash != other.gsHash) return false;
        if (hsHash != other.hsHash) return false;
        if (dsHash != other.dsHash) return false;

        // Root signature
        if (rootSignature != other.rootSignature) return false;

        // Input layout hash
        if (inputLayoutHash != other.inputLayoutHash) return false;

        // Rasterizer state
        if (fillMode != other.fillMode) return false;
        if (cullMode != other.cullMode) return false;
        if (frontCounterClockwise != other.frontCounterClockwise) return false;
        if (depthBias != other.depthBias) return false;
        if (depthBiasClamp != other.depthBiasClamp) return false;
        if (slopeScaledDepthBias != other.slopeScaledDepthBias) return false;
        if (depthClipEnable != other.depthClipEnable) return false;
        if (multisampleEnable != other.multisampleEnable) return false;
        if (antialiasedLineEnable != other.antialiasedLineEnable) return false;
        if (forcedSampleCount != other.forcedSampleCount) return false;
        if (conservativeRaster != other.conservativeRaster) return false;

        // Depth stencil state
        if (depthEnable != other.depthEnable) return false;
        if (depthWriteMask != other.depthWriteMask) return false;
        if (depthFunc != other.depthFunc) return false;
        if (stencilEnable != other.stencilEnable) return false;
        if (stencilReadMask != other.stencilReadMask) return false;
        if (stencilWriteMask != other.stencilWriteMask) return false;
        if (memcmp(&frontFace, &other.frontFace, sizeof(frontFace)) != 0) return false;
        if (memcmp(&backFace, &other.backFace, sizeof(backFace)) != 0) return false;

        // Blend state
        if (alphaToCoverageEnable != other.alphaToCoverageEnable) return false;
        if (independentBlendEnable != other.independentBlendEnable) return false;
        for (UINT i = 0; i < 8; ++i)
        {
            if (memcmp(&renderTargetBlend[i], &other.renderTargetBlend[i], sizeof(D3D12_RENDER_TARGET_BLEND_DESC)) != 0)
                return false;
        }

        // Output merger
        if (numRenderTargets != other.numRenderTargets) return false;
        for (UINT i = 0; i < 8; ++i)
        {
            if (rtvFormats[i] != other.rtvFormats[i]) return false;
        }
        if (dsvFormat != other.dsvFormat) return false;

        // Sample desc
        if (sampleCount != other.sampleCount) return false;
        if (sampleQuality != other.sampleQuality) return false;
        if (sampleMask != other.sampleMask) return false;

        // Misc
        if (primitiveTopologyType != other.primitiveTopologyType) return false;
        if (ibStripCutValue != other.ibStripCutValue) return false;

        return true;
    }

    size_t PSOKey::Hash() const
    {
        // FNV-1a style combining of all key fields
        uint64_t hash = FNV_OFFSET;

        auto combine = [&hash](uint64_t value)
        {
            hash ^= value;
            hash *= FNV_PRIME;
        };

        // Shader hashes
        combine(vsHash);
        combine(psHash);
        combine(gsHash);
        combine(hsHash);
        combine(dsHash);

        // Root signature (pointer as value)
        combine(reinterpret_cast<uint64_t>(rootSignature));

        // Input layout
        combine(inputLayoutHash);

        // Rasterizer state
        combine(static_cast<uint64_t>(fillMode));
        combine(static_cast<uint64_t>(cullMode));
        combine(static_cast<uint64_t>(frontCounterClockwise));
        combine(static_cast<uint64_t>(depthBias));
        // Float hashes - bit cast
        combine(*reinterpret_cast<const uint32_t*>(&depthBiasClamp));
        combine(*reinterpret_cast<const uint32_t*>(&slopeScaledDepthBias));
        combine(static_cast<uint64_t>(depthClipEnable));
        combine(static_cast<uint64_t>(multisampleEnable));
        combine(static_cast<uint64_t>(antialiasedLineEnable));
        combine(static_cast<uint64_t>(forcedSampleCount));
        combine(static_cast<uint64_t>(conservativeRaster));

        // Depth stencil state
        combine(static_cast<uint64_t>(depthEnable));
        combine(static_cast<uint64_t>(depthWriteMask));
        combine(static_cast<uint64_t>(depthFunc));
        combine(static_cast<uint64_t>(stencilEnable));
        combine(static_cast<uint64_t>(stencilReadMask));
        combine(static_cast<uint64_t>(stencilWriteMask));
        // Stencil op descs - hash as bytes
        for (size_t i = 0; i < sizeof(frontFace); ++i)
            combine(reinterpret_cast<const uint8_t*>(&frontFace)[i]);
        for (size_t i = 0; i < sizeof(backFace); ++i)
            combine(reinterpret_cast<const uint8_t*>(&backFace)[i]);

        // Blend state
        combine(static_cast<uint64_t>(alphaToCoverageEnable));
        combine(static_cast<uint64_t>(independentBlendEnable));
        for (UINT i = 0; i < 8; ++i)
        {
            for (size_t j = 0; j < sizeof(D3D12_RENDER_TARGET_BLEND_DESC); ++j)
                combine(reinterpret_cast<const uint8_t*>(&renderTargetBlend[i])[j]);
        }

        // Output merger
        combine(static_cast<uint64_t>(numRenderTargets));
        for (UINT i = 0; i < 8; ++i)
            combine(static_cast<uint64_t>(rtvFormats[i]));
        combine(static_cast<uint64_t>(dsvFormat));

        // Sample desc
        combine(static_cast<uint64_t>(sampleCount));
        combine(static_cast<uint64_t>(sampleQuality));
        combine(static_cast<uint64_t>(sampleMask));

        // Misc
        combine(static_cast<uint64_t>(primitiveTopologyType));
        combine(static_cast<uint64_t>(ibStripCutValue));

        return static_cast<size_t>(hash);
    }

    //-------------------------------------------------------------------------
    // PSOCache Implementation
    //-------------------------------------------------------------------------

    bool PSOCache::Initialize(ID3D12Device* device, uint32_t maxEntries)
    {
        if (!device)
        {
            OutputDebugStringA("[PSOCache] ERROR: null device\n");
            return false;
        }

        m_device = device;
        m_maxEntries = maxEntries;
        m_hits = 0;
        m_misses = 0;
        m_cache.clear();

        OutputDebugStringA("[PSOCache] Initialized\n");
        return true;
    }

    void PSOCache::Shutdown()
    {
        LogStats();
        m_cache.clear();
        m_device = nullptr;
        OutputDebugStringA("[PSOCache] Shutdown complete\n");
    }

    uint64_t PSOCache::HashBytecode(const void* data, size_t size)
    {
        if (!data || size == 0)
            return 0;

        // FNV-1a 64-bit
        uint64_t hash = FNV_OFFSET;
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= FNV_PRIME;
        }
        return hash;
    }

    uint64_t PSOCache::HashInputLayout(const D3D12_INPUT_ELEMENT_DESC* elements, uint32_t count)
    {
        if (!elements || count == 0)
            return 0;

        uint64_t hash = FNV_OFFSET;

        hash ^= count;
        hash *= FNV_PRIME;

        for (uint32_t i = 0; i < count; ++i)
        {
            const D3D12_INPUT_ELEMENT_DESC& e = elements[i];

            // Hash semantic name (null-terminated string)
            if (e.SemanticName)
            {
                for (const char* p = e.SemanticName; *p; ++p)
                {
                    hash ^= static_cast<uint8_t>(*p);
                    hash *= FNV_PRIME;
                }
            }

            hash ^= e.SemanticIndex;
            hash *= FNV_PRIME;
            hash ^= static_cast<uint64_t>(e.Format);
            hash *= FNV_PRIME;
            hash ^= e.InputSlot;
            hash *= FNV_PRIME;
            hash ^= e.AlignedByteOffset;
            hash *= FNV_PRIME;
            hash ^= static_cast<uint64_t>(e.InputSlotClass);
            hash *= FNV_PRIME;
            hash ^= e.InstanceDataStepRate;
            hash *= FNV_PRIME;
        }

        return hash;
    }

    PSOKey PSOCache::BuildKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
    {
        PSOKey key = {};

        // Shader bytecode hashes
        if (desc.VS.pShaderBytecode && desc.VS.BytecodeLength)
            key.vsHash = HashBytecode(desc.VS.pShaderBytecode, desc.VS.BytecodeLength);
        if (desc.PS.pShaderBytecode && desc.PS.BytecodeLength)
            key.psHash = HashBytecode(desc.PS.pShaderBytecode, desc.PS.BytecodeLength);
        if (desc.GS.pShaderBytecode && desc.GS.BytecodeLength)
            key.gsHash = HashBytecode(desc.GS.pShaderBytecode, desc.GS.BytecodeLength);
        if (desc.HS.pShaderBytecode && desc.HS.BytecodeLength)
            key.hsHash = HashBytecode(desc.HS.pShaderBytecode, desc.HS.BytecodeLength);
        if (desc.DS.pShaderBytecode && desc.DS.BytecodeLength)
            key.dsHash = HashBytecode(desc.DS.pShaderBytecode, desc.DS.BytecodeLength);

        // Root signature
        key.rootSignature = desc.pRootSignature;

        // Input layout
        if (desc.InputLayout.pInputElementDescs && desc.InputLayout.NumElements)
        {
            key.inputLayoutHash = HashInputLayout(
                desc.InputLayout.pInputElementDescs,
                desc.InputLayout.NumElements);
        }

        // Rasterizer state (all fields)
        key.fillMode = desc.RasterizerState.FillMode;
        key.cullMode = desc.RasterizerState.CullMode;
        key.frontCounterClockwise = desc.RasterizerState.FrontCounterClockwise;
        key.depthBias = desc.RasterizerState.DepthBias;
        key.depthBiasClamp = desc.RasterizerState.DepthBiasClamp;
        key.slopeScaledDepthBias = desc.RasterizerState.SlopeScaledDepthBias;
        key.depthClipEnable = desc.RasterizerState.DepthClipEnable;
        key.multisampleEnable = desc.RasterizerState.MultisampleEnable;
        key.antialiasedLineEnable = desc.RasterizerState.AntialiasedLineEnable;
        key.forcedSampleCount = desc.RasterizerState.ForcedSampleCount;
        key.conservativeRaster = desc.RasterizerState.ConservativeRaster;

        // Depth stencil state (all fields)
        key.depthEnable = desc.DepthStencilState.DepthEnable;
        key.depthWriteMask = desc.DepthStencilState.DepthWriteMask;
        key.depthFunc = desc.DepthStencilState.DepthFunc;
        key.stencilEnable = desc.DepthStencilState.StencilEnable;
        key.stencilReadMask = desc.DepthStencilState.StencilReadMask;
        key.stencilWriteMask = desc.DepthStencilState.StencilWriteMask;
        key.frontFace = desc.DepthStencilState.FrontFace;
        key.backFace = desc.DepthStencilState.BackFace;

        // Blend state (all fields)
        key.alphaToCoverageEnable = desc.BlendState.AlphaToCoverageEnable;
        key.independentBlendEnable = desc.BlendState.IndependentBlendEnable;
        for (UINT i = 0; i < 8; ++i)
            key.renderTargetBlend[i] = desc.BlendState.RenderTarget[i];

        // Output merger
        key.numRenderTargets = desc.NumRenderTargets;
        for (UINT i = 0; i < 8; ++i)
            key.rtvFormats[i] = desc.RTVFormats[i];
        key.dsvFormat = desc.DSVFormat;

        // Sample desc
        key.sampleCount = desc.SampleDesc.Count;
        key.sampleQuality = desc.SampleDesc.Quality;
        key.sampleMask = desc.SampleMask;

        // Misc
        key.primitiveTopologyType = desc.PrimitiveTopologyType;
        key.ibStripCutValue = desc.IBStripCutValue;

        return key;
    }

    ID3D12PipelineState* PSOCache::GetOrCreate(
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
        const char* tag)
    {
        if (!m_device)
        {
            OutputDebugStringA("[PSOCache] ERROR: not initialized\n");
            return nullptr;
        }

        PSOKey key = BuildKey(desc);

        // Check cache
        auto it = m_cache.find(key);
        if (it != m_cache.end())
        {
            ++m_hits;
            return it->second.Get();
        }

        // Cache miss - create new PSO
        ++m_misses;

        // Check capacity
        if (m_cache.size() >= m_maxEntries)
        {
            char buf[128];
            sprintf_s(buf, "[PSOCache] WARNING: capacity reached (%u entries)\n", m_maxEntries);
            OutputDebugStringA(buf);
        }

        ComPtr<ID3D12PipelineState> pso;
        HRESULT hr = m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
        if (FAILED(hr))
        {
            char buf[128];
            sprintf_s(buf, "[PSOCache] ERROR: CreateGraphicsPipelineState failed (0x%08X) tag=%s\n",
                      hr, tag ? tag : "?");
            OutputDebugStringA(buf);
#if defined(_DEBUG)
            __debugbreak();
#endif
            return nullptr;
        }

        // Log cache miss
        {
            const char* cullStr = "?";
            switch (key.cullMode)
            {
            case D3D12_CULL_MODE_NONE: cullStr = "NONE"; break;
            case D3D12_CULL_MODE_FRONT: cullStr = "FRONT"; break;
            case D3D12_CULL_MODE_BACK: cullStr = "BACK"; break;
            }

            char buf[256];
            sprintf_s(buf, "[PSOCache] MISS: tag=\"%s\" vs=0x%08llX ps=0x%08llX cull=%s depth=%s\n",
                      tag ? tag : "?",
                      key.vsHash & 0xFFFFFFFF,
                      key.psHash & 0xFFFFFFFF,
                      cullStr,
                      key.depthEnable ? "ON" : "OFF");
            OutputDebugStringA(buf);
        }

        // Store in cache
        ID3D12PipelineState* rawPtr = pso.Get();
        m_cache[key] = std::move(pso);

        return rawPtr;
    }

    bool PSOCache::PreWarm(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const char* tag)
    {
        ID3D12PipelineState* pso = GetOrCreate(desc, tag);
        return pso != nullptr;
    }

    void PSOCache::LogStats() const
    {
        uint64_t total = m_hits + m_misses;
        double hitRate = (total > 0) ? (static_cast<double>(m_hits) / total * 100.0) : 0.0;

        char buf[256];
        sprintf_s(buf, "[PSOCache] Stats: %u entries, %llu hits, %llu misses (%.1f%% hit rate)\n",
                  static_cast<uint32_t>(m_cache.size()),
                  m_hits, m_misses, hitRate);
        OutputDebugStringA(buf);
    }
}
