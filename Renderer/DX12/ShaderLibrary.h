#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include "PSOCache.h"

namespace Renderer
{
    // Root parameter indices - this is the CPU/GPU ABI contract
    // MUST match shader register assignments (b0, t0, b1, b2)
    enum RootParam : uint32_t
    {
        RP_FrameCB = 0,          // b0 space0 - Frame constants (ViewProj)
        RP_TransformsTable = 1,  // t0 space0 - Transforms SRV descriptor table
        RP_InstanceOffset = 2,   // b1 space0 - Instance offset (1 DWORD root constant)
        RP_DebugCB = 3,          // b2 space0 - Debug constants (ColorMode, 4 DWORDs)
        RP_Count
    };

    class ShaderLibrary
    {
    public:
        ShaderLibrary() = default;
        ~ShaderLibrary() = default;

        ShaderLibrary(const ShaderLibrary&) = delete;
        ShaderLibrary& operator=(const ShaderLibrary&) = delete;

        // Initialize shaders, root signature, and PSO
        bool Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat);

        // Shutdown and release resources
        void Shutdown();

        // Accessors
        ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }
        ID3D12PipelineState* GetPSO() const { return m_pso; }
        ID3D12PipelineState* GetCubesOpaquePSO() const { return m_cubesOpaquePso; }  // Task B
        ID3D12PipelineState* GetFloorPSO() const { return m_floorPso; }
        ID3D12PipelineState* GetMarkerPSO() const { return m_markerPso; }
        ID3D12RootSignature* GetMarkerRootSignature() const { return m_markerRootSignature.Get(); }

        // Log PSO cache statistics
        void LogPSOCacheStats() const { m_psoCache.LogStats(); }

    private:
        bool CreateRootSignature(ID3D12Device* device);
        bool LoadShaders();
        bool LoadCompiledShader(const wchar_t* path, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob);
        bool CreatePSO(ID3D12Device* device, DXGI_FORMAT rtvFormat);

        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

        // PSO cache (owns all PSOs - raw pointers below are non-owning)
        mutable PSOCache m_psoCache;

        // Non-owning PSO pointers (owned by m_psoCache)
        ID3D12PipelineState* m_pso = nullptr;
        ID3D12PipelineState* m_cubesOpaquePso = nullptr;  // Task B: Explicit opaque for sanity test
        ID3D12PipelineState* m_floorPso = nullptr;
        ID3D12PipelineState* m_markerPso = nullptr;

        // Shader bytecode blobs (needed for PSO cache key hashing)
        Microsoft::WRL::ComPtr<ID3DBlob> m_vsBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> m_psBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> m_floorVsBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> m_floorPsBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> m_markerVsBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> m_markerPsBlob;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_markerRootSignature;
    };
}
